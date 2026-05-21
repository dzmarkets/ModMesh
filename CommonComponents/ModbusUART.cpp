#include "ModbusUART.h"
#include "SharedConfig.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "ModbusUART";

ModbusUART::ModbusUART() : m_rx_callback(nullptr), m_uart_queue(nullptr) {}

ModbusUART::~ModbusUART() {
    // Cleanup if needed
}

bool ModbusUART::begin(ModbusFrameReceivedCb rx_callback) {
    m_rx_callback = rx_callback;

    ESP_LOGI(TAG, "Initializing Modbus UART on port %d, baud %d", MODBUS_UART_PORT, MODBUS_BAUD_RATE);

    const uart_config_t uart_cfg = {
        .baud_rate  = MODBUS_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags      = {
            .backup_before_sleep = 0
        }
    };

    ESP_ERROR_CHECK(uart_driver_install(MODBUS_UART_PORT,
                                        MODBUS_MAX_FRAME_SIZE * 2, 0,
                                        20, &m_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(MODBUS_UART_PORT, &uart_cfg));

    ESP_ERROR_CHECK(uart_set_pin(MODBUS_UART_PORT,
                                 MAX485_TXD_GPIO,
                                 MAX485_RXD_GPIO,
                                 MAX485_RE_DE_GPIO,
                                 UART_PIN_NO_CHANGE));
                                 
    // Enable internal pull-up on RX to prevent floating (causes 0x00 spam)
    gpio_set_pull_mode((gpio_num_t)MAX485_RXD_GPIO, GPIO_PULLUP_ONLY);
    
    // Set RS485 half duplex mode which automatically handles DE/RE
    ESP_ERROR_CHECK(uart_set_mode(MODBUS_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    // Create the task, pinned to Core 1 (APP_CPU) to ensure strict timing
    xTaskCreatePinnedToCore(uartTask, "modbus_uart_task", 4096, this, 10, NULL, 1);

    return true;
}

void ModbusUART::transmit(const uint8_t* frame, size_t len) {
    if (len == 0 || frame == nullptr) return;
    uart_write_bytes(MODBUS_UART_PORT, (const char *)frame, len);
    // Wait for TX to finish to ensure 3.5t gap before next interaction
    uart_wait_tx_done(MODBUS_UART_PORT, pdMS_TO_TICKS(100));
}

void ModbusUART::uartTask(void *pvParameters) {
    ModbusUART* instance = static_cast<ModbusUART*>(pvParameters);
    instance->processTask();
}

void ModbusUART::processTask() {
    uint8_t rx_buf[MODBUS_MAX_FRAME_SIZE];
    size_t  rx_len = 0;
    
    // For 9600 baud, 1 char = 11 bits ~ 1.14ms.
    // 3.5 chars ~ 4ms. We'll use a 5ms timeout to detect end of frame.
    const int frame_timeout_ms = 5;

    while (1) {
        uart_event_t event;
        if (xQueueReceive(m_uart_queue, &event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                // Read available bytes
                int n = uart_read_bytes(MODBUS_UART_PORT,
                                        rx_buf + rx_len,
                                        sizeof(rx_buf) - rx_len,
                                        pdMS_TO_TICKS(frame_timeout_ms));
                if (n > 0) {
                    rx_len += n;
                }

                // Wait for inter-frame silence to detect end of Modbus frame
                vTaskDelay(pdMS_TO_TICKS(frame_timeout_ms));
                
                // Read any stragglers
                int extra = uart_read_bytes(MODBUS_UART_PORT,
                                            rx_buf + rx_len,
                                            sizeof(rx_buf) - rx_len,
                                            0); // No wait
                if (extra > 0) {
                    rx_len += extra;
                }

                if (rx_len > 0) {
                    // Frame complete (gap detected)
                    if (m_rx_callback) {
                        m_rx_callback(rx_buf, rx_len);
                    }
                    rx_len = 0; // Reset for next frame
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow — flushing");
                uart_flush_input(MODBUS_UART_PORT);
                xQueueReset(m_uart_queue);
                rx_len = 0;
            }
        }
    }
}
