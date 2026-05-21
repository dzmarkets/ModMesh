//
// File Path: ESP-NOW-MeshCore/components/ws2812_driver/ws2812_driver.c
// Brief:     Source file for the custom WS2812 driver using ESP32 RMT peripheral.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.1.0
// CreateDate: 2026-05-05
// UpdateDate: 2026-05-05
//

#include "ws2812_driver.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ws2812_driver";

// WS2812 timing requirements (approximate for 10MHz resolution)
#define WS2812_T0H_TICKS 4  // 0.4us
#define WS2812_T0L_TICKS 8  // 0.8us
#define WS2812_T1H_TICKS 8  // 0.8us
#define WS2812_T1L_TICKS 4  // 0.4us
#define WS2812_RESET_TICKS 500 // 50us

typedef struct {
    rmt_channel_handle_t tx_chan;
    rmt_encoder_handle_t led_encoder;
    uint32_t max_leds;
    uint8_t *pixel_buf;
} ws2812_t;

// Custom encoder for WS2812
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws_encoder = (ws2812_encoder_t *)encoder;
    rmt_encoder_handle_t bytes_encoder = ws_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = ws_encoder->copy_encoder;
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;

    switch (ws_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws_encoder->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
        __attribute__((fallthrough));
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ws_encoder->reset_code, sizeof(ws_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws_encoder->state = 0;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t ws2812_del_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws_encoder = (ws2812_encoder_t *)encoder;
    rmt_del_encoder(ws_encoder->bytes_encoder);
    rmt_del_encoder(ws_encoder->copy_encoder);
    free(ws_encoder);
    return ESP_OK;
}

static esp_err_t ws2812_reset_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws_encoder = (ws2812_encoder_t *)encoder;
    rmt_encoder_reset(ws_encoder->bytes_encoder);
    rmt_encoder_reset(ws_encoder->copy_encoder);
    ws_encoder->state = 0;
    return ESP_OK;
}

static esp_err_t ws2812_new_encoder(rmt_encoder_handle_t *ret_encoder)
{
    ws2812_encoder_t *ws_encoder = calloc(1, sizeof(ws2812_encoder_t));
    ESP_RETURN_ON_FALSE(ws_encoder, ESP_ERR_NO_MEM, TAG, "no mem for ws2812 encoder");
    ws_encoder->base.encode = ws2812_encode;
    ws_encoder->base.del = ws2812_del_encoder;
    ws_encoder->base.reset = ws2812_reset_encoder;

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = WS2812_T0H_TICKS,
            .level0 = 1,
            .duration1 = WS2812_T0L_TICKS,
            .level1 = 0
        },
        .bit1 = {
            .duration0 = WS2812_T1H_TICKS,
            .level0 = 1,
            .duration1 = WS2812_T1L_TICKS,
            .level1 = 0
        },
        .flags.msb_first = 1
    };
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &ws_encoder->bytes_encoder), TAG, "failed to create bytes encoder");

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &ws_encoder->copy_encoder), TAG, "failed to create copy encoder");

    ws_encoder->reset_code = (rmt_symbol_word_t) {
        .duration0 = WS2812_RESET_TICKS,
        .level0 = 0,
        .duration1 = 0,
        .level1 = 0
    };

    *ret_encoder = &ws_encoder->base;
    return ESP_OK;
}

esp_err_t ws2812_init(int gpio_num, uint32_t max_leds, ws2812_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Initializing WS2812 on GPIO %d for %lu LEDs", gpio_num, (unsigned long)max_leds);
    ws2812_t *ws = calloc(1, sizeof(ws2812_t));
    ESP_RETURN_ON_FALSE(ws, ESP_ERR_NO_MEM, TAG, "no mem for ws2812 handle");

    ws->max_leds = max_leds;
    ws->pixel_buf = calloc(1, max_leds * 3);
    if (!ws->pixel_buf) {
        free(ws);
        return ESP_ERR_NO_MEM;
    }

    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz, 1 tick = 0.1us
        .trans_queue_depth = 4,
    };
    ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &ws->tx_chan), err, TAG, "failed to create RMT TX channel");
    ESP_GOTO_ON_ERROR(ws2812_new_encoder(&ws->led_encoder), err, TAG, "failed to create WS2812 encoder");
    ESP_GOTO_ON_ERROR(rmt_enable(ws->tx_chan), err, TAG, "failed to enable RMT channel");

    *out_handle = (ws2812_handle_t)ws;
    return ESP_OK;

err:
    if (ws->tx_chan) rmt_del_channel(ws->tx_chan);
    if (ws->pixel_buf) free(ws->pixel_buf);
    free(ws);
    return ret;
}

esp_err_t ws2812_set_pixel(ws2812_handle_t handle, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    ws2812_t *ws = (ws2812_t *)handle;
    ESP_RETURN_ON_FALSE(index < ws->max_leds, ESP_ERR_INVALID_ARG, TAG, "pixel index out of range");
    
    // WS2812 uses GRB order
    ws->pixel_buf[index * 3 + 0] = green;
    ws->pixel_buf[index * 3 + 1] = red;
    ws->pixel_buf[index * 3 + 2] = blue;
    
    return ESP_OK;
}

esp_err_t ws2812_refresh(ws2812_handle_t handle)
{
    ws2812_t *ws = (ws2812_t *)handle;
    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    return rmt_transmit(ws->tx_chan, ws->led_encoder, ws->pixel_buf, ws->max_leds * 3, &transmit_config);
}

esp_err_t ws2812_clear(ws2812_handle_t handle)
{
    ws2812_t *ws = (ws2812_t *)handle;
    memset(ws->pixel_buf, 0, ws->max_leds * 3);
    return ws2812_refresh(handle);
}
