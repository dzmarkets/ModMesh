#include "espnow_control.h"
#include "shared_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "espnow_control";

#pragma pack(push, 1)
struct ModbusEspNowFrag {
    uint8_t msg_id;
    uint8_t frag_index;
    uint8_t total_frags;
    uint8_t payload_len;
    uint8_t payload[246]; // max payload size to keep total struct <= 250
};
#pragma pack(pop)

static espnow_recv_cb_t s_rx_callback = NULL;
static uint8_t s_reassembly_buf[256];
static size_t s_reassembly_len = 0;
static uint8_t s_expected_frag_index = 0;
static uint8_t s_current_msg_id = 0;
static uint8_t s_out_msg_id = 0;

static void onDataReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len < 4) return; // Header size
    
    const ModbusEspNowFrag* frag = (const ModbusEspNowFrag*)data;
    
    // New message started
    if (frag->frag_index == 0) {
        s_current_msg_id = frag->msg_id;
        s_expected_frag_index = 0;
        s_reassembly_len = 0;
    }
    
    // Validate sequence
    if (frag->msg_id == s_current_msg_id && frag->frag_index == s_expected_frag_index) {
        if (s_reassembly_len + frag->payload_len <= 256) {
            memcpy(s_reassembly_buf + s_reassembly_len, frag->payload, frag->payload_len);
            s_reassembly_len += frag->payload_len;
            s_expected_frag_index++;
            
            // Check if complete
            if (s_expected_frag_index == frag->total_frags) {
                if (s_rx_callback) {
                    s_rx_callback(esp_now_info->src_addr, s_reassembly_buf, s_reassembly_len);
                }
                s_reassembly_len = 0; // Reset
            }
        } else {
            ESP_LOGE(TAG, "Reassembly buffer overflow");
            s_reassembly_len = 0;
        }
    } else {
        ESP_LOGW(TAG, "Fragment out of order or wrong msg_id.");
        s_reassembly_len = 0;
    }
}

static void onDataSend(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGE(TAG, "Send to " MACSTR " failed", MAC2STR(tx_info->des_addr));
    }
}

bool espnow_control_init(espnow_recv_cb_t rx_cb) {
    s_rx_callback = rx_cb;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return false;
    }

    ESP_ERROR_CHECK(esp_now_set_pmk(ESPNOW_PMK));

    esp_now_register_recv_cb(onDataReceive);
    esp_now_register_send_cb(onDataSend);

    ESP_LOGI(TAG, "ESP-NOW Initialized with Security on Channel %d", ESPNOW_WIFI_CHANNEL);
    return true;
}

bool espnow_control_add_peer(const uint8_t* mac_addr) {
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac_addr, 6);
    peer_info.channel = ESPNOW_WIFI_CHANNEL;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = true;
    memcpy(peer_info.lmk, ESPNOW_LMK, 16);

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add encrypted peer");
        return false;
    }
    ESP_LOGI(TAG, "Added encrypted peer: " MACSTR, MAC2STR(mac_addr));
    return true;
}

bool espnow_control_send(const uint8_t* dest_mac, const uint8_t* payload, size_t len) {
    if (len == 0) return false;

    s_out_msg_id++;
    
    size_t offset = 0;
    uint8_t frag_index = 0;
    uint8_t total_frags = (len + 245) / 246; // ceiling division
    
    while (offset < len) {
        ModbusEspNowFrag frag;
        frag.msg_id = s_out_msg_id;
        frag.frag_index = frag_index;
        frag.total_frags = total_frags;
        
        size_t chunk_size = len - offset;
        if (chunk_size > 246) {
            chunk_size = 246;
        }
        
        frag.payload_len = chunk_size;
        memcpy(frag.payload, payload + offset, chunk_size);
        
        esp_err_t result = esp_now_send(dest_mac, (const uint8_t *)&frag, sizeof(ModbusEspNowFrag) - 246 + chunk_size);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error sending fragment %d", frag_index);
            return false;
        }
        
        offset += chunk_size;
        frag_index++;
    }
    return true;
}
