//
// File Path: ESP-NOW-MeshCore/components/ws2812_driver/include/ws2812_driver.h
// Brief:     Header file for the custom WS2812 driver component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.1.0
// CreateDate: 2026-05-05
// UpdateDate: 2026-05-05
//

#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* ws2812_handle_t;

/**
 * @brief Initialize WS2812 driver
 * 
 * @param gpio_num GPIO number for LED strip
 * @param max_leds Maximum number of LEDs in the strip
 * @param out_handle Pointer to store the driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ws2812_init(int gpio_num, uint32_t max_leds, ws2812_handle_t *out_handle);

/**
 * @brief Set pixel colour
 * 
 * @param handle Driver handle
 * @param index Pixel index
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ws2812_set_pixel(ws2812_handle_t handle, uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Refresh LED strip (send data to hardware)
 * 
 * @param handle Driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ws2812_refresh(ws2812_handle_t handle);

/**
 * @brief Clear LED strip (turn off all LEDs)
 * 
 * @param handle Driver handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ws2812_clear(ws2812_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // WS2812_DRIVER_H
