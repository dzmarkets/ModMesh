//
// File Path: ESP-NOW-MeshCore/components/message_processor/message_processor.c
// Brief:     Source file for message_processor component.
//            Handles decryption, deduplication, and application logic.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#include "message_processor.h"
#include "mesh_types.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include "espnow_control.h"
#include "security_manager.h"
#include "actuators.h"
#include "mesh_manager.h"

static const char *TAG = "msg_proc";

// ---------------------------------------------------------------------------
// Flooding deduplication cache
// ---------------------------------------------------------------------------
#define CACHE_SIZE 128
static uint32_t s_msg_cache[CACHE_SIZE] = {0};
static uint8_t  s_cache_index = 0;

// djb2 hash – fast, good distribution for short strings
static uint32_t calculate_hash(const uint8_t *data, int len)
{
    uint32_t hash = 5381;
    for (int i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

static bool is_message_seen(uint32_t hash)
{
    if (hash == 0) return false;
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (s_msg_cache[i] == hash) return true;
    }
    return false;
}

static void add_to_cache(uint32_t hash)
{
    s_msg_cache[s_cache_index] = hash;
    s_cache_index = (s_cache_index + 1) % CACHE_SIZE;
}

// ---------------------------------------------------------------------------
// ACK tracking
// ---------------------------------------------------------------------------
static volatile bool s_ack_received_flag = false;

// Local MAC (populated in message_processor_init)
static uint8_t s_local_mac[6] = {0};

// Broadcast MAC constant
static const uint8_t k_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---------------------------------------------------------------------------
// Internal: build and send an ACK back to the original sender
// ---------------------------------------------------------------------------
static void send_ack(const uint8_t *dest_mac)
{
    // ACK frame: [0x02][6-byte local MAC]
    uint8_t ack_frame[7];
    ack_frame[0] = (uint8_t)MSG_TYPE_ACK;
    memcpy(&ack_frame[1], s_local_mac, 6);

    // Ensure the destination is a registered peer before sending
    espnow_control_add_peer(dest_mac);

    esp_err_t err = espnow_control_send(dest_mac, ack_frame, sizeof(ack_frame));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ACK sent to " MACSTR, MAC2STR(dest_mac));
    } else {
        ESP_LOGW(TAG, "ACK send to " MACSTR " failed: %s",
                 MAC2STR(dest_mac), esp_err_to_name(err));
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void message_processor_init(void)
{
    esp_read_mac(s_local_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Message processor ready. Local MAC: " MACSTR,
             MAC2STR(s_local_mac));
}

void message_processor_handle_received(const uint8_t *src_mac,
                                       const uint8_t *data, int data_len)
{
    if (src_mac == NULL || data == NULL || data_len < 1) return;

    msg_type_t msg_type = (msg_type_t)data[0];

    // ----------------------------------------------------------------
    // ACK packet path
    // ----------------------------------------------------------------
    if (msg_type == MSG_TYPE_ACK) {
        if (data_len >= 7) {
            uint8_t ack_mac[6];
            memcpy(ack_mac, &data[1], 6);
            ESP_LOGI(TAG, "ACK received from " MACSTR " (via " MACSTR ")",
                     MAC2STR(ack_mac), MAC2STR(src_mac));
        } else {
            ESP_LOGI(TAG, "ACK received from " MACSTR, MAC2STR(src_mac));
        }
        s_ack_received_flag = true;
        
        // Register/Update this peer in the mesh manager
        mesh_manager_update_peer_activity(src_mac, NULL);
        espnow_control_add_peer(src_mac);
        return;
    }

    // ----------------------------------------------------------------
    // DATA packet path
    // ----------------------------------------------------------------
    if (msg_type != MSG_TYPE_DATA) {
        ESP_LOGW(TAG, "Unknown msg_type 0x%02X – discarding", msg_type);
        return;
    }

    // The actual encrypted payload starts at byte 1
    const uint8_t *enc_data     = &data[1];
    int            enc_data_len = data_len - 1;

    ESP_LOGI(TAG, "DATA packet from " MACSTR " (%d bytes encrypted)",
             MAC2STR(src_mac), enc_data_len);

    // --- Decrypt ---
    char plaintext[256] = {0};
    if (!security_manager_decrypt(enc_data, enc_data_len, plaintext, sizeof(plaintext))) {
        ESP_LOGE(TAG, "Decryption failed – packet dropped");
        return;
    }

    // Parse name and original MAC from plaintext: "[NAME | MAC | SEQ] ..."
    char peer_name[32] = {0};
    uint8_t orig_mac[6] = {0};
    bool mac_parsed = false;
    
    const char *name_start = strchr(plaintext, '[');
    const char *sep1       = strchr(plaintext, '|'); // separator between name and MAC
    const char *mac_end    = strchr(plaintext, ']');
    
    if (name_start && sep1 && mac_end && sep1 > name_start && mac_end > sep1) {
        // 1. Extract Name
        size_t name_len = sep1 - name_start - 1;
        if (name_len > 31) name_len = 31;
        strncpy(peer_name, name_start + 1, name_len);
        peer_name[name_len] = '\0';
        if (name_len > 0 && peer_name[name_len-1] == ' ') peer_name[name_len-1] = '\0';
        
        // 2. Extract MAC
        unsigned int m[6];
        int parsed = sscanf(sep1 + 1, " %02X:%02X:%02X:%02X:%02X:%02X",
                            &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
        if (parsed == 6) {
            for(int i=0; i<6; i++) orig_mac[i] = (uint8_t)m[i];
            mac_parsed = true;
        }
    }

    const uint8_t *tracking_mac = mac_parsed ? orig_mac : src_mac;
    mesh_manager_update_peer_activity(tracking_mac, peer_name);
    
    if (mac_parsed) espnow_control_add_peer(orig_mac);
    espnow_control_add_peer(src_mac);

    // --- Send ACK back to the immediate sender ---
    // We do this BEFORE deduplication so the sender knows we heard them,
    // even if we've already seen this exact message content before.
    send_ack(src_mac);

    // --- Deduplication (flood cache) ---
    // Hash the *plaintext* because the IV makes every ciphertext unique.
    uint32_t msg_hash = calculate_hash((const uint8_t *)plaintext, strlen(plaintext));
    if (is_message_seen(msg_hash)) {
        ESP_LOGI(TAG, "Duplicate message from " MACSTR " – dropped (flood protection)",
                 MAC2STR(src_mac));
        return;
    }
    add_to_cache(msg_hash);

    // --- Application layer ---
    ESP_LOGI(TAG, "Payload from %s (" MACSTR "): %s", 
             strlen(peer_name) > 0 ? peer_name : "UNKNOWN",
             MAC2STR(src_mac), plaintext);
             
#if DEVICE_ROLE == ROLE_ACTUATOR || DEVICE_ROLE == ROLE_BOTH
    actuators_execute(plaintext);
#endif

    // --- Rebroadcast the original DATA frame (multi-hop flood) ---
    // Every unique packet is rebroadcasted exactly once.
    // Unique sequence numbers in the payload ensure that even identical 
    // heartbeats are repeated, allowing the mesh to be truly dynamic.
    espnow_control_set_ap_mode();
    
    esp_err_t err = espnow_control_send(k_broadcast_mac, data, data_len);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Message rebroadcast to mesh (flooding)");
    } else {
        ESP_LOGW(TAG, "Rebroadcast failed: %s", esp_err_to_name(err));
    }
    espnow_control_set_sta_mode();
}

bool message_processor_ack_received(void)
{
    return s_ack_received_flag;
}

void message_processor_clear_ack_flag(void)
{
    s_ack_received_flag = false;
}
