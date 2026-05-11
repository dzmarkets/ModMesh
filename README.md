# ModMesh

![ModMesh Banner](assets/banner.png)

## 🚀 Overview

**ModMesh** is a professional-grade, modular ESP-NOW mesh networking ecosystem. It leverages a robust Pub/Sub architecture and a Quad-Task RTOS model to provide high-reliability communication for distributed IoT systems.

The project is structured into three specialized roles, each managed as a submodule to ensure consistency and modularity:
- **Gateway**: Central coordinator and external interface.
- **Sensor**: Optimized for data acquisition and periodic reporting.
- **Actuator**: High-priority execution nodes for hardware control.

---

## 🏗️ Project Structure

ModMesh utilizes Git submodules to maintain a single source of truth (`ESP-NOW-MeshCore`) while allowing role-specific configurations.

```text
ModMesh/
├── Gateway/     # Submodule: Mesh-to-External bridge
├── Sensor/      # Submodule: Data acquisition nodes
├── Actuator/    # Submodule: Control and execution nodes
└── assets/      # Documentation resources
```

---

## ✨ Key Features

### ⚡ Quad-Task RTOS Architecture
Optimized for real-time performance with specialized tasks:
1. **Mesh Task**: Handles ESP-NOW protocol and peer discovery.
2. **Sensor/App Task**: High-priority data acquisition.
3. **Reset Monitor**: Persistent background monitor for system health.
4. **Diagnostic Task**: Handles visual feedback and logging.

### 📡 Keyword-Based Pub/Sub Routing
Eliminates complex addressing by using semantic keywords (e.g., `[LIGHT]`, `[TEMP]`) for message routing, allowing nodes to subscribe to specific data streams dynamically.

### 🔐 Enterprise-Grade Security
- **AES Payload Encryption**: End-to-end protection for all mesh traffic.
- **Peer Authentication**: Handshake-based validation for new nodes.
- **Network Integrity**: Secure decommissioning via Emergency Mesh Reset.

### 🚨 Emergency Mesh Reset
A fail-safe mechanism that allows for a network-wide "Zero-State" reset. Features a 3-second visual RED blink warning before clearing all persistent peer data and actuator states.

---

## 🛠️ Getting Started

### Prerequisites
- ESP-IDF v5.x
- Git

### Installation
Clone the repository and initialize all submodules:

```bash
git clone --recursive https://github.com/dzmarkets/ModMesh.git
cd ModMesh
```

If you have already cloned the repo without submodules:

```bash
git submodule update --init --recursive
```

### Building a Role
Navigate to the desired role directory and build using `idf.py`:

```bash
cd Gateway
idf.py build
```

---

## 📊 Visual Diagnostics

ModMesh uses a smart LED signaling system for immediate hardware feedback:
- **🔵 Blue Pulse**: Active sensor data transmission.
- **🔴 Red Blink (Slow)**: Peer discovery or partial mesh state.
- **🔴 Red Flash (Rapid)**: Emergency reset warning (3 seconds).
- **🟢 Green Pulse**: Successful peer handshake.

---

## 📄 License
This project is part of the `dzmarkets` ecosystem. All rights reserved.