//
// File Path: ESP-NOW-MeshCore/components/message_provider/message_provider.c
// Brief:     Source file for message_provider component.
//            Builds the outgoing ESP-NOW DATA packet with unique sequence numbers.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#include "message_provider.h"
#include "mesh_types.h"         // for MSG_TYPE_DATA
#include <stdio.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_random.h"
#include "security_manager.h"

static char   s_device_name[32] = "UNKNOWN";
static char   s_custom_msg[64]  = "Message One";
static uint8_t s_mac_addr[6]   = {0};

void message_provider_init(const char *device_name)
{
    if (device_name) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }
    // Cache the Wi-Fi STA MAC address
    esp_read_mac(s_mac_addr, ESP_MAC_WIFI_STA);
}

void message_provider_set_message(const char *msg)
{
    if (msg) {
        strncpy(s_custom_msg, msg, sizeof(s_custom_msg) - 1);
        s_custom_msg[sizeof(s_custom_msg) - 1] = '\0';
    }
}

size_t message_provider_get_next(char *buffer, size_t max_len)
{
    if (!buffer || max_len < 2) return 0;

    // Prepend the MSG_TYPE_DATA byte
    buffer[0] = (uint8_t)MSG_TYPE_DATA;

    // Build plaintext payload: "[DEVICE | MAC | SEQ] sensor_data"
    // Initialize sequence number with a random value so it is unique across reboots
    static uint32_t s_msg_seq = 0;
    static bool seq_init = false;
    if (!seq_init) {
        s_msg_seq = esp_random();
        seq_init = true;
    }
    
    char plaintext[128];
    snprintf(plaintext, sizeof(plaintext),
             "[%s | %02X:%02X:%02X:%02X:%02X:%02X | %lu] %s",
             s_device_name,
             s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
             s_mac_addr[3], s_mac_addr[4], s_mac_addr[5],
             (unsigned long)s_msg_seq++,
             s_custom_msg);

    // Encrypt into buffer starting at offset 1 (after the type byte)
    size_t encrypted_len = security_manager_encrypt(plaintext,
                                                     (uint8_t *)&buffer[1],
                                                     max_len - 1);
    if (encrypted_len == 0) return 0;

    return 1 + encrypted_len; // type byte + ciphertext
}
