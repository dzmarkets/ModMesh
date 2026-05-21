# ModMesh: Master Node Documentation

## 📌 Introduction

The **ModMesh Master Node** is the core bridge router for the ModMesh ecosystem. Unlike traditional Modbus Slaves, this device operates as an **Encrypted Modbus RTU Router**. It physically connects to the Industrial Control System (PLC) via **RS-485** and bridges Modbus queries to remote, wireless **Slave Nodes** using the **ESP-NOW** protocol.

It handles real-time traffic routing, packet fragmentation/reassembly, and provides instant visual feedback on data flow.

---

## 🏗️ Dual-Core Architecture

To ensure strict Modbus timing and prevent wireless networking jitter from causing timeouts on the PLC, the Master Node uses a strictly isolated dual-core architecture.

### 📊 RTOS Task Model
| Core | Task / Component | Responsibility |
| :---: | :--- | :--- |
| **Core 1** | `modbus_bridge` | **Real-Time RS-485 UART**. Polls the UART FIFO with a strict 5ms gap timeout (3.5 character equivalent at 9600 baud) to perfectly frame incoming Modbus requests from the PLC. |
| **Core 0** | `espnow_control` | **Wireless Networking & Security**. Handles Wi-Fi MAC layer interrupts, CCMP AES-128 encryption/decryption, and the fragmentation/reassembly of payloads > 250 bytes. |

---

## 🏭 Modbus RTU Router Logic

Unlike the legacy Gateway that acted as a virtual slave, the Master Node is completely transparent to the PLC.

### Logic Workflow: PLC to Wireless Slave
1. **PLC** sends a standard Modbus RTU request on the RS-485 bus (e.g., Read Holding Registers from Slave ID 1).
2. **Master Node (Core 1)** detects the end of the frame via the 3.5t gap timer.
3. **Master Node** checks the first byte (Slave ID).
4. If the ID matches a registered MAC address (e.g., `SLAVE_NODE_1_MAC`), it immediately beams the raw Modbus frame over the air via **ESP-NOW**.

### Logic Workflow: Wireless Slave to PLC
1. **Wireless Slave** responds to the Modbus query over ESP-NOW.
2. **Master Node (Core 0)** receives, decrypts, and reassembles the packet.
3. **Master Node** immediately writes the raw Modbus response back onto the RS-485 bus.
4. **PLC** receives the response exactly as if it were wired directly to the Slave.

---

## 🔐 Security & Data Integrity

The Master Node acts as a secure air-gap for the PLC network:
- **AES-128-CCMP**: All Modbus payloads are encrypted at the MAC layer using the Wi-Fi hardware engine.
- **Dedicated Keys**: Utilizes strict `PMK` (Primary Master Key) and `LMK` (Local Master Key) pairing.
- **Protected Peers**: Only traffic from explicitly whitelisted Slave MAC addresses will be routed to the RS-485 bus.
- **Custom Fragmentation**: Safely splits and reassembles oversized Modbus payloads (up to 256 bytes) across the 250-byte ESP-NOW limitation.

---

## ⚙️ Configuration (shared_config.h)

Centralized routing configurations:

| Parameter | Default | Description |
| :--- | :--- | :--- |
| `MAX485_RE_DE_GPIO`| `-1` | RS-485 Half-Duplex Direction Control (Handled automatically by hardware in this setup). |
| `MAX485_TXD_GPIO` | `17` | UART TX (ESP32) -> DI (MAX485). |
| `MAX485_RXD_GPIO` | `18` | UART RX (ESP32) -> RO (MAX485). |
| `MODBUS_BAUD_RATE` | `9600` | Industrial standard for PLCs. |
| `ESPNOW_WIFI_CHANNEL`| `1` | Strict channel pinning for low-latency transmission. |

---

## 🚦 Data Flow Color Spectrum (WS2812 RGB LED)

The Master Node features a built-in WS2812B NeoPixel that provides instant, sub-50ms visual feedback of the routing pipeline. When a successful Modbus poll occurs, you will see a rapid, beautiful "spectrum flash":

- 🟢 **Dim Green**: **Idle/Ready** - Initialized and listening.
- 🔴 **Solid Red**: **Error** - Wi-Fi Init failed or UART driver failure.
- 🔵 **Quick Flash Blue**: **Modbus RX** - Received query from PLC.
- 🟡 **Quick Flash Yellow**: **ESP-NOW TX** - Beaming query to Slave.
- 🌐 **Quick Flash Cyan**: **ESP-NOW RX** - Received response from Slave.
- ⚪ **Quick Flash White**: **Modbus TX** - Routing response back to PLC.

---

## 🔌 Hardware Setup (ESP32-S3)

### 1. RS-485 Wiring
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

*Developed by M. YOUCEF Yazid | v1.0.0 Master Node Edition*
