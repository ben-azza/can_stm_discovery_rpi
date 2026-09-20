# Projet STM32 - Nucleo L476RG - reception CAN depuis ROS2/RPi5

## Fichiers fournis
- `CONFIGURATION_CUBEMX.md` : configuration pas-a-pas de CAN1 et USART2 dans CubeMX/CubeIDE.
- `main.c` : logique complete (reception CAN, affichage UART, attente 5s non bloquante, envoi ACK).
- `init_functions.c` : `MX_CAN1_Init()`, `MX_USART2_UART_Init()` et `HAL_CAN_MspInit()` de reference,
  a comparer/fusionner avec ce que CubeMX genere automatiquement.

## Matos necessaire
- Nucleo L476RG
- Un transceiver CAN externe (SN65HVD230 / TJA1050 / MCP2551...) entre PA11/PA12
  (niveaux logiques 3.3V) et le bus CAN_H/CAN_L physique.
- Resistances de terminaison 120 ohm aux DEUX extremites physiques du bus CAN
  (une seule est peut-etre deja integree au HAT Waveshare cote RPi5 -- verifie
  avec un multimetre que la resistance mesuree entre CAN_H et CAN_L, bus au
  repos et alimente, fait environ 60 ohm = deux 120 ohm en parallele).

## Etapes
1. Suivre `CONFIGURATION_CUBEMX.md` pour creer/configurer le projet dans CubeIDE.
2. Generer le code.
3. Copier le contenu de `main.c` fourni ici dans le `main.c` genere (en
   respectant les sections `USER CODE BEGIN/END` si tu comptes regenerer
   plus tard depuis le fichier `.ioc`).
4. Verifier que `MX_CAN1_Init()` et `MX_USART2_UART_Init()` generes
   correspondent bien a `init_functions.c` (notamment le bitrate CAN).
5. Flasher via le ST-Link integre a la Nucleo (bouton "Run" de CubeIDE,
   ou `st-flash` en ligne de commande).
6. Ouvrir un terminal serie sur le port du ST-Link (115200 8N1) pour voir
   les logs `printf`.

## Verification independante de ROS2
Avant de brancher le RPi5, tu peux tester le STM32 seul avec un adaptateur
USB-CAN ou un deuxieme module MCP2515 + `candump`/`cansend` :
```bash
cansend can0 100#DC0520039411 0000   # x=1500mm, y=800mm, theta=4500 (45.00 deg)
candump can0                          # doit afficher la trame 200#01 5 secondes plus tard
```
