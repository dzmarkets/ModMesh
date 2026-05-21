//
// File Path: ESP-NOW-MeshCore/components/status_indicator/status_indicator.c
// Brief:     Source file for status_indicator component (RGB LED driver).
//            Drives a common-cathode 3-pin RGB LED or WS2812B addressable LED.
//            GPIO pin numbers are centralised in shared_config.h.
//
//            Colour mapping:
//              RED   (LED_STATE_DISCONNECTED) – no peers, mesh offline.
//              GREEN (LED_STATE_CONNECTED)    – at least one peer reachable.
//              BLUE  (LED_STATE_SENDING)      – transmitting sensor data / waiting for ACK.
//
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.4.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-08
//

#include "status_indicator.h"
#include "shared_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#if !USE_EXTERNAL_RGB_LED
#include "ws2812_driver.h"
static ws2812_handle_t s_led_strip = NULL;
#endif

#ifndef RGB_LED_RED_GPIO
#define RGB_LED_RED_GPIO    4
#endif

#ifndef RGB_LED_GREEN_GPIO
#define RGB_LED_GREEN_GPIO  5
#endif

#ifndef RGB_LED_BLUE_GPIO
#define RGB_LED_BLUE_GPIO   6
#endif

static const char *TAG = "status_ind";

// Track the current logical state
static led_state_t s_current_state = LED_STATE_OFF;
static esp_timer_handle_t s_return_timer = NULL;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Drive all three colour channels to a specific RGB value (0-255). */
static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
#if USE_EXTERNAL_RGB_LED
    gpio_set_level(RGB_LED_RED_GPIO,   r > 0 ? 1 : 0);
    gpio_set_level(RGB_LED_GREEN_GPIO, g > 0 ? 1 : 0);
    gpio_set_level(RGB_LED_BLUE_GPIO,  b > 0 ? 1 : 0);
#else
    if (s_led_strip) {
        if (r == 0 && g == 0 && b == 0) {
            ws2812_clear(s_led_strip);
        } else {
            ws2812_set_pixel(s_led_strip, 0, r, g, b);
            ws2812_refresh(s_led_strip);
        }
    }
#endif
}

/** Periodic callback to return the LED to IDLE state. */
static void return_timer_callback(void* arg)
{
    // Auto-return to IDLE if not in a sticky state
    if (s_current_state != LED_STATE_ERROR && 
        s_current_state != LED_STATE_WARNING &&
        s_current_state != LED_STATE_DIAGNOSTIC && 
        s_current_state != LED_STATE_IDLE) {
        s_current_state = LED_STATE_IDLE;
        rgb_set(0, 10, 0);   // Dim GREEN
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void status_indicator_configure(void)
{
#if USE_EXTERNAL_RGB_LED
    ESP_LOGI(TAG, "Configuring EXTERNAL RGB LED  R=%d  G=%d  B=%d",
             RGB_LED_RED_GPIO, RGB_LED_GREEN_GPIO, RGB_LED_BLUE_GPIO);

    // Configure each colour channel as push-pull output, start LOW (off)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RGB_LED_RED_GPIO)
                      | (1ULL << RGB_LED_GREEN_GPIO)
                      | (1ULL << RGB_LED_BLUE_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
#else
    ESP_LOGI(TAG, "Configuring INTERNAL WS2812B LED on GPIO %d", WS2812B_LED_GPIO);
    
    if (ws2812_init(WS2812B_LED_GPIO, 1, &s_led_strip) == ESP_OK) {
        ws2812_clear(s_led_strip);
    } else {
        ESP_LOGE(TAG, "Failed to initialize WS2812B LED");
    }
#endif

    // Setup auto-return timer (one-shot 50ms)
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &return_timer_callback,
        .name = "status_ind_timer"
    };
    esp_timer_create(&oneshot_timer_args, &s_return_timer);

    // Default state: IDLE
    status_indicator_set_state(LED_STATE_IDLE);
}

void status_indicator_set_state(led_state_t state)
{
    // If we're already in this state, or in error state (which is sticky), do nothing unless it's a new error.
    if (s_current_state == state || (s_current_state == LED_STATE_ERROR && state != LED_STATE_IDLE && state != LED_STATE_OFF)) return;
    
    s_current_state = state;

    // Stop timer if it's running
    esp_timer_stop(s_return_timer);

    switch (state) {
        case LED_STATE_IDLE:
            rgb_set(0, 10, 0);   // Dim GREEN
            break;

        case LED_STATE_MODBUS_RX:
            rgb_set(0, 0, 50);   // BLUE
            esp_timer_start_once(s_return_timer, 50000); // 50ms
            break;

        case LED_STATE_ESPNOW_TX:
            rgb_set(50, 50, 0);  // YELLOW
            esp_timer_start_once(s_return_timer, 50000); // 50ms
            break;

        case LED_STATE_ESPNOW_RX:
            rgb_set(0, 50, 50);  // CYAN
            esp_timer_start_once(s_return_timer, 50000); // 50ms
            break;

        case LED_STATE_MODBUS_TX:
            rgb_set(50, 50, 50); // WHITE
            esp_timer_start_once(s_return_timer, 50000); // 50ms
            break;
            
        case LED_STATE_WARNING:
            rgb_set(50, 25, 0);  // ORANGE
            break;
            
        case LED_STATE_DIAGNOSTIC:
            rgb_set(25, 0, 50);  // PURPLE
            // Returns to idle via manual call
            break;

        case LED_STATE_ERROR:
            rgb_set(50, 0, 0);   // Solid RED
            break;

        case LED_STATE_OFF:
            rgb_set(0, 0, 0);    // OFF
            break;

        default:
            rgb_set(0, 0, 0);    // ALL OFF
            break;
    }
}

led_state_t status_indicator_get_state(void)
{
    return s_current_state;
}
