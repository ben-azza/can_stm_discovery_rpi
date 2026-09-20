# STM32F4 Discovery — Configuration CubeMX + Code complet

Communication CAN avec un RPi5 (position x,y,theta + texte libre), affichage
via port USB CDC integre (pas d'adaptateur externe necessaire).

## 1. Identification de la carte

Carte : **STM32F407G-DISC1** (STM32F4DISCOVERY), microcontroleur **STM32F407VGT6**.

Deux connecteurs USB distincts sur cette carte :
- **CN1** (mini-USB) : ST-Link, sert uniquement a la programmation/debug
- **CN5** (micro-USB) : port OTG FS du STM32F407, utilise ici comme port serie virtuel (USB CDC)

## 2. Pourquoi PA11/PA12 ne sont pas utilisables pour CAN

Sur cette carte, PA11/PA12 sont deja cablees en dur vers le connecteur USB OTG FS (CN5).
On utilise donc **PD0/PD1** pour le bus CAN a la place.

## 3. Configuration CubeMX — pas a pas

### 3.1 Creation du projet
1. `File > New > STM32 Project`
2. Board Selector > rechercher `STM32F4DISCOVERY` (ou `DISC1`)
3. Nommer le projet, `Finish`, accepter l'initialisation par defaut des peripheriques

### 3.2 Clock Configuration
- HSE actif (8 MHz, cristal externe de la carte)
- PLL configure pour **SYSCLK = 168 MHz**
- **APB1 Prescaler = /4** -> **PCLK1 = 42 MHz** (maximum autorise sur le F407)
- APB2 Prescaler = /2 -> PCLK2 = 84 MHz
- Utiliser le bouton "Resolve Clock Issues" si CubeMX signale une incoherence

### 3.3 CAN1 (Connectivity > CAN1)
1. Cocher **Activate**
2. Dans la vue Pinout (graphique du chip), cliquer manuellement :
   - **PD0** -> selectionner `CAN1_RX`
   - **PD1** -> selectionner `CAN1_TX`
   (Ne pas laisser CubeMX assigner PA11/PA12 automatiquement — les forcer en GPIO normal
   ou Reset_State si elles sont proposees par defaut)

Parameter Settings :
| Parametre | Valeur |
|---|---|
| Prescaler | 6 |
| Time Quantum in Bit Segment 1 | 11 Times |
| Time Quantum in Bit Segment 2 | 2 Times |
| Resynchronization Jump Width | 1 Time |
| Mode | Normal |

Verification du calcul : PCLK1(42MHz) / Prescaler(6) = 7MHz de quantum.
7MHz / (1 + 11 + 2) = **500 kHz** -> bitrate CAN = 500 kbit/s (point d'echantillonnage 85.7%)

NVIC Settings : cocher **CAN1 RX0 interrupt**

### 3.4 USB_OTG_FS (Connectivity > USB_OTG_FS)
- Mode = **Device_Only**
- **Activate_VBUS** coche
- Speed reste "Device Full Speed 12MBit/s" (valeur par defaut)

### 3.5 USB_DEVICE (Middleware and Software Packs > USB_DEVICE)
- **Class For FS IP = `Communication Device Class (Virtual Port Com)`**

  ⚠️ Attention : CubeMX propose parfois par defaut `Audio Device Class` a cause du
  codec audio integre a la carte (CS43L22). Il FAUT changer cette valeur pour CDC,
  sinon le port serie virtuel ne fonctionnera pas.

### 3.6 GPIO — LED
Verifier que **PD12** (LD4, LED verte) est en mode `GPIO_Output` (normalement deja
configure par le Board Selector).

### 3.7 USART2 (optionnel)
Comme l'affichage passe desormais par USB CDC, USART2 n'est plus necessaire. Tu peux
le laisser active sans consequence, ou le desactiver pour simplifier le projet.

### 3.8 Generer le code
`Project > Generate Code`. Un nouveau dossier **USB_DEVICE/** doit apparaitre dans
l'arborescence, contenant notamment `usbd_cdc_if.c`, `usbd_conf.c`, `usb_device.c`.

### 3.9 Flag linker pour printf flottant
`Project > Properties > C/C++ Build > Settings > Tool Settings > MCU GCC Linker >
Miscellaneous > Other flags` :
```
-u _printf_float
```

## 4. Verification apres generation

Dans le `main.c` genere, verifier que **`MX_USB_DEVICE_Init();`** est bien appelee
dans `main()`, avant les zones `USER CODE BEGIN 2`. L'ordre typique genere par
CubeMX est :
```c
MX_GPIO_Init();
MX_CAN1_Init();
MX_USART2_UART_Init();   /* si laisse actif */
MX_USB_DEVICE_Init();
```

Dans `MX_CAN1_Init()`, verifier :
```c
hcan1.Init.Prescaler = 6;
hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
```

Dans `stm32f4xx_hal_msp.c`, fonction `HAL_CAN_MspInit()`, verifier que le GPIO
configure est bien **GPIOD**, broches **PIN_0 et PIN_1** avec `GPIO_AF9_CAN1`.

## 5. Protocole des trames CAN

| ID CAN | Emetteur | Contenu |
|---|---|---|
| 0x100 | RPi5 -> STM32 | 6 octets utiles : x, y, theta en int16 little-endian, valeur reelle x100 |
| 0x150 | RPi5 -> STM32 | Texte libre ASCII, decoupe en trames de 8 octets max, terminee par un octet nul |
| 0x200 | STM32 -> RPi5 | 1 octet : `0x01` = position atteinte |

## 6. Cablage physique

### SN65HVD230 <-> STM32F4 Discovery

| SN65HVD230 | F4 Discovery |
|---|---|
| VCC | 3V3 |
| GND | GND (commun avec le RPi5 obligatoire) |
| CTX (D) | PD1 |
| CRX (R) | PD0 |
| CANH / CANL | vers le bus CAN (terminaison 120 ohms), relie au CAN HAT du RPi5 |

### USB
- Cable micro-USB sur **CN5** (OTG FS) -> vers RPi5 ou PC, pour l'affichage
- Cable mini-USB sur **CN1** (ST-Link) -> pour flasher le firmware

## 7. Code complet a integrer dans main.c

Coller chaque bloc ci-dessous dans la zone `USER CODE` correspondante du `main.c`
genere par CubeIDE (les zones existent deja, generees automatiquement).

### USER CODE BEGIN Includes
```c
#include <stdio.h>
#include <string.h>
#include "usbd_cdc_if.h"

extern CAN_HandleTypeDef hcan1;
extern USBD_HandleTypeDef hUsbDeviceFS;
```

### USER CODE BEGIN PV
```c
#define ID_CMD    0x100U   /* consigne de position, recue du RPi5 */
#define ID_ACK    0x200U   /* accuse "position atteinte", envoye au RPi5 */
#define ID_STRING 0x150U   /* message texte libre, recu du RPi5 */

#define STRING_BUF_SIZE 64U
#define DUREE_ATTENTE_MS 5000U

static volatile uint8_t nouvelleCommandeRecue = 0;
static uint8_t rxDataBuffer[8];

static uint8_t stringRxBuffer[STRING_BUF_SIZE];
static uint16_t stringRxIndex = 0;
static volatile uint8_t nouvelleChaineRecue = 0;

typedef enum {
    ETAT_ATTENTE_COMMANDE = 0,
    ETAT_TEMPORISATION,
    ETAT_ENVOI_ACK
} EtatRobot_t;

static EtatRobot_t etat = ETAT_ATTENTE_COMMANDE;
static uint32_t horodatageDebutAttente = 0;
```

### USER CODE BEGIN PFP
```c
static void Traiter_Trame_Commande(uint8_t *data);
static void Envoyer_Trame_Ack(void);
static void Configurer_Filtre_CAN(void);

/* Redirection de printf vers l'USB CDC (port OTG FS, connecteur micro-USB CN5) */
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    uint32_t essais = 0;

    while (CDC_Transmit_FS(&c, 1) == USBD_BUSY)
    {
        essais++;
        if (essais > 10000)
        {
            break;  /* evite un blocage infini si rien n'est branche */
        }
    }
    return ch;
}
```

### USER CODE BEGIN 0
```c
/**
 * Callback HAL appele automatiquement des qu'une trame CAN
 * est deposee dans le FIFO0. Filtre configure en "accepte tout",
 * on distingue les IDs ici.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
    {
        return;
    }

    if (rxHeader.StdId == ID_CMD)
    {
        memcpy(rxDataBuffer, data, sizeof(rxDataBuffer));
        nouvelleCommandeRecue = 1;
    }
    else if (rxHeader.StdId == ID_STRING)
    {
        for (uint8_t i = 0; i < rxHeader.DLC; i++)
        {
            if (data[i] == '\0')
            {
                if (stringRxIndex < STRING_BUF_SIZE)
                {
                    stringRxBuffer[stringRxIndex] = '\0';
                }
                else
                {
                    stringRxBuffer[STRING_BUF_SIZE - 1] = '\0';
                }
                nouvelleChaineRecue = 1;
                stringRxIndex = 0;
                return;
            }

            if (stringRxIndex < (STRING_BUF_SIZE - 1))
            {
                stringRxBuffer[stringRxIndex++] = data[i];
            }
        }
    }
}

static void Traiter_Trame_Commande(uint8_t *data)
{
    int16_t x_mm, y_mm, theta_mdeg;

    /* Eteindre la LED : on recommence un nouveau deplacement */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);

    memcpy(&x_mm,       &data[0], 2);
    memcpy(&y_mm,       &data[2], 2);
    memcpy(&theta_mdeg, &data[4], 2);

    printf("Position recue : x=%.3f m, y=%.3f m, theta=%.2f deg\r\n",
           x_mm / 1000.0f, y_mm / 1000.0f, theta_mdeg / 100.0f);
    printf("Deplacement en cours... attente de 5s\r\n");
}

