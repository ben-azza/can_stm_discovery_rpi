/*
 * ============================================================================
 * Redirection de printf vers USB CDC (port OTG FS, connecteur micro-USB CN5)
 * ============================================================================
 *
 * A la place de la version UART (__io_putchar via HAL_UART_Transmit),
 * utiliser cette version si l'affichage doit passer par l'USB OTG FS
 * (port micro-USB direct du STM32F407, pas le ST-Link).
 *
 * Necessite d'avoir active USB_OTG_FS (Device Only) + USB_DEVICE (CDC)
 * dans CubeMX au prealable (voir README).
 * ============================================================================
 */

/* ===== USER CODE BEGIN Includes ===== */
#include <stdio.h>
#include <string.h>
#include "usbd_cdc_if.h"   /* genere automatiquement par CubeMX/USB_DEVICE */

extern CAN_HandleTypeDef hcan1;
extern USBD_HandleTypeDef hUsbDeviceFS;
/* ===== USER CODE END Includes ===== */


/* ===== USER CODE BEGIN PFP ===== */

/*
 * Redirection de printf vers l'USB CDC.
 * CDC_Transmit_FS peut renvoyer USBD_BUSY si le buffer precedent
 * n'est pas encore vide (l'hote n'a pas encore lu) : on reessaie
 * un court instant avant d'abandonner ce caractere.
 */
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

/*
 * Alternative plus efficace : envoyer directement une chaine complete
 * au lieu d'appeler __io_putchar caractere par caractere.
 * Utile si tu remplaces printf() par cette fonction pour de meilleures
 * performances (optionnel, printf() standard fonctionne deja avec
 * __io_putchar seul).
 */
void USB_Print(const char *msg)
{
    uint32_t essais = 0;
    uint16_t len = (uint16_t)strlen(msg);

    while (CDC_Transmit_FS((uint8_t *)msg, len) == USBD_BUSY)
    {
        essais++;
        if (essais > 10000)
        {
            return;
        }
        HAL_Delay(1);
    }
}
/* ===== USER CODE END PFP ===== */
