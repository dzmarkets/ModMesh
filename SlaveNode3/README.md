# ModMesh: Encrypted Modbus RTU Slave Node 3

![ModMesh Banner](../assets/banner.png)

## 📌 1. Introduction & Industrial Use Case

The **ModMesh Slave Node 3** operates as a hybrid wireless bridge and **Virtual Modbus Sensor** in the ModMesh ecosystem. Unlike a standard transparent bridge, Slave Node 3 intercepts incoming Modbus Read queries meant for **Slave ID 3** and processes them internally. 

It is configured to act as an Over-The-Air (OTA) Sensor, allowing a PLC connected to the Master Node to poll a physical push-button on the Slave Node without any RS-485 sensors attached. This demonstrates the system's ability to seamlessly integrate digital inputs directly into a traditional Modbus RTU network.

---

## 🏗️ 2. Industrial Dual-Core Architecture

![SlaveNode3 Architecture Scheme](assets/scheme.png)

To ensure strict Modbus timing and prevent wireless networking jitter, the Slave Node uses a strictly isolated dual-core architecture. For Slave Node 3, Core 0 handles the Virtual Sensor logic entirely in memory:

```mermaid
graph TD
    subgraph ESP32-S3 Dual-Core Processor
        subgraph Core 0 - Secure Wireless Pipeline [Normal Priority]
            BTN[GPIO 2 Push Button] -->|Toggle State| V_REG[Virtual Modbus Registers]
            EW[ESP-NOW Driver] -->|AES-128 Decrypt| FRAG[Defragmentation]
            FRAG -->|Modbus Read Request| CB[onEspNowRequestReceived]
            CB -->|Reads Toggled State| V_REG
            CB -->|Generates Response| EW
        end
        
        subgraph Core 1 - Real-Time Serial [High Priority]
            MB[modbus_bridge.cpp] --> UART[RS-485 UART - Standby]
        end
    end
    
    Master[Master Node] <-->|AES-128 CCMP Encryption| Core0[Core 0 Wireless Task]
    
    style Core 1 - Real-Time Serial fill:#e1f5fe,stroke:#03a9f4,stroke-width:2px
    style Core 0 - Secure Wireless Pipeline fill:#efebe9,stroke:#8d6e63,stroke-width:2px
```

### 📊 Thread & Core Allocation

| Core | Component / Task | RTOS Priority | Responsibility |
| :---: | :--- | :---: | :--- |
| **Core 0** | `espnow_control` | **5 (Normal)** | **Encrypted Wireless Pipeline & Sensor**. Receives encrypted queries from the Master Node, parses Modbus function codes natively (FC 01, 03), reads the `GPIO 2` internal toggled state, and dynamically generates valid Modbus RTU responses (including CRC-16) to beam back. |
| **Core 1** | `modbus_bridge` | **10 (High)** | **Standby UART**. Initializes the RS-485 driver in case physical bridging is required in future configurations. |

---

## 🏭 3. Virtual Modbus Sensor Logic

Slave Node 3 emulates a physical Modbus device by parsing standard Modbus frames entirely in software and reading hardware GPIO states.

```mermaid
sequenceDiagram
    autonumber
    participant MN as Master Node
    participant SN as Slave Node 3 (Sensor)
    participant BTN as GPIO 2 Button

    Note over BTN, SN: Hardware Interrupt / Polling detects Falling Edge
    BTN->>SN: User Presses Button
    Note over SN: is_toggled = !is_toggled

    MN->>SN: ESP-NOW Query (Slave ID 3, FC 01, Read Coil)
    Note over SN: Decrypts & validates MAC
    Note over SN: Parses FC 01 (Read Coil)
    Note over SN: Reads internal `is_toggled` state
    Note over SN: Calculates Modbus CRC-16
    SN->>MN: ESP-NOW Encrypted Modbus Response (State ON/OFF)
```

### Supported Function Codes:
- **FC 01 (Read Coils) / FC 03 (Read Holding Registers)**: Returns the current toggled state of the Button (0 or 1).

---

## 🔐 4. Wireless Security & Custom Fragmentation

The Slave Node acts as a secure wireless terminus for the industrial network:

- **Hardware-Level AES-128 Encryption**: All over-the-air ESP-NOW frames are encrypted using the ESP32-S3's hardware Wi-Fi cryptographic engine (`PMK` and `LMK`).
- **Protected Peer Whitelisting**: The Slave Node explicitly binds to `MASTER_NODE_MAC`. Unregistered or un-encrypted devices attempting to spoof the network are silently dropped by the MAC layer.

---

## 🔌 5. Hardware Setup & Pinout Configurations

The Slave Node is built on the **ESP32-S3 DevKit** platform.

