# Projet : Communication RPi5 <-> STM32 Nucleo L476RG via bus CAN + ROS2

## 1. Vue d'ensemble de l'architecture

```
RPi5 (SPI) --- MCP2515 (sur le CAN HAT) --- transceiver du HAT --- CANH/CANL ---┐
                                                                                  ├── Bus CAN (120 ohms a chaque bout)
STM32 Nucleo L476RG (PA11/PA12, bxCAN) --- SN65HVD230 --- CANH/CANL ------------┘
```

### Role de chaque composant (a bien distinguer)

| Composant | Type | Role |
|---|---|---|
| **bxCAN** (integre au STM32) | Controleur CAN (logique) | Gere les trames, filtres, interruptions. Communique en logique 3.3V sur PA11 (RX) / PA12 (TX). |
| **SN65HVD230** | Transceiver CAN (etage physique) | Traduit la logique 3.3V du bxCAN vers le signal differentiel CANH/CANL du bus. Ce n'est **ni** un controleur CAN a lui seul, **ni** de l'UART. |
| **MCP2515** (sur le CAN HAT du RPi5) | Controleur CAN + transceiver integre | Equivalent cote RPi5 : le RPi5 ne parle pas CAN nativement, le MCP2515 s'en charge, relie en SPI. |
| **USART2** (integre au STM32) | Port serie de debug | **Sans rapport avec le bus CAN.** Sert uniquement a afficher les `printf()` sur un terminal, via le port USB du ST-Link (PA2/PA3). |

Chaine complete : `STM32 (bxCAN logique) -> SN65HVD230 (traducteur physique) -> bus CAN -> MCP2515 (HAT) -> RPi5`

## 2. Cablage physique

### SN65HVD230 <-> Nucleo L476RG

| Broche SN65HVD230 | Broche Nucleo L476RG | Role |
|---|---|---|
| VCC | 3V3 | Alimentation 3.3V |
| GND | GND | Masse (voir point critique ci-dessous) |
| CTX (ou D) | PA12 | CAN1_TX |
| CRX (ou R) | PA11 | CAN1_RX |
| CANH / CANL | - | Vers le bus, relies au CAN HAT du RPi5 |

### Points critiques de cablage

- **GND commun obligatoire** entre le RPi5 et la Nucleo (fil de masse en plus du CANH/CANL).
- **Terminaison 120 ohms aux DEUX extremites du bus** (ni zero, ni plus de deux) : generalement un jumper/switch sur le HAT et un autre sur le module SN65HVD230.
- Ne pas inverser CANH et CANL avec CTX/CRX : ce sont deux paires differentes.
  - CTX/CRX (ou D/R) = cote logique 3.3V -> vers le microcontroleur
  - CANH/CANL = cote bus differentiel -> vers le cable partage

## 3. Protocole applicatif (format des trames CAN)

| ID CAN | Emetteur | Contenu |
|---|---|---|
| 0x100 | RPi5 -> STM32 | 6 octets utiles : x, y, theta en int16 little-endian, valeur reelle x 100 |
| 0x200 | STM32 -> RPi5 | 1 octet : `0x01` = position atteinte |

Le code applicatif :
1. Le RPi5 envoie une position (x, y, theta) une fois au lancement.
2. Le STM32 recoit, affiche la position sur son terminal serie (USART2), attend 5 secondes (non bloquant), puis renvoie l'accuse de reception et allume la LED LD2.
3. Le RPi5 recoit l'accuse et publie "La position est atteinte" sur le topic ROS2 `/position_status`.

## 4. Configuration CubeMX (recapitulatif)

- **Horloge** : HSI 16MHz -> PLL -> SYSCLK 80MHz, PCLK1 = 80MHz (APB1CLKDivider = DIV1)
- **CAN1** (Connectivity > CAN1) :
  - Activate, Mode Master (assigne PA11/PA12 automatiquement)
  - Prescaler = 10, BS1 = 13TQ, BS2 = 2TQ, SJW = 1TQ, Mode Normal
  - Verification : PCLK1(80MHz) / Prescaler(10) = 8MHz de quantum ; 8MHz / (1+13+2) = **500 kHz** -> bitrate 500 kbit/s
  - NVIC : cocher CAN1 RX0 interrupt
