//
// File Path: ESP-NOW-MeshCore/components/status_indicator/include/status_indicator.h
// Brief:     Header file for status_indicator component (RGB LED driver).
//            States: Red = Disconnected, Green = Connected.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.4.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-08
//

#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STATE_IDLE = 0,         /**< GREEN (Dim) – Ready */
    LED_STATE_MODBUS_RX,        /**< BLUE – Received from PLC */
    LED_STATE_ESPNOW_TX,        /**< YELLOW – Transmitting to Slave */
    LED_STATE_ESPNOW_RX,        /**< CYAN – Received from Slave */
    LED_STATE_MODBUS_TX,        /**< WHITE – Transmitting to PLC */
    LED_STATE_WARNING,          /**< ORANGE – Button held / Warning */
    LED_STATE_DIAGNOSTIC,       /**< PURPLE – Diagnostic mode */
    LED_STATE_ERROR,            /**< RED – Critical Error */
    LED_STATE_OFF,              /**< ALL OFF */
} led_state_t;

/**
 * @brief Configure the RGB LED GPIO pins and set initial state (disconnected/red).
 *        GPIO pin numbers are read from shared_config.h (RGB_LED_*_GPIO).
 */
void status_indicator_configure(void);

/**
 * @brief Set the RGB LED to a specific logical state.
 * @param state One of LED_STATE_DISCONNECTED, LED_STATE_CONNECTED, LED_STATE_SENDING, etc.
 */
void status_indicator_set_state(led_state_t state);

/**
 * @brief Get the current LED logical state.
 * @return Current led_state_t value.
 */
led_state_t status_indicator_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_INDICATOR_H