```
       ┌────────────────────────────────────────────────────────┐
       │                       ESP32-S3                         │
       │                                                        │
       │  [GPIO  2] ◄────── [Push Button] ──────► GND           │
       │                    (Internal Pull-up)                  │
       │                                                        │
       │  [GPIO 17] ──────► [DI]   MAX485   [A] ───► RS-485 (A)  │
       │  [GPIO 18] ◄────── [RO]   MODULE   [B] ───► RS-485 (B)  │
       │                           (Auto-Dir)                   │
       │                                                        │
       │  [GPIO  1] ◄────── [Smart Reset Button] ──────► GND    │
       │  [GPIO 48] ──────► [WS2812 DIN (RGB LED)]               │
       └────────────────────────────────────────────────────────┘
```

### Pin Assignment Tables

| Pin Function | GPIO | ESP32-S3 Pin | Connection on Device |
| :--- | :---: | :---: | :--- |
| **Toggle Button** | **GPIO 2** | Pin 2 | Momentary button connected to GND. Firmware uses internal pull-up. |
| **MAX485 DI (TX)** | **GPIO 17** | Pin 17 | Driver Input (DI) |
| **MAX485 RO (RX)** | **GPIO 18** | Pin 18 | Receiver Output (RO) |
| **Smart Reset Button** | **GPIO 1** | Pin 1 | Momentary Button connected to GND (Active Low) |
| **WS2812 DIN** | **GPIO 48**| Pin 48 | WS2812B NeoPixel Data Input (DIN) |

### Smart Reset & pre-Wipe Blinking Logic
- **Short Press (< 1s)**: Triggers a clean soft reboot (**Blue** flash).
- **Double Click**: Prints detailed runtime statistics to Serial (**Purple** flash).
- **Hold $\ge$ 3s (Local Reset)**: Blinks **Red/Off at 10Hz for 3 seconds** before wiping the NVS flash.
- **Remote Reset**: If the Master Node triggers a network-wide wipe, the Slave Node will receive the secure signature, blink rapidly, and erase itself.

---

## 🚦 6. Color Spectrum Visual Feedback (WS2812 NeoPixel)

The Slave Node incorporates a WS2812 NeoPixel to provide sub-50ms visual tracking:

| Color | Mode / Pattern | State Name | Meaning |
| :---: | :--- | :--- | :--- |
| 🟢 | **Dim Solid Green** | `LED_STATE_IDLE` | Node is healthy, idle, and listening. |
| 🔴 | **Solid Red** | `LED_STATE_ERROR` | System failure (Wi-Fi, UART, or NVS init error). |
| 🌐 | **Quick Cyan Flash** | `LED_STATE_ESPNOW_RX` | Encrypted query received from Master Node. |
| 🟡 | **Quick Yellow Flash** | `LED_STATE_ESPNOW_TX` | Encrypted, internally-generated Modbus response beamed back to Master Node. |
| 🔴🔴 | **10Hz Red/Off Blink** | Pre-Wipe Warning | Wiping NVS Flash in progress (lasts 3 seconds). |

---

## ⚙️ 7. Configuration (`shared_config.h`)

All core network configurations are managed centrally in `shared_config.h`:

```cpp
#define ESPNOW_WIFI_CHANNEL 1                  // Low-latency pinned Wi-Fi channel
#define MODBUS_BAUD_RATE    9600               // Industrial standard baud rate

// Peer Hardware MAC Whitelist
static const uint8_t MASTER_NODE_MAC[6]  = {0x94, 0xA9, 0x90, 0x19, 0x6A, 0x1C};
static const uint8_t SLAVE_NODE_3_MAC[6] = {0xAC, 0xA7, 0x04, 0x15, 0xBC, 0xC0};
```

---

## 🛠️ 8. Educational Log Analysis & Troubleshooting

Students can analyze the following serial logs to verify the Virtual Sensor is operating flawlessly:

### Case A: User Presses Button
```log
I (1000) SlaveNode3: Slave Node 3 initialized as Virtual Sensor and listening.
I (15200) SlaveNode3: Button pressed! Toggled state to ON
```

### Case B: Master Node Polls Sensor (FC 01)
```log
I (16130) SlaveNode3: Received ESP-NOW query from Master (len: 8)
I (16130) ESPNOW_RX: 03 01 00 00 00 01 84 28               # Master requests Read Coil
I (16140) SlaveNode3: Sent Modbus response back to Master (len: 6)
I (16140) ESPNOW_TX: 03 01 01 01 90 50                     # State is returned as ON (0x01)
```

---

*Developed by M. YOUCEF Yazid | v1.0.0 Slave Node 3 Production Edition*
