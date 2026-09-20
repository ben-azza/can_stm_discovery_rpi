# Configuration CubeMX - Nucleo L476RG - CAN1 + UART2

## 1. Nouveau projet
- CubeMX / CubeIDE -> New Project -> Board Selector -> **NUCLEO-L476RG**
- Accepter l'initialisation par defaut des pins (LED, bouton, UART2 pour le ST-Link).

## 2. Pinout CAN1
Le L476RG (LQFP64) route CAN1 sur :
- **PA11** = CAN1_RX
- **PA12** = CAN1_TX

Dans l'onglet Pinout :
1. Activer le peripherique **CAN1** (colonne de gauche, Connectivity).
2. Mode : **Master** (mode normal, pas loopback).
3. Verifier que PA11/PA12 sont bien assignes (CubeMX le fait automatiquement).

> Remarque materielle : PA11/PA12 sont aussi utilises par l'USB OTG sur
> certaines cartes -- sur la Nucleo L476RG standard il n'y a pas de conflit.
> Il te faut un **module transceiver CAN externe** (SN65HVD230, MCP2551,
> TJA1050...) entre CAN1_TX/RX du STM32 et le bus CAN_H/CAN_L, car le
> bxCAN interne ne sort que des niveaux logiques 3.3V, pas le signal
> differentiel du bus.

## 3. Parametres CAN1 (onglet Configuration -> CAN1)

### Prescaler / bit timing pour obtenir 500 kbit/s
Avec un HCLK/APB1 typique a 80 MHz sur cette carte (a verifier dans Clock
Configuration), un jeu de parametres courant pour 500 kbps est :

| Parametre                  | Valeur |
|-----------------------------|--------|
| Prescaler                   | 10     |
| Time Quantum in Bit Segment 1 | 13   |
| Time Quantum in Bit Segment 2 | 2    |
| ReSynchronization Jump Width | 1     |
| Mode                         | Normal |

CubeMX affiche en bas de l'onglet le **bitrate resultant calcule** -> ajuste
le prescaler jusqu'a lire exactement **500,000 bits/s**. C'est CRITIQUE :
le bitrate doit correspondre AU BIT PRES a celui configure sur le RPi5
(`ip link set can0 up type can bitrate 500000`).

### Autres options
- Time Triggered Communication : **Disable**
- Automatic Bus-Off Management : **Enable** (redemarre tout seul en cas de bus-off)
- Automatic Wake-Up Mode : Disable
- Automatic Retransmission : **Enable**
- Receive FIFO Locked mode : Disable
- Transmit FIFO Priority : Disable

## 4. Interruptions (onglet NVIC)
Cocher :
- **CAN1 RX0 interrupt** -> Enable
- (optionnel) CAN1 SCE interrupt -> Enable, utile pour detecter les erreurs bus-off

## 5. UART2 (deja configure par defaut avec la board Nucleo)
- Sert a la fois de console de debug (`printf`) via le ST-Link virtual COM port.
- Baudrate par defaut 115200, 8N1 -- laisser tel quel.
- Terminal serie a utiliser sur le PC : minicom / PuTTY / Tera Term sur le
  port `/dev/ttyACM0` (Linux) ou `COMx` (Windows), 115200 bauds.

## 6. Horloge (Clock Configuration)
- Configurer le systeme pour tourner au maximum (80 MHz) via le PLL sur le
  HSE (8 MHz du ST-Link) -- c'est la configuration par defaut proposee par
  CubeMX pour cette carte, il suffit de cliquer "Resolve Clock Issues" si
  un warning apparait.
- Bien noter la frequence APB1 (souvent 80 MHz) car c'est elle qui sert de
  base au calcul du bit timing CAN1 ci-dessus.

## 7. Generation du code
- Project Manager -> Toolchain/IDE : STM32CubeIDE (ou Makefile si tu utilises
  un autre environnement).
- Generate Code.
- Remplacer le contenu de `Core/Src/main.c` par le fichier `main.c` fourni
  a cote de ce document (il reprend les sections USER CODE generees par
  CubeMX, a fusionner avec ton fichier genere si tu regeneres plus tard).
