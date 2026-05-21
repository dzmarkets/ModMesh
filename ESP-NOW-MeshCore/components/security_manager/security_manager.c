//
// File Path: ESP-NOW-MeshCore/components/security_manager/security_manager.c
// Brief:     Source file for security_manager component.
//            Implements AES-128-CBC encryption/decryption using mbedtls.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#include "security_manager.h"
#include "shared_config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/aes.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "security_mgr";

const char* security_manager_get_key(void)
{
    return NETWORK_API_KEY;
}

// Convert 32-char hex string to 16-byte array
static void get_aes_key(uint8_t *key_bin) {
    const char *hex = NETWORK_API_KEY;
    for (int i = 0; i < 16; i++) {
        sscanf(hex + 2*i, "%2hhx", &key_bin[i]);
    }
}

size_t security_manager_encrypt(const char *plaintext, uint8_t *out_buffer, size_t max_len)
{
    if (!plaintext || !out_buffer) return 0;
    
    size_t plaintext_len = strlen(plaintext);
    
    // PKCS#7 Padding calculation
    size_t padding_len = 16 - (plaintext_len % 16);
    size_t padded_len = plaintext_len + padding_len;
    
    // Total size will be 16 bytes IV + padded ciphertext
    size_t total_out_len = 16 + padded_len;
    if (total_out_len > max_len) {
        ESP_LOGE(TAG, "Buffer too small for encryption!");
        return 0;
    }
    
    uint8_t key[16];
    get_aes_key(key);
    
    // Generate Random IV into the first 16 bytes of out_buffer
    esp_fill_random(out_buffer, 16);
    
    // We need a local copy of IV because mbedtls_aes_crypt_cbc modifies it
    uint8_t iv_copy[16];
    memcpy(iv_copy, out_buffer, 16);
    
    // Prepare padded plaintext buffer
    uint8_t padded_pt[256]; // ESP-NOW max payload is 250, so 256 is safe
    memcpy(padded_pt, plaintext, plaintext_len);
    for (size_t i = 0; i < padding_len; i++) {
        padded_pt[plaintext_len + i] = (uint8_t)padding_len;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    // Encrypt into the out_buffer (starting at offset 16, after the IV)
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv_copy, padded_pt, out_buffer + 16);
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Encryption failed with error %d", ret);
        return 0;
    }
    
    return total_out_len;
}

bool security_manager_decrypt(const uint8_t *payload, int length, char *out_plaintext, size_t max_len)
{
    if (!payload || !out_plaintext || length < 32 || (length % 16) != 0) {
        ESP_LOGE(TAG, "Invalid payload length for AES-128-CBC");
        return false;
    }
    
    size_t ciphertext_len = length - 16;
    if (ciphertext_len > max_len) {
        ESP_LOGE(TAG, "Output buffer too small");
        return false;
    }
    
    uint8_t key[16];
    get_aes_key(key);
    
    // Extract IV from first 16 bytes
    uint8_t iv[16];
    memcpy(iv, payload, 16);
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 128);
    
    // Decrypt the ciphertext (starting after the IV)
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, iv, payload + 16, (uint8_t*)out_plaintext);
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Decryption failed (Invalid Key or Corrupted Data)");
        return false;
    }
    
    // Validate PKCS#7 padding
    uint8_t pad_val = out_plaintext[ciphertext_len - 1];
    if (pad_val > 16 || pad_val == 0) {
        ESP_LOGE(TAG, "Invalid PKCS#7 Padding (Wrong Key or Hacker attempt)");
        return false;
    }
    
    for (int i = 0; i < pad_val; i++) {
        if (out_plaintext[ciphertext_len - 1 - i] != pad_val) {
            ESP_LOGE(TAG, "Padding bytes mismatch (Wrong Key or Hacker attempt)");
            return false;
        }
    }
    
    // Null-terminate the string by overwriting the first padding byte
    out_plaintext[ciphertext_len - pad_val] = '\0';
    
    return true;
}
