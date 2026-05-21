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
static led_state_t s_current_state = LED_STATE_DISCONNECTED;
static esp_timer_handle_t s_blink_timer = NULL;
static bool s_blink_toggle = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Drive all three colour channels to a specific RGB value (0 or 1 each). */
static void rgb_set(int r, int g, int b)
{
#if USE_EXTERNAL_RGB_LED
    gpio_set_level(RGB_LED_RED_GPIO,   r);
    gpio_set_level(RGB_LED_GREEN_GPIO, g);
    gpio_set_level(RGB_LED_BLUE_GPIO,  b);
#else
    if (s_led_strip) {
        if (r == 0 && g == 0 && b == 0) {
            ws2812_clear(s_led_strip);
        } else {
            // WS2812 is bright, using 50 instead of 255
            ws2812_set_pixel(s_led_strip, 0, r ? 50 : 0, g ? 50 : 0, b ? 50 : 0);
            ws2812_refresh(s_led_strip);
        }
    }
#endif
}

/** Periodic callback to toggle the LED for blinking states. */
static void blink_timer_callback(void* arg)
{
    if (s_current_state == LED_STATE_PARTIAL) {
        s_blink_toggle = !s_blink_toggle;
        if (s_blink_toggle) {
            rgb_set(0, 1, 0); // GREEN ON
        } else {
            rgb_set(0, 0, 0); // OFF
        }
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

    // Setup blinking timer (500ms toggle)
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &blink_timer_callback,
        .name = "status_ind_timer"
    };
    esp_timer_create(&periodic_timer_args, &s_blink_timer);
    esp_timer_start_periodic(s_blink_timer, 500000); // 500ms

    // Default state: RED (disconnected)
    status_indicator_set_state(LED_STATE_DISCONNECTED);
}

void status_indicator_set_state(led_state_t state)
{
    if (s_current_state == state) return;
    s_current_state = state;

    switch (state) {
        case LED_STATE_DISCONNECTED:
            rgb_set(1, 0, 0);   // RED
            ESP_LOGI(TAG, "Indicator -> RED   (No peers online)");
            break;

        case LED_STATE_CONNECTED:
            rgb_set(0, 1, 0);   // GREEN SOLID
            ESP_LOGI(TAG, "Indicator -> GREEN (All peers online)");
            break;

        case LED_STATE_PARTIAL:
            s_blink_toggle = true;
            rgb_set(0, 1, 0);   // Start GREEN ON
            ESP_LOGI(TAG, "Indicator -> GREEN BLINK (Partial mesh)");
            break;

        case LED_STATE_SENDING:
            rgb_set(0, 0, 1);   // BLUE
            ESP_LOGI(TAG, "Indicator -> BLUE  (Sending Sensor Data / Waiting ACK)");
            break;

        case LED_STATE_OFF:
            rgb_set(0, 0, 0);   // OFF
            break;

        default:
            rgb_set(0, 0, 0);   // ALL OFF
            break;
    }
}

led_state_t status_indicator_get_state(void)
{
    return s_current_state;
}
