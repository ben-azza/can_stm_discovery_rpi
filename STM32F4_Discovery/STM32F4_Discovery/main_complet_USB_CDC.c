/*
 * ============================================================================
 * CODE COMPLET A INTEGRER DANS main.c (STM32F4 Discovery, affichage USB CDC)
 * ============================================================================
 * Copier chaque bloc dans la zone USER CODE correspondante du main.c
 * genere par CubeIDE (apres avoir suivi la config CubeMX ci-dessous).
 * ============================================================================
 */

/* ========================================================================
 * USER CODE BEGIN Includes
 * ======================================================================== */
#include <stdio.h>
#include <string.h>
#include "usbd_cdc_if.h"

extern CAN_HandleTypeDef hcan1;
extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE END Includes */


/* ========================================================================
 * USER CODE BEGIN PV
 * ======================================================================== */
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
/* USER CODE END PV */


/* ========================================================================
 * USER CODE BEGIN PFP
 * ======================================================================== */
static void Traiter_Trame_Commande(uint8_t *data);
static void Envoyer_Trame_Ack(void);
static void Configurer_Filtre_CAN(void);

/* Redirection de printf vers l'USB CDC (port OTG FS, connecteur micro-USB) */
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    uint32_t essais = 0;

    while (CDC_Transmit_FS(&c, 1) == USBD_BUSY)
    {
        essais++;
        if (essais > 10000)
        {
            break;
        }
    }
    return ch;
}
/* USER CODE END PFP */


/* ========================================================================
 * USER CODE BEGIN 0
 * ======================================================================== */

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
/* USER CODE END 0 */


/* ========================================================================
 * USER CODE BEGIN 2   (dans main(), apres TOUS les MX_..._Init(),
 *                       y compris MX_USB_DEVICE_Init())
 * ======================================================================== */
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
/* USER CODE END 2 */


/* ========================================================================
 * USER CODE BEGIN WHILE   (a l'interieur de while (1) { ... })
 * ======================================================================== */
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
/* USER CODE END WHILE */
