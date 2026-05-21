//
// File Path: ESP-NOW-MeshCore/components/espnow_control/espnow_control.c
// Brief:     Source file for espnow_control component.
//            Manages WiFi initialisation, ESP-NOW registration, and peer tracking.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-25
// UpdateDate: 2026-05-05
//

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "espnow_control.h"
#include "shared_config.h"

static const char *TAG = "ESPNOW_CTRL";

// Store the user callbacks
static espnow_recv_cb_t g_user_recv_cb = NULL;
static espnow_send_cb_t g_user_send_cb = NULL;

// Track how many unique peers we have registered
static int s_peer_count = 0;

// Track current AP mode state
static bool s_is_ap_mode = false;

// ---------------------------------------------------------------------------
// Internal ESP-NOW callbacks (forward to user callbacks)
// ---------------------------------------------------------------------------

static void espnow_internal_recv_cb(const esp_now_recv_info_t *esp_now_info,
                                    const uint8_t *data, int data_len)
{
    if (g_user_recv_cb && esp_now_info && esp_now_info->src_addr) {
        g_user_recv_cb(esp_now_info->src_addr, data, data_len);
    }
}

static void espnow_internal_send_cb(const uint8_t *mac_addr,
                                    esp_now_send_status_t status)
{
    if (g_user_send_cb && mac_addr) {
        g_user_send_cb(mac_addr, status);
    }
}

// ---------------------------------------------------------------------------
// Public API – Initialisation
// ---------------------------------------------------------------------------

esp_err_t espnow_control_init(espnow_recv_cb_t recv_cb, espnow_send_cb_t send_cb)
{
    g_user_recv_cb = recv_cb;
    g_user_send_cb = send_cb;

    // Initialise NVS (required by WiFi driver)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialise the TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create the default netif objects for STA (AP removed to save power/avoid SSID)
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Start in STA-only mode (default receiver role)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Lock the channel so all devices can hear each other
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    s_is_ap_mode  = false;
    s_peer_count  = 0;

    // Initialise ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initialising ESP-NOW: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_internal_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_internal_send_cb));

    ESP_LOGI(TAG, "ESP-NOW initialised on channel %d (STA/receiver mode)",
             ESPNOW_WIFI_CHANNEL);
    return ESP_OK;
}

esp_err_t espnow_control_deinit(void)
{
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    esp_wifi_stop();
    s_peer_count = 0;
    s_is_ap_mode = false;
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API – Peer management
// ---------------------------------------------------------------------------

esp_err_t espnow_control_add_peer(const uint8_t *peer_mac)
{
    if (peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_now_is_peer_exist(peer_mac)) {
        ESP_LOGD(TAG, "Peer " MACSTR " already registered", MAC2STR(peer_mac));
        return ESP_OK;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = ESPNOW_WIFI_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx   = WIFI_IF_STA;

    esp_err_t ret = esp_now_add_peer(&peerInfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add peer " MACSTR ": %s",
                 MAC2STR(peer_mac), esp_err_to_name(ret));
    } else {
        s_peer_count++;
        ESP_LOGI(TAG, "Registered peer " MACSTR " (total peers: %d)",
                 MAC2STR(peer_mac), s_peer_count);
    }
    return ret;
}

int espnow_control_get_peer_count(void)
{
    return s_peer_count;
}

// ---------------------------------------------------------------------------
// Public API – Mode switching
// ---------------------------------------------------------------------------

void espnow_control_set_ap_mode(void)
{
    // NO-OP: We now stay in STA mode permanently to avoid radio dropouts.
    // This maintains the function signature for main.c compatibility.
    s_is_ap_mode = true; 
}

void espnow_control_set_sta_mode(void)
{
    // NO-OP: We now stay in STA mode permanently.
    s_is_ap_mode = false;
}

bool espnow_control_is_ap_mode(void)
{
    return s_is_ap_mode;
}

// ---------------------------------------------------------------------------
// Public API – Send
// ---------------------------------------------------------------------------

esp_err_t espnow_control_send(const uint8_t *peer_mac,
                               const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    // peer_mac == NULL → send to all registered peers
    return esp_now_send(peer_mac, data, len);
}
