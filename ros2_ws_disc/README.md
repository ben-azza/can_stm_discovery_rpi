# Workspace ROS2 — Pont CAN RPi5 <-> STM32F4 Discovery

Ce package fonctionne avec le firmware STM32F4 Discovery (CAN sur PD0/PD1,
affichage via USB CDC). Le protocole CAN est identique a la version Nucleo :
seul le microcontroleur cote STM32 a change, pas le format des trames.

## 1. Protocole des trames CAN (rappel)

| ID CAN | Emetteur | Contenu |
|--------|----------|---------|
| 0x100  | RPi5 -> STM32 | 6 octets utiles : x, y, theta en int16 little-endian, valeur reelle x100 |
| 0x150  | RPi5 -> STM32 | Texte libre ASCII, decoupe en trames de 8 octets max, terminee par un octet nul |
| 0x200  | STM32 -> RPi5 | 1 octet : `0x01` = position atteinte |

**Important** : le firmware F4 Discovery envoie `0x01` comme octet d'accuse de
reception (pas `0xAA`). Le launch file (`bridge_launch.py`) est deja configure
avec `ack_byte: 1` pour correspondre. Si tu ecris ton propre lancement, pense
a passer ce parametre.

## 2. Prerequis materiel (RPi5)

- CAN HAT (MCP2515) monte sur le RPi5
- Bus CAN cable vers le module SN65HVD230, lui-meme cable sur **PD0/PD1**
  de la STM32F4 Discovery (pas PA11/PA12, occupees par l'USB OTG)
- Terminaison 120 ohms aux deux extremites du bus
- GND commun entre RPi5 et F4 Discovery

## 3. Activer l'interface CAN sur le RPi5

```bash
sudo nano /boot/firmware/config.txt
```
Ajouter :
```
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
```
Puis :
```bash
sudo reboot
sudo apt install can-utils python3-pip -y
pip3 install python-can --break-system-packages

sudo ip link set can0 up type can bitrate 500000
sudo ip link set can0 txqueuelen 1000
ip -details link show can0
```

## 4. Compiler le package ROS2

```bash
cd ~/project/ros2_ws
colcon build --packages-select stm32_can_bridge
source install/setup.bash
```

## 5. Lancer

### a) Envoyer une position et attendre l'accuse de reception

```bash
ros2 launch stm32_can_bridge bridge_launch.py x:=1.5 y:=2.3 theta:=0.78
```

Verifier la reception cote ROS2 :
```bash
ros2 topic echo /position_status
```

### b) Envoyer du texte libre (interactif)

```bash
ros2 run stm32_can_bridge can_string_sender
```
Puis taper un message et appuyer sur Entree. Taper `quit` pour arreter.

Sur le terminal serie de la F4 Discovery (via USB CDC, `/dev/ttyACM0` sur le
connecteur OTG FS, pas le ST-Link), le message apparait :
```
Message recu du RPi5 : "ton message ici"
```

## 6. Voir les logs du STM32 (USB CDC, pas UART)

Contrairement a la Nucleo, la F4 Discovery n'a pas de port serie integre au
ST-Link. L'affichage passe par le port **OTG FS** (connecteur micro-USB CN5,
different du connecteur ST-Link CN1 utilise pour le flash) :

```bash
ls /dev/ttyACM*
sudo apt install screen -y
sudo screen /dev/ttyACM0 115200
```

## 7. Depannage

- Rien ne passe sur `candump can0` : verifier terminaison 120 ohms, GND commun,
  et que le cablage utilise bien PD0/PD1 (pas PA11/PA12) sur la Discovery.
- `/dev/ttyACM0` absent : verifier que le cable est branche sur **CN5** (OTG FS)
  et non CN1 (ST-Link), et que `Class For FS IP = Communication Device Class`
  a bien ete selectionne dans CubeMX (pas Audio Device Class).
- `ack_listener` ne detecte jamais l'ACK alors qu'il est bien envoye : verifier
  que le parametre `ack_byte` cote ROS2 correspond a l'octet envoye par le
  firmware (`0x01` par defaut pour la F4 Discovery).
- `ModuleNotFoundError: No module named 'can'` : `pip3 install python-can --break-system-packages`
