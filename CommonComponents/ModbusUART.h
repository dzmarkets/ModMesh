#ifndef MODBUS_UART_H
#define MODBUS_UART_H

#include <stdint.h>
#include <stddef.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Callback type for when a complete Modbus frame is received on UART
typedef std::function<void(const uint8_t* frame, size_t len)> ModbusFrameReceivedCb;

class ModbusUART {
public:
    ModbusUART();
    ~ModbusUART();

    // Initialize UART (Pinned to Core 1 internally by FreeRTOS task)
    bool begin(ModbusFrameReceivedCb rx_callback);

    // Send a Modbus frame over RS-485
    void transmit(const uint8_t* frame, size_t len);

private:
    static void uartTask(void *pvParameters);
    void processTask();

    ModbusFrameReceivedCb m_rx_callback;
    QueueHandle_t m_uart_queue;
};

#endif // MODBUS_UART_H