- **USART2** : deja actif par defaut sur Nucleo (115200 bauds, 8N1, relie au ST-Link)
- **Linker flag obligatoire** pour l'affichage des flottants avec printf :
  `Project > Properties > C/C++ Build > Settings > Tool Settings > MCU GCC Linker > Miscellaneous > Other flags` :
  ```
  -u _printf_float
  ```

## 5. Toutes les commandes — de zero jusqu'au lancement

### 5.1 Mise a jour du systeme RPi5

```bash
sudo apt update && sudo apt upgrade -y
```

### 5.2 Activer le SPI et le driver MCP2515

```bash
sudo nano /boot/firmware/config.txt
```
Ajouter a la fin (adapter `oscillator=` et `interrupt=` a la doc de VOTRE HAT) :
```
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
```
Sauvegarder (`Ctrl+O`, `Entree`, `Ctrl+X`), puis :
```bash
sudo reboot
```

Verifier apres redemarrage :
```bash
ip link show can0
```

### 5.3 Outils CAN et Python

```bash
sudo apt install can-utils -y
sudo apt install python3-pip -y
pip3 install python-can --break-system-packages
```

### 5.4 Activer l'interface CAN (a chaque boot, sauf service systemd)

```bash
sudo ip link set can0 up type can bitrate 500000
sudo ip link set can0 txqueuelen 1000
ip -details link show can0
```

### 5.5 Test bas niveau du bus (optionnel mais recommande)

Terminal A :
```bash
candump can0
```
Terminal B :
```bash
cansend can0 100#1122334455667788
```

### 5.6 Installer ROS2 (si pas deja fait)

```bash
sudo apt install ros-jazzy-desktop -y
source /opt/ros/jazzy/setup.bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
sudo apt install python3-colcon-common-extensions -y
```
*(adapter `jazzy` a votre distribution ROS2 : verifier avec `ls /opt/ros/`)*

### 5.7 Placer et compiler le package ROS2

```bash
mkdir -p ~/project/ros2_ws/src
cd ~/project/ros2_ws/src
# copier/decompresser ici le dossier stm32_can_bridge

cd ~/project/ros2_ws
colcon build --packages-select stm32_can_bridge
source install/setup.bash
```

### 5.8 Ouvrir le terminal serie du STM32 (voir les printf)

```bash
lsusb
ls /dev/ttyACM*
sudo apt install screen -y
sudo screen /dev/ttyACM0 115200
```
Quitter `screen` : `Ctrl+A` puis `K` puis `y`.

Si `/dev/ttyACM*` n'existe pas :
- Verifier que le cable USB est un cable de **donnees** (pas juste charge)
- Verifier qu'il est branche sur le port **ST-Link** de la Nucleo
- Verifier la LED verte PWR allumee
- Debrancher/rebrancher, puis refaire `lsusb`

### 5.9 Lancer le projet complet

```bash
cd ~/project/ros2_ws
source install/setup.bash
ros2 launch stm32_can_bridge bridge_launch.py x:=1.5 y:=2.3 theta:=0.78
```

### 5.10 Verifier la reception cote ROS2

```bash
ros2 topic echo /position_status
```

## 6. Automatiser l'activation du bus CAN au demarrage (optionnel)

Pour eviter de retaper `sudo ip link set can0 up` a chaque redemarrage :

```bash
sudo nano /etc/systemd/system/can0-up.service
```

