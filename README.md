# ROS2-CAN-Bridge — CAN Communication Bridge for ROS2

**A solution to the lack of native CAN bus support in micro-ROS**, enabling
communication between a ROS2 node (Raspberry Pi 5) and an STM32
microcontroller over a standard CAN bus.

![Status](https://img.shields.io/badge/status-working-brightgreen)
![ROS2](https://img.shields.io/badge/ROS2-Jazzy-blue)
![STM32](https://img.shields.io/badge/STM32-F4%20%2F%20L4-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## Context and Problem Solved

**micro-ROS**, the ROS2 variant designed for microcontrollers, provides
**no native transport for the CAN bus** (Controller Area Network). The
officially supported transports are limited to Serial, UDP/Ethernet, and a
few specific stacks (such as XRCE-DDS over WiFi). Yet the CAN bus remains
the standard in industrial and embedded robotics: reliability, determinism,
and resilience to electrical noise over long cable runs in harsh
environments.

To work around this limitation, this project implements an **application-
level bridge**: a set of ROS2 nodes running on a Raspberry Pi 5 (fitted
with an MCP2515-based CAN HAT) that translate ROS2 messages into CAN
frames, paired with a bare-metal STM32 firmware (HAL) that receives these
frames, executes the requested actions, and sends its acknowledgements
back over the same bus.

This bridge makes it possible to integrate an STM32 microcontroller into a
full ROS2 system **without relying on micro-ROS's CAN support**, using the
CAN bus purely as a transport layer driven by standard application code
(STM32 HAL on the microcontroller side, python-can over SocketCAN on the
Raspberry Pi side).

## Architecture

```
┌─────────────────┐         CAN Bus (500 kbit/s)          ┌──────────────────────┐
│  Raspberry Pi 5  │ ◄──────────────────────────────────► │  STM32 (Nucleo L476RG │
│                  │   MCP2515 (CAN HAT)  <->  SN65HVD230  │  or F4 Discovery)     │
│  ROS2 nodes      │                                        │  Bare-metal firmware │
│  (python-can /   │                                        │  HAL + bxCAN         │
│   SocketCAN)     │                                        │                      │
└─────────────────┘                                        └──────────────────────┘
```

- **Raspberry Pi 5 side**: ROS2 nodes in Python (`rclpy`) using the
  `python-can` library over a `SocketCAN` interface (`can0`), itself
  driven by an MCP2515 CAN controller connected via SPI.
- **STM32 side**: bare-metal firmware built with STM32CubeIDE (HAL), using
  the microcontroller's built-in bxCAN peripheral and an SN65HVD230
  transceiver for the physical bus interface.
- **Application protocol**: a documented CAN frame format (see below)
  carrying position setpoints (x, y, theta), free-form text messages, and
  acknowledgements.

## Features

- Sends a position setpoint (x, y, theta) from the Raspberry Pi to the STM32
- STM32-side processing of the setpoint (display, timed delay simulating a
  physical move), followed by a "position reached" acknowledgement
- Sends free-form text messages from the Raspberry Pi to the STM32
  (multi-frame protocol to work around the 8-byte limit of standard CAN
  frames)
- Distinguishes acknowledgement types (position vs. message) via a status
  byte in the response frame
- Local visual feedback on the STM32 (LED) synchronized with CAN events

## CAN Frame Protocol

| CAN ID | Sender | Content |
|--------|--------|---------|
| `0x100` | RPi5 → STM32 | Position setpoint: x, y, theta as little-endian int16 (real value x100) |
| `0x150` | RPi5 → STM32 | Free-form text message, split into 8-byte frames, terminated by a null byte |
| `0x200` | STM32 → RPi5 | Acknowledgement: `0x01` = position reached, `0x02` = message received |

## Hardware Used

- Raspberry Pi 5
- MCP2515-based CAN HAT (SPI)
- STM32 Nucleo L476RG **or** STM32F4 Discovery (STM32F407VGT6)
- SN65HVD230 CAN transceiver
- 120-ohm termination resistors (x2, one at each end of the bus)

## Repository Structure

```
.
├── ros2_ws/                          # ROS2 workspace (Raspberry Pi side)
│   └── src/stm32_can_bridge/
│       ├── stm32_can_bridge/
│       │   ├── can_position_publisher.py
│       │   ├── can_ack_listener.py
│       │   └── can_string_sender.py
│       └── launch/bridge_launch.py
├── stm32_firmware/                   # STM32 firmware (bare-metal, HAL)
│   ├── Core/Src/main.c
│   └── ...
└── docs/                             # Documentation, diagrams, CubeMX config
```

## Installation and Usage

### 1. Raspberry Pi 5 Setup

```bash
# Enable SPI and the MCP2515 driver (adjust oscillator/interrupt to your HAT)
sudo nano /boot/firmware/config.txt
# Add:
#   dtparam=spi=on
#   dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
sudo reboot

# Dependencies
sudo apt install can-utils python3-pip -y
pip3 install python-can --break-system-packages

# Bring up the CAN interface
sudo ip link set can0 up type can bitrate 500000
```

### 2. Build and Run the ROS2 Package

```bash
cd ros2_ws
colcon build --packages-select stm32_can_bridge
source install/setup.bash

# Send a position setpoint
ros2 launch stm32_can_bridge bridge_launch.py x:=1.5 y:=2.3 theta:=0.78

# Send a free-form text message (interactive)
ros2 run stm32_can_bridge can_string_sender
```

### 3. STM32 Firmware

1. Open the project in STM32CubeIDE
2. Verify the CAN1 configuration (see `docs/` for board-specific details)
3. Build and flash

See the full documentation in `docs/` for CubeMX configuration details
specific to the board used (Nucleo L476RG or F4 Discovery).

## Possible Improvements

- Add a CRC or sequence number to the protocol to detect lost or
  duplicated frames
- Move to extended (29-bit) CAN identifiers if the number of message
  types grows
- Port the STM32 firmware to FreeRTOS for proper multitasking
- Publish a structured ROS2 message (instead of a plain `std_msgs/String`)
  for cleaner integration into a larger ROS2 graph

## License

MIT — see the `LICENSE` file.

## Author

Project developed by [Your Name] — [GitHub / LinkedIn link]
