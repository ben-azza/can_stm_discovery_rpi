/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * Nucleo L476RG - Reception CAN (x, y, theta) depuis RPi5/ROS2
 * Affiche les valeurs sur UART2 (115200 8N1), attend 5s puis renvoie
 * une trame CAN "position atteinte" (ID 0x200).
 *
 * Protocole CAN (bitrate 500 kbps, a faire correspondre cote RPi5) :
 *   ID 0x100 (RPi5 -> STM32) : x_mm(int16) | y_mm(int16) | theta_mdeg(int16) | 00 00
 *   ID 0x200 (STM32 -> RPi5) : 0x01
 ******************************************************************************
 */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <string.h>

/* ---- Handles generes par CubeMX ---- */
CAN_HandleTypeDef hcan1;
UART_HandleTypeDef huart2;

/* ---- Prototypes generes par CubeMX ---- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PV */
#define ID_CMD 0x100U   /* consigne de position, recue du RPi5 */
#define ID_ACK 0x200U   /* accuse "position atteinte", envoye au RPi5 */

static volatile uint8_t nouvelleCommandeRecue = 0;
static uint8_t rxDataBuffer[8];

/* Machine a etats pour l'attente de 5s NON bloquante */
typedef enum {
    ETAT_ATTENTE_COMMANDE = 0,
    ETAT_TEMPORISATION,
    ETAT_ENVOI_ACK
} EtatRobot_t;

static EtatRobot_t etat = ETAT_ATTENTE_COMMANDE;
static uint32_t horodatageDebutAttente = 0;
#define DUREE_ATTENTE_MS 5000U
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
static void Traiter_Trame_Commande(uint8_t *data);
static void Envoyer_Trame_Ack(void);

/* Redirection de printf vers UART2 (retarget syscalls) */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/**
 * Callback HAL appele automatiquement des qu'une trame CAN
 * matchant le filtre est deposee dans le FIFO0 par le materiel.
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
}

static void Traiter_Trame_Commande(uint8_t *data)
{
    int16_t x_mm, y_mm, theta_mdeg;

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

    /* On attend qu'une mailbox de transmission soit libre */
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
    {
        /* attente courte, non bloquante dans l'absolu mais bornee */
    }

    if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox) == HAL_OK)
    {
        printf("Message envoye au RPi5 : \"la position est atteinte\"\r\n");
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
    /* On ne veut recevoir QUE l'ID 0x100 : mask = tous les bits actifs */
    sFilterConfig.FilterIdHigh = (ID_CMD << 5);
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = (0x7FF << 5);
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

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_CAN1_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    Configurer_Filtre_CAN();

    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }

    printf("\r\n=== STM32 pret. En attente de commandes CAN (ID 0x%03X) ===\r\n", ID_CMD);
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */
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
            /* Attente non bloquante de 5s : le CPU reste libre de traiter
             * d'autres interruptions (ex: une nouvelle commande CAN, un
             * bouton, etc.) pendant ce temps. */
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
    }
}

/* USER CODE BEGIN 4 */
/* (les fonctions utilitaires sont definies plus haut, section USER CODE 0) */
/* USER CODE END 4 */

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
