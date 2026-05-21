# ModMesh: Slave Node 1 Documentation

## 📌 Introduction

The **ModMesh Slave Node 1** operates as a wireless bridge sensor node in the ModMesh ecosystem. It is an **Encrypted Modbus RTU Slave Router** that acts as the Modbus Master to physical RS-485 sensors (such as temp/humidity sensors, meters, etc.) and routes requests back and forth to the **Master Node** over the air using the **ESP-NOW** protocol.

It is paired specifically to handle queries meant for **Slave ID 1** and communicates directly with the Master Node.

---

## 🏗️ Dual-Core Architecture

To ensure strict Modbus timing and prevent wireless networking jitter from causing timeouts on the physical sensor bus, the Slave Node uses a strictly isolated dual-core architecture.

### 📊 RTOS Task Model
| Core | Task / Component | Responsibility |
| :---: | :--- | :--- |
| **Core 1** | `modbus_bridge` | **Real-Time RS-485 UART**. Communicates directly with the physical sensor. Transmits queries and polls the UART FIFO with a strict 5ms gap timeout to frame responses. |
| **Core 0** | `espnow_control` | **Wireless Networking & Security**. Receives encrypted queries from the Master Node over ESP-NOW and immediately routes the sensor's response back. |

---

## 🏭 Modbus RTU Slave Router Logic

Unlike the Master Node which receives requests from a PLC, the Slave Node is in standby waiting for wireless queries.

### Logic Workflow: Master to Physical Sensor
1. **Master Node** sends an encrypted Modbus query meant for Slave ID 1 over ESP-NOW.
2. **Slave Node (Core 0)** receives, decrypts, and reassembles the packet.
3. **Slave Node** immediately writes the raw Modbus frame onto the local RS-485 bus to poll the physical sensor.

### Logic Workflow: Physical Sensor to Master
1. **Physical Sensor** responds to the Modbus query over RS-485.
2. **Slave Node (Core 1)** detects the end of the response frame via the 3.5t gap timer.
3. **Slave Node** beams the raw Modbus response back to the Master Node over ESP-NOW.

---

## 🔐 Security & Data Integrity

The Slave Node features MAC-layer hardware-accelerated security:
- **AES-128-CCMP**: All packets are encrypted at the Wi-Fi physical MAC layer.
- **Dedicated Keys**: Utilizes strict `PMK` and `LMK` pairing matching the Master Node.
- **Master Node Pinning**: Only traffic received from the configured `MASTER_NODE_MAC` is accepted and processed.
- **Custom Fragmentation**: Reassembles fragmented payloads > 250 bytes received over the air.

---

## ⚙️ Configuration (shared_config.h)

Centralized routing configurations:

| Parameter | Default | Description |
| :--- | :--- | :--- |
| `MAX485_RE_DE_GPIO`| `-1` | RS-485 Half-Duplex Direction Control. |
| `MAX485_TXD_GPIO` | `17` | UART TX (ESP32) -> DI (MAX485). |
| `MAX485_RXD_GPIO` | `18` | UART RX (ESP32) -> RO (MAX485). |
| `MODBUS_BAUD_RATE` | `9600` | Industrial standard for RS-485 sensors. |
| `MASTER_NODE_MAC` | `94:A9:90:19:6A:1C` | Target MAC of the Master Node. |

---

## 🚦 Data Flow Color Spectrum (WS2812 RGB LED)

The Slave Node features a built-in WS2812B NeoPixel that provides instant, sub-50ms visual feedback of the routing pipeline. When a successful Modbus poll occurs, you will see a rapid, beautiful "spectrum flash":

- 🟢 **Dim Green**: **Idle/Ready** - Initialized and listening.
- 🔴 **Solid Red**: **Error** - Wi-Fi Init failed or UART driver failure.
- 🌐 **Quick Flash Cyan**: **ESP-NOW RX** - Received query from Master Node.
- ⚪ **Quick Flash White**: **Modbus TX** - Transmitting query to Physical Sensor.
- 🔵 **Quick Flash Blue**: **Modbus RX** - Received response from Physical Sensor.
- 🟡 **Quick Flash Yellow**: **ESP-NOW TX** - Beaming response back to Master Node.

---

## 🔌 Hardware Setup (ESP32-S3)

### 1. RS-485 Wiring (Connecting to Sensor)
- **VCC**: 3.3V or 5V (Match MAX485 module).
- **GND**: Common Ground.
- **DI (TX)**: GPIO 17.
- **RO (RX)**: GPIO 18.
- **RE/DE**: Not connected (Auto-direction).

### 2. Smart Reset Button
- **GPIO 1**: Connect to GND via a momentary switch (Active Low).
- **Short Press (<1s)**: Soft Reboot (Blue flash).
- **Double Click**: Print Diagnostics to serial (Purple flash).
- **Long Hold (>3s)**: Factory Reset (Orange to Rapid Red).

### 3. Status Indicator
- **WS2812B DIN**: GPIO 48 (Standard ESP32-S3 DevKit).

---

*Developed by M. YOUCEF Yazid | v1.0.0 Slave Node 1 Edition*