Contenu :
```ini
[Unit]
Description=Activation du bus CAN0 au demarrage
After=network.target

[Service]
Type=oneshot
ExecStart=/sbin/ip link set can0 up type can bitrate 500000
ExecStartPost=/sbin/ip link set can0 txqueuelen 1000
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Activer :
```bash
sudo systemctl daemon-reload
sudo systemctl enable can0-up.service
sudo systemctl start can0-up.service
```

## 7. Cote STM32 (une seule fois, sur le PC avec CubeIDE)

1. Installer STM32CubeIDE : https://www.st.com/en/development-tools/stm32cubeide.html
2. Ouvrir le projet, verifier la config CAN1 + USART2 dans le `.ioc`
3. Ajouter le flag linker `-u _printf_float` (voir section 4)
4. `Project > Build Project`
5. Brancher la Nucleo en USB, cliquer sur **Run** ou **Debug** pour flasher

## 8. Fonctionnalite LED (allumage apres les 5 secondes)

Dans `Envoyer_Trame_Ack()`, apres l'envoi reussi de la trame CAN :
```c
HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
```

Dans `Traiter_Trame_Commande()`, au debut, pour reinitialiser l'etat a chaque nouvelle commande :
```c
HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
```

Comportement resultant :
1. Reception commande -> LED eteinte, affichage position, minuteur 5s demarre
2. Apres 5s -> LED allumee, envoi de l'ACK au RPi5
3. Nouvelle commande -> LED s'eteint, cycle recommence

## 9. Resultat attendu (sortie complete)

**Dans `candump can0`** :
```
can0  100   [8]  2A 05 CE 08 32 00 00 00
can0  200   [1]  01
```

**Dans le terminal serie STM32 (`screen /dev/ttyACM0 115200`)** :
```
=== STM32 pret. En attente de commandes CAN (ID 0x100) ===
Position recue : x=1.500 m, y=2.300 m, theta=0.78 deg
Deplacement en cours... attente de 5s
Message envoye au RPi5 : "la position est atteinte"
```
(La LED LD2 s'allume au moment de ce dernier message.)

**Dans le terminal ROS2** :
```
[can_position_publisher-2]: Position envoyee sur ID=0x100 : x=1.50 y=2.30 theta=0.78
[can_ack_listener-1]: La position est atteinte
```

**Avec `ros2 topic echo /position_status`** :
```
data: La position est atteinte
```

## 10. Depannage

| Symptome | Cause probable | Solution |
|---|---|---|
| `ip link show can0` : interface absente | Overlay MCP2515 mal configure | Verifier `oscillator=` et `interrupt=` dans `/boot/firmware/config.txt` selon la doc du HAT |
| `RTNETLINK answers: Operation not permitted` | Commande sans `sudo` | Ajouter `sudo` devant `ip link set ...` |
| `RTNETLINK answers: Device or resource busy` | Interface deja montee | Verifier avec `ip -details link show can0`, pas forcement une erreur |
| `can state ERROR-PASSIVE` | Aucun autre noeud sur le bus (STM32 pas branche/alimente) | Normal si seul sur le bus ; brancher/alimenter la Nucleo |
| `can state ERROR-WARNING` (mais trames passent) | Terminaison 120 ohms incorrecte, CANH/CANL inverses, ou GND non commun | Verifier cablage physique point par point |
| `ModuleNotFoundError: No module named 'can'` | `python-can` non installe dans l'environnement utilise par ROS2 | `pip3 install python-can --break-system-packages` |
| Erreur linker "float formatting support is not enabled" | Flag manquant pour printf flottant | Ajouter `-u _printf_float` dans MCU GCC Linker > Miscellaneous |
| `/dev/ttyACM0` absent | Cable USB non-data, mauvais port, ou carte non alimentee | Verifier cable, port ST-Link, LED PWR ; debrancher/rebrancher |
| ACK jamais recu cote ROS2 alors que le STM32 l'envoie | Octet ACK differe entre STM32 (`0x01`) et parametre ROS2 (`ack_byte`) | Harmoniser la valeur des deux cotes (voir section 3) |

## 11. Recapitulatif du role CAN vs UART (question frequente)

- Le **SN65HVD230 est un transceiver CAN**, pas de l'UART. Il ne fait que convertir le signal logique CAN du bxCAN (integre au STM32) en signal differentiel CANH/CANL pour le bus physique.
- L'**UART (USART2)** est un peripherique totalement independant du CAN, utilise uniquement pour afficher les messages de debug sur un terminal, via le port USB du ST-Link. Il n'a aucun role dans la communication CAN elle-meme.