static void Envoyer_Trame_Ack(void)
{
    CAN_TxHeaderTypeDef txHeader;
    uint8_t txData[1] = {0x01};
    uint32_t txMailbox;

    txHeader.StdId = ID_ACK;
    txHeader.ExtId = 0;
    txHeader.IDE   = CAN_ID_STD;
    txHeader.RTR   = CAN_RTR_DATA;
    txHeader.DLC   = 1;
    txHeader.TransmitGlobalTime = DISABLE;

    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
    {
        /* attente courte */
    }

    if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox) == HAL_OK)
    {
        printf("Message envoye au RPi5 : \"la position est atteinte\"\r\n");
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);  /* LED LD4 verte */
    }
    else
    {
        printf("Erreur : echec envoi trame CAN ACK\r\n");
    }
}

static void Configurer_Filtre_CAN(void)
{
    CAN_FilterTypeDef sFilterConfig = {0};

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}
```

### USER CODE BEGIN 2
(dans `main()`, apres TOUS les `MX_..._Init()`, y compris `MX_USB_DEVICE_Init()`)
```c
HAL_Delay(1500);  /* laisse le temps a l'hote d'enumerer le peripherique USB */

Configurer_Filtre_CAN();

if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
{
    Error_Handler();
}

if (HAL_CAN_Start(&hcan1) != HAL_OK)
{
    Error_Handler();
}

