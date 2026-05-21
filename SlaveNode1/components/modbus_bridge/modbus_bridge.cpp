#include "modbus_bridge.h"
#include "shared_config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "modbus_bridge";
#define UART_PORT ((uart_port_t)MODBUS_UART_PORT)

static modbus_recv_cb_t s_rx_callback = NULL;
static QueueHandle_t s_uart_queue = NULL;

static void uart_task(void *pvParameters) {
    uint8_t rx_buf[MODBUS_MAX_FRAME_SIZE];
    size_t  rx_len = 0;
    
    // For 9600 baud, 3.5 chars ~ 4ms. We'll use a 5ms timeout.
    const int frame_timeout_ms = 5;

    while (1) {
        uart_event_t event;
        if (xQueueReceive(s_uart_queue, &event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                int n = uart_read_bytes(UART_PORT,
                                        rx_buf + rx_len,
                                        sizeof(rx_buf) - rx_len,
                                        pdMS_TO_TICKS(frame_timeout_ms));
                if (n > 0) {
                    rx_len += n;
                }

                vTaskDelay(pdMS_TO_TICKS(frame_timeout_ms));
                
                int extra = uart_read_bytes(UART_PORT,
                                            rx_buf + rx_len,
                                            sizeof(rx_buf) - rx_len,
                                            0);
                if (extra > 0) {
                    rx_len += extra;
                }

                if (rx_len > 0) {
                    if (s_rx_callback) {
                        s_rx_callback(rx_buf, rx_len);
                    }
                    rx_len = 0;
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow — flushing");
                uart_flush_input(UART_PORT);
                xQueueReset(s_uart_queue);
                rx_len = 0;
            }
        }
    }
}

bool modbus_bridge_init(modbus_recv_cb_t rx_callback) {
    s_rx_callback = rx_callback;

    ESP_LOGI(TAG, "Initializing Modbus UART on port %d, baud %d", MODBUS_UART_PORT, MODBUS_BAUD_RATE);

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = MODBUS_BAUD_RATE;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.rx_flow_ctrl_thresh = 0;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
                                        MODBUS_MAX_FRAME_SIZE * 2, 0,
                                        20, &s_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));

    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
                                 MAX485_TXD_GPIO,
                                 MAX485_RXD_GPIO,
                                 MAX485_RE_DE_GPIO,
                                 UART_PIN_NO_CHANGE));
                                 
    gpio_set_pull_mode((gpio_num_t)MAX485_RXD_GPIO, GPIO_PULLUP_ONLY);
    
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    xTaskCreatePinnedToCore(uart_task, "modbus_uart_task", 4096, NULL, 10, NULL, 1);

    return true;
}

void modbus_bridge_transmit(const uint8_t* frame, size_t len) {
    if (len == 0 || frame == NULL) return;
    uart_write_bytes(UART_PORT, (const char *)frame, len);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
}
