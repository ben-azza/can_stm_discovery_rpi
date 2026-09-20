/* Ces fonctions sont normalement generees automatiquement par CubeMX dans
 * main.c a partir de la configuration decrite dans CONFIGURATION_CUBEMX.md.
 * Elles sont fournies ici a titre de reference / verification, a coller
 * dans les sections correspondantes si tu ecris le projet a la main
 * plutot que de le generer depuis l'IDE. */

#include "main.h"

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart2;

/**
 * CAN1 : bitrate 500 kbps pour APB1 = 80 MHz
 * Prescaler = 10, BS1 = 13 TQ, BS2 = 2 TQ, SJW = 1 TQ
 * -> 1 bit = (1 + 13 + 2) TQ = 16 TQ
 * -> Tq = Prescaler / APB1 = 10 / 80MHz = 125 ns
 * -> bit time = 16 * 125ns = 2000 ns = 2 us -> 500 kbit/s (1/2us = 500kHz) OK
 */
void MX_CAN1_Init(void)
{
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 10;
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = ENABLE;
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }
}

void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* Rappel HAL MSP : active les horloges et configure les pins PA11/PA12
 * (CAN1) ainsi que PA2/PA3 (USART2, deja fait par CubeMX pour la Nucleo) */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA11 = CAN1_RX, PA12 = CAN1_TX */
        GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
    }
}