printf("\r\n=== STM32F4 Discovery pret (USB CDC). En attente de commandes CAN ===\r\n");
```

### USER CODE BEGIN WHILE
(a l'interieur de `while (1) { ... }`)
```c
if (nouvelleChaineRecue)
{
    nouvelleChaineRecue = 0;
    printf("Message recu du RPi5 : \"%s\"\r\n", stringRxBuffer);
}

switch (etat)
{
case ETAT_ATTENTE_COMMANDE:
    if (nouvelleCommandeRecue)
    {
        nouvelleCommandeRecue = 0;
        Traiter_Trame_Commande(rxDataBuffer);
        horodatageDebutAttente = HAL_GetTick();
        etat = ETAT_TEMPORISATION;
    }
    break;

case ETAT_TEMPORISATION:
    if ((HAL_GetTick() - horodatageDebutAttente) >= DUREE_ATTENTE_MS)
    {
        etat = ETAT_ENVOI_ACK;
    }
    break;

case ETAT_ENVOI_ACK:
    Envoyer_Trame_Ack();
    etat = ETAT_ATTENTE_COMMANDE;
    break;
}
```

## 8. Compilation et flash

1. `Project > Build Project`
2. Brancher le cable mini-USB sur **CN1** (ST-Link)
3. **Run** ou **Debug** pour flasher
4. Une fois flashe, debrancher le cable ST-Link et brancher un cable micro-USB
   sur **CN5** (OTG FS) vers le RPi5 ou le PC

## 9. Test

Cote RPi5 :
```bash
ls /dev/ttyACM*
sudo screen /dev/ttyACM0 115200
```

Resultat attendu au demarrage :
```
=== STM32F4 Discovery pret (USB CDC). En attente de commandes CAN ===
```

Puis apres reception d'une commande de position :
```
Position recue : x=1.500 m, y=2.300 m, theta=0.78 deg
Deplacement en cours... attente de 5s
Message envoye au RPi5 : "la position est atteinte"
```
(la LED verte LD4 sur PD12 s'allume a ce moment)

Et pour un message texte :
```
Message recu du RPi5 : "bonjour stm32"
```

## 10. Depannage specifique a cette configuration

| Symptome | Cause probable | Solution |
|---|---|---|
| `/dev/ttyACM0` absent | Class For FS IP reglee sur Audio au lieu de CDC | Corriger dans CubeMX, regenerer le code |
| `/dev/ttyACM0` absent | Cable branche sur CN1 au lieu de CN5 | Utiliser le bon connecteur (OTG FS) |
| Rien ne s'affiche mais le port existe | printf appele trop tot, avant enumeration USB | Verifier le `HAL_Delay(1500)` en debut de USER CODE 2 |
| Bus CAN en erreur | PD0/PD1 mal assignes, restes sur PA11/PA12 | Verifier `HAL_CAN_MspInit()` dans stm32f4xx_hal_msp.c |
| Erreur linker printf float | Flag manquant | Ajouter `-u _printf_float` dans MCU GCC Linker |
