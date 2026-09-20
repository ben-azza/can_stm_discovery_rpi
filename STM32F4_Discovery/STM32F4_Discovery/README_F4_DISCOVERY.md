# Migration vers STM32F4 Discovery — Guide complet A a Z

## 1. Difference cles avec la Nucleo L476RG

| Aspect | Nucleo L476RG | F4 Discovery |
|---|---|---|
| CAN1 RX/TX | PA11 / PA12 | **PD0 / PD1** (PA11/PA12 = USB OTG, occupees) |
| Port serie vers PC | Integre (ST-Link VCP, /dev/ttyACM0) | **Absent** : necessite un adaptateur USB-TTL externe (/dev/ttyUSB0) |
| LED utilisee | LD2 sur PA5 | **LD4 (verte) sur PD12** |
| Horloge | HSI 16MHz -> 80MHz, PCLK1=40 ou 80MHz | HSE 8MHz -> 168MHz, PCLK1=**42MHz max** |
| Prescaler CAN (500kbit/s) | 5 ou 10 selon PCLK1 | **6** (avec PCLK1=42MHz, BS1=11TQ, BS2=2TQ) |

## 2. Etapes CubeMX (dans l'ordre)

1. `File > New > STM32 Project` > Board Selector > chercher `STM32F4DISCOVERY` (ou DISC1)
2. **Clock Configuration** : verifier HSE actif, PLL -> SYSCLK 168MHz, APB1 /4 -> PCLK1 42MHz, APB2 /2 -> PCLK2 84MHz (bouton "Resolve Clock Issues" si besoin)
3. **Connectivity > CAN1** : Activate. Puis dans la vue Pinout, forcer manuellement :
   - PD0 -> CAN1_RX
   - PD1 -> CAN1_TX
   (PA11/PA12 seront grisees ou a laisser en GPIO normal / desactivees)
4. Parameter Settings CAN1 :
   - Prescaler = 6
   - Time Quantum BS1 = 11 Times
   - Time Quantum BS2 = 2 Times
   - SJW = 1 Time
   - Mode = Normal
5. NVIC : cocher CAN1 RX0 interrupt
6. **Connectivity > USART2** : Mode Asynchronous (PA2/PA3), 115200 bauds 8N1
7. Verifier que PD12 (LD4) est en GPIO_Output (normalement deja fait par le Board Selector)
8. `Project > Generate Code`

## 3. Cablage physique

### SN65HVD230 <-> F4 Discovery

| SN65HVD230 | F4 Discovery |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CTX (D) | PD1 |
| CRX (R) | PD0 |
| CANH/CANL | vers le bus CAN (terminaison 120 ohms), relie au CAN HAT du RPi5 |

### Adaptateur USB-TTL <-> F4 Discovery (obligatoire pour voir les printf)

| Adaptateur USB-TTL | F4 Discovery |
|---|---|
| TX | PA3 |
| RX | PA2 |
| GND | GND |

Une fois branche en USB sur le PC/RPi5, il apparait comme `/dev/ttyUSB0` (Linux) et non `/dev/ttyACM0`.

Test :
```bash
ls /dev/ttyUSB*
sudo screen /dev/ttyUSB0 115200
```

## 4. Integrer le code applicatif

Voir le fichier `code_applicatif_F4.c` fourni a cote de ce README : il contient tout le code
a coller dans les zones USER CODE du `main.c` genere par CubeIDE (memes zones que pour
la Nucleo : Includes, PV, PFP, 0, 2, WHILE).

Points a verifier apres generation CubeMX (indiques aussi en bas du fichier) :
- `MX_CAN1_Init()` doit avoir Prescaler=6, TimeSeg1=CAN_BS1_11TQ, TimeSeg2=CAN_BS2_2TQ
- `HAL_CAN_MspInit()` dans `stm32f4xx_hal_msp.c` doit configurer PD0/PD1 en GPIO_AF9_CAN1,
  PAS PA11/PA12

## 5. Flag linker pour printf flottant

`Project > Properties > C/C++ Build > Settings > Tool Settings > MCU GCC Linker > Miscellaneous > Other flags` :
```
-u _printf_float
```

## 6. Fonctionnalites incluses dans ce code

- Reception d'une position (x, y, theta) sur ID CAN 0x100, affichage, attente 5s non bloquante,
  puis envoi d'un accuse de reception sur ID CAN 0x200 (octet 0x01) et allumage LED LD4 (PD12)
- Reception de chaines de caracteres libres sur ID CAN 0x150 (protocole multi-trames avec
  terminateur nul), affichees directement sur le terminal serie

## 7. Compilation et flash

1. `Project > Build Project`
2. Brancher la carte en USB (port ST-Link de la Discovery, different du port utilisateur si present)
3. **Run** ou **Debug** pour flasher

## 8. Test complet

```bash
# Cote RPi5 : verifier le bus CAN
ip -details link show can0

# Terminal serie STM32 (adaptateur USB-TTL)
sudo screen /dev/ttyUSB0 115200

# Lancer ROS2 (nouveau terminal)
cd ~/project/ros2_ws
source install/setup.bash
ros2 launch stm32_can_bridge bridge_launch.py x:=1.5 y:=2.3 theta:=0.78

# Tester l'envoi de texte libre (nouveau terminal)
ros2 run stm32_can_bridge can_string_sender
```

Resultat attendu dans le terminal serie F4 Discovery :
```
=== STM32F4 Discovery pret. En attente de commandes CAN ===
Position recue : x=1.500 m, y=2.300 m, theta=0.78 deg
Deplacement en cours... attente de 5s
Message envoye au RPi5 : "la position est atteinte"
Message recu du RPi5 : "bonjour stm32"
```

La LED verte LD4 (PD12) s'allume au moment de l'envoi de l'accuse de reception.
