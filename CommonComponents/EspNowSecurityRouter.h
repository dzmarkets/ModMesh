#ifndef ESPNOW_SECURITY_ROUTER_H
#define ESPNOW_SECURITY_ROUTER_H

#include <stdint.h>
#include <stddef.h>
#include <functional>
#include "esp_now.h"

// Callback type for when a complete (reassembled) Modbus frame is received over ESP-NOW
// src_mac: MAC address of the sender
// payload: The complete Modbus RTU frame
// len: Length of the Modbus frame
typedef std::function<void(const uint8_t* src_mac, const uint8_t* payload, size_t len)> EspNowFrameReceivedCb;

class EspNowSecurityRouter {
public:
    EspNowSecurityRouter();
    ~EspNowSecurityRouter();

    // Initialize Wi-Fi (Station mode) and ESP-NOW
    // Pinned to Core 0 to handle Wi-Fi events efficiently
    bool begin(EspNowFrameReceivedCb rx_callback);

    // Add a peer with AES-128 CCMP encryption using LMK
    bool addEncryptedPeer(const uint8_t* mac_addr);

    // Send a payload to a specific MAC. Handles fragmentation if len > 246 bytes.
    bool send(const uint8_t* dest_mac, const uint8_t* payload, size_t len);

private:
    static void onDataReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
    static void onDataSend(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);

    void handleReceivedData(const uint8_t* src_mac, const uint8_t* data, int data_len);

    EspNowFrameReceivedCb m_rx_callback;
    static EspNowSecurityRouter* s_instance; // Singleton-like access for C callbacks

    // State for reassembly (assuming only one incoming fragmented packet at a time for simplicity)
    uint8_t m_reassembly_buf[256];
    size_t m_reassembly_len;
    uint8_t m_expected_frag_index;
    uint8_t m_current_msg_id;
    uint8_t m_out_msg_id; // For sending
};

#endif // ESPNOW_SECURITY_ROUTER_H
