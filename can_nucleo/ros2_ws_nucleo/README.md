# Workspace ROS2 — Pont CAN RPi5 <-> STM32 Nucleo L476RG

## 1. Prerequis materiel (RPi5)

- CAN HAT (MCP2515) monte sur le RPi5.
- Bus CAN cable vers le module SN65HVD230 branche sur PA11/PA12 de la Nucleo.
- Resistances de terminaison 120 ohms aux DEUX extremites du bus.
- GND commun entre RPi5 et Nucleo.

## 2. Activer l'interface CAN sur le RPi5

Ajouter dans `/boot/firmware/config.txt` :

```
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
```

(adapter `oscillator=` a la valeur reelle du quartz de votre HAT — voir sa doc)

Redemarrer, puis :

```bash
sudo apt install can-utils python3-pip -y
pip3 install python-can

sudo ip link set can0 up type can bitrate 500000
ip -details link show can0
```

Test bas niveau (optionnel, avant de lancer ROS2) :

```bash
candump can0
```

## 3. Compiler le package ROS2

```bash
cd ~/ros2_ws
colcon build --packages-select stm32_can_bridge
source install/setup.bash
```

## 4. Lancer

```bash
ros2 launch stm32_can_bridge bridge_launch.py x:=1.5 y:=2.3 theta:=0.78
```

Ce lancement demarre :
- `can_ack_listener` : ecoute en continu le bus CAN et publie sur `/position_status`
  des qu'il recoit l'ID `0x200` avec l'octet `0xAA` (accuse "position atteinte").
- `can_position_publisher` : envoie une seule fois la trame `0x100` contenant
  x, y, theta (encodes en int16 * 100), puis se termine.

Verifier la reception cote ROS2 :

```bash
ros2 topic echo /position_status
```

## 5. Format des trames CAN

| ID CAN | Emetteur | Contenu |
|--------|----------|---------|
| 0x100  | RPi5 -> STM32 | 6 octets utiles : x (int16 LE), y (int16 LE), theta (int16 LE), valeurs reelles x100 |
| 0x200  | STM32 -> RPi5 | 1 octet : 0xAA = position atteinte |

## 6. Depannage

- Rien ne passe sur `candump can0` : verifier terminaison 120 ohms et GND commun.
- `ip link show can0` indique `DOWN` : refaire `sudo ip link set can0 up type can bitrate 500000`.
- Verifier que le bitrate cote STM32 (CubeMX) est bien aussi 500 kbit/s.
