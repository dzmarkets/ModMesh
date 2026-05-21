#include "EspNowSecurityRouter.h"
#include "SharedConfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "EspNowSecurityRouter";

#pragma pack(push, 1)
struct ModbusEspNowFrag {
    uint8_t msg_id;
    uint8_t frag_index;
    uint8_t total_frags;
    uint8_t payload_len;
    uint8_t payload[246]; // max payload size to keep total struct <= 250
};
#pragma pack(pop)

EspNowSecurityRouter* EspNowSecurityRouter::s_instance = nullptr;

EspNowSecurityRouter::EspNowSecurityRouter() : 
    m_rx_callback(nullptr), 
    m_reassembly_len(0), 
    m_expected_frag_index(0), 
    m_current_msg_id(0), 
    m_out_msg_id(0) {
    s_instance = this;
}

EspNowSecurityRouter::~EspNowSecurityRouter() {
    s_instance = nullptr;
}

bool EspNowSecurityRouter::begin(EspNowFrameReceivedCb rx_callback) {
    m_rx_callback = rx_callback;

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

    // Set Primary Master Key for ESP-NOW security
    ESP_ERROR_CHECK(esp_now_set_pmk(ESPNOW_PMK));

    esp_now_register_recv_cb(onDataReceive);
    esp_now_register_send_cb(onDataSend);

    ESP_LOGI(TAG, "ESP-NOW Initialized with Security on Channel %d", ESPNOW_WIFI_CHANNEL);
    return true;
}

bool EspNowSecurityRouter::addEncryptedPeer(const uint8_t* mac_addr) {
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

bool EspNowSecurityRouter::send(const uint8_t* dest_mac, const uint8_t* payload, size_t len) {
    if (len == 0) return false;

    m_out_msg_id++;
    
    size_t offset = 0;
    uint8_t frag_index = 0;
    uint8_t total_frags = (len + 245) / 246; // ceiling division
    
    while (offset < len) {
        ModbusEspNowFrag frag;
        frag.msg_id = m_out_msg_id;
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

void EspNowSecurityRouter::onDataSend(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    // Only logging errors to prevent spam, could use a queue for confirmation
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGE(TAG, "Send to " MACSTR " failed", MAC2STR(tx_info->des_addr));
    }
}

void EspNowSecurityRouter::onDataReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (s_instance) {
        s_instance->handleReceivedData(esp_now_info->src_addr, data, data_len);
    }
}

void EspNowSecurityRouter::handleReceivedData(const uint8_t* src_mac, const uint8_t* data, int data_len) {
    if (data_len < 4) return; // Header size
    
    const ModbusEspNowFrag* frag = (const ModbusEspNowFrag*)data;
    
    // New message started
    if (frag->frag_index == 0) {
        m_current_msg_id = frag->msg_id;
        m_expected_frag_index = 0;
        m_reassembly_len = 0;
    }
    
    // Validate sequence
    if (frag->msg_id == m_current_msg_id && frag->frag_index == m_expected_frag_index) {
        if (m_reassembly_len + frag->payload_len <= 256) {
            memcpy(m_reassembly_buf + m_reassembly_len, frag->payload, frag->payload_len);
            m_reassembly_len += frag->payload_len;
            m_expected_frag_index++;
            
            // Check if complete
            if (m_expected_frag_index == frag->total_frags) {
                if (m_rx_callback) {
                    m_rx_callback(src_mac, m_reassembly_buf, m_reassembly_len);
                }
                m_reassembly_len = 0; // Reset
            }
        } else {
            ESP_LOGE(TAG, "Reassembly buffer overflow");
            m_reassembly_len = 0;
        }
    } else {
        ESP_LOGW(TAG, "Fragment out of order or wrong msg_id. Expected %d, got %d", m_expected_frag_index, frag->frag_index);
        m_reassembly_len = 0;
    }
}
