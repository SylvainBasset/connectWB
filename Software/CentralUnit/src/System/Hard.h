/******************************************************************************/
/*                                   Hard.h                                   */
/******************************************************************************/
/*
   Hardware definitions

   Copyright (C) 2018  Sylvain BASSET

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

   ------------
   @version 1.0
   @history 1.0, 4 mars 2018, creation
*/


#ifndef __HARD_H                     /* to prevent recursive inclusion */
#define __HARD_H

#include "stm32l0xx_hal.h"


/*----------------------------------------------------------------------------*/
/* IOs pins definitions                                                       */
/*----------------------------------------------------------------------------*/

#define GPIO_CLK_ENABLE()              __GPIOA_CLK_ENABLE() ;  \
                                       __GPIOB_CLK_ENABLE() ;  \
                                       __GPIOC_CLK_ENABLE()

#define CSTATE_LEDWIFI_RED_PIN         GPIO_PIN_0
#define CSTATE_LEDWIFI_RED_GPIO        GPIOA
#define CSTATE_LEDWIFI_RED_AF          0

#define CSTATE_LEDWIFI_BLUE_PIN        GPIO_PIN_1
#define CSTATE_LEDWIFI_BLUE_GPIO       GPIOA
#define CSTATE_LEDWIFI_BLUE_AF         0

#define CSTATE_CP_LINE_PIN             GPIO_PIN_6
#define CSTATE_CP_LINE_GPIO            GPIOA
#define CSTATE_CP_LINE_AF              0

#define CSTATE_LEDWIFI_GREEN_PIN       GPIO_PIN_4
#define CSTATE_LEDWIFI_GREEN_GPIO      GPIOA
#define CSTATE_LEDWIFI_GREEN_AF        0

                                       /* User Led from nucleo board (LD2) */
#define SYSLED_PIN                     GPIO_PIN_5
#define SYSLED_GPIO                    GPIOA
#define SYSLED_AF                      0

#define WIFI_TX_PIN                    GPIO_PIN_9
#define WIFI_TX_GPIO                   GPIOA
#define WIFI_TX_AF                     GPIO_AF4_USART1

#define WIFI_RX_PIN                    GPIO_PIN_10
#define WIFI_RX_GPIO                   GPIOA
#define WIFI_RX_AF                     GPIO_AF4_USART1

#define WIFI_RTS_PIN                   GPIO_PIN_12
#define WIFI_RTS_GPIO                  GPIOA
#define WIFI_RTS_AF                    GPIO_AF4_USART1

#define WIFI_CTS_PIN                   GPIO_PIN_11
#define WIFI_CTS_GPIO                  GPIOA
#define WIFI_CTS_AF                    GPIO_AF4_USART1


   /*---------------------- */

#define CSTATE_LEDWIFI_COMMON_PIN      GPIO_PIN_0
#define CSTATE_LEDWIFI_COMMON_GPIO     GPIOB
#define CSTATE_LEDWIFI_COMMON_AF       0

#define CSTATE_LEDCH_RED_PIN           GPIO_PIN_8
#define CSTATE_LEDCH_RED_GPIO          GPIOB
#define CSTATE_LEDCH_RED_AF            0

                                       /* TX pin for OpenEVSE UART communication */
#define OESVE_TX_PIN                   GPIO_PIN_10
#define OESVE_TX_GPIO                  GPIOB
#define OESVE_TX_AF                    GPIO_AF4_LPUART1
                                       /* RX pin for OpenEVSE UART communication */
#define OESVE_RX_PIN                   GPIO_PIN_11
#define OESVE_RX_GPIO                  GPIOB
#define OESVE_RX_AF                    GPIO_AF4_LPUART1

   /*---------------------- */

#define CSTATE_BUTTON_P1_PIN           GPIO_PIN_1
#define CSTATE_BUTTON_P1_GPIO          GPIOC
#define CSTATE_BUTTON_P1_AF            0

#define CSTATE_BUTTON_P2_PIN           GPIO_PIN_2
#define CSTATE_BUTTON_P2_GPIO          GPIOC
#define CSTATE_BUTTON_P2_AF            0

#define CSTATE_LEDCH_GREEN_PIN         GPIO_PIN_5
#define CSTATE_LEDCH_GREEN_GPIO        GPIOC
#define CSTATE_LEDCH_GREEN_AF          0

#define CSTATE_LEDCH_BLUE_PIN          GPIO_PIN_6
#define CSTATE_LEDCH_BLUE_GPIO         GPIOC
#define CSTATE_LEDCH_BLUE_AF           0

#define WIFI_WAKEUP_PIN                GPIO_PIN_8
#define WIFI_WAKEUP_GPIO               GPIOC
#define WIFI_WAKEUP_AF                 0

#define CSTATE_LEDCH_COMMON_PIN        GPIO_PIN_9
#define CSTATE_LEDCH_COMMON_GPIO       GPIOC
#define CSTATE_LEDCH_COMMON_AF         0

#define WIFI_RESET_PIN                 GPIO_PIN_12
#define WIFI_RESET_GPIO                GPIOC
#define WIFI_RESET_AF                  0

#define USER_BP_PIN                    GPIO_PIN_13
#define USER_BP_GPIO                   GPIOC
#define USER_BP_AF                     0


/*----------------------------------------------------------------------------*/
/* IRQs Priorities                                                            */
/*----------------------------------------------------------------------------*/

#define TIMCALIB_IRQPri    0           /* calibration timer IRQ priority */
#define UWIFI_DMA_IRQPri   1           /* Wifi DMA UART */
#define UWIFI_IRQPri       2           /* Wifi UART */
#define UOEVSE_IRQPri      3           /* OpenEVSE UART */


/*----------------------------------------------------------------------------*/
/* definitions for system LED Timer (example)                                 */
/*----------------------------------------------------------------------------*/

#define TIMSYSLED    TIM6              /* timer driver for system LED */
                                       /* system Led timer clock enable/disable */
#define TIMSYSLED_CLK_ENABLE()     __TIM6_CLK_ENABLE()
#define TIMSYSLED_CLK_DISABLE()    __TIM6_CLK_DISABLE()

#define TIMSYSLED_IRQn             TIM6_IRQn
#define TIMSYSLED_IRQHandler       TIM6_DAC_IRQHandler


/*----------------------------------------------------------------------------*/
/* definitions for calib Timer                                                */
/*----------------------------------------------------------------------------*/

#define TIMCALIB    TIM21
#define TIMCALIB_CLK_ENABLE()     __TIM21_CLK_ENABLE()
#define TIMCALIB_CLK_DISABLE()    __TIM21_CLK_DISABLE()

#define TIMCALIB_IRQn             TIM21_IRQn
#define TIMCALIB_IRQHandler       TIM21_IRQHandler


/*----------------------------------------------------------------------------*/
/* definitions for Wifi USART                                                 */
/*----------------------------------------------------------------------------*/

#define UWIFI                      USART1
#define UWIFI_CLK_ENABLE()         __USART1_CLK_ENABLE()
#define UWIFI_CLK_DISABLE()        __USART1_CLK_DISABLE()

#define UWIFI_FORCE_RESET()        __USART1_FORCE_RESET()
#define UWIFI_RELEASE_RESET()      __USART1_RELEASE_RESET()

#define UWIFI_IRQn                 USART1_IRQn
#define UWIFI_IRQHandler           USART1_IRQHandler


/*----------------------------------------------------------------------------*/
/* definitions for openEvse USART                                             */
/*----------------------------------------------------------------------------*/

#define UOEVSE                     LPUART1
#define UOEVSE_CLK_ENABLE()        __LPUART1_CLK_ENABLE()
#define UOEVSE_CLK_DISABLE()       __LPUART1_CLK_DISABLE()

#define UOEVSE_FORCE_RESET()       __LPUART1_FORCE_RESET()
#define UOEVSE_RELEASE_RESET()     __LPUART1_RELEASE_RESET()

#define UOEVSE_IRQn                LPUART1_IRQn
#define UOEVSE_IRQHandler          LPUART1_IRQHandler


/*----------------------------------------------------------------------------*/
/* definitions for Adc (ChargeState)                                            */
/*----------------------------------------------------------------------------*/

#define ADC_CSTATE                 ADC1
#define ADC_CSTATE_CLK_ENABLE()    __ADC1_CLK_ENABLE()
#define ADC_CSTATE_CLK_DISABLE()   __ADC1_CLK_DISABLE()

#define ADC_CSTATE_FORCE_RESET()       __ADC1_FORCE_RESET()
#define ADC_CSTATE_RELEASE_RESET()     __ADC1_RELEASE_RESET()



/*----------------------------------------------------------------------------*/
/* definitions for DMAs                                                       */
/*----------------------------------------------------------------------------*/

#define DMA_MAKE_CSELR( Value, Channel )   \
   ( ( Value & DMA_CSELR_C1S_Msk ) << ( 4 * ( Channel - 1 ) ) )

#define DMA_MAKE_ISRIFCR( Value, Channel )  \
   ( ( Value & 0x0F ) << ( 4 * ( Channel - 1 ) ) )


#define UWIFI_DMA                   DMA1
#define UWIFI_DMA_CSELR             DMA1_CSELR
#define UWIFI_DMA_CLK_ENABLE()      __HAL_RCC_DMA1_CLK_ENABLE()

#define UWIFI_DMA_TX                DMA1_Channel2
#define UWIFI_DMA_TX_CHANNEL        2
#define UWIFI_DMA_TX_REQ            DMA_REQUEST_3
#define UWIFI_DMA_TX_CSELR( Value ) DMA_MAKE_CSELR( Value, UWIFI_DMA_TX_CHANNEL )
#define UWIFI_DMA_TX_ISRIFCR( Value ) \
                                    DMA_MAKE_ISRIFCR( Value, UWIFI_DMA_TX_CHANNEL )

#define UWIFI_DMA_RX                DMA1_Channel3
#define UWIFI_DMA_RX_CHANNEL        3
#define UWIFI_DMA_RX_REQ            DMA_REQUEST_3
#define UWIFI_DMA_RX_CSELR( Value ) DMA_MAKE_CSELR( Value, UWIFI_DMA_RX_CHANNEL )
#define UWIFI_DMA_RX_ISRIFCR( Value ) \
                                    DMA_MAKE_ISRIFCR( Value, UWIFI_DMA_RX_CHANNEL )

#define UWIFI_DMA_IRQn             DMA1_Channel2_3_IRQn
#define UWIFI_DMA_IRQHandler       DMA1_Channel2_3_IRQHandler


#define UOEVSE_DMA                  DMA1
#define UOEVSE_DMA_CSELR            DMA1_CSELR
#define UOEVSE_DMA_CLK_ENABLE()     __HAL_RCC_DMA1_CLK_ENABLE()

#define UOEVSE_DMA_TX               DMA1_Channel7
#define UOEVSE_DMA_TX_CHANNEL       7
#define UOEVSE_DMA_TX_REQ           DMA_REQUEST_5
#define UOEVSE_DMA_TX_CSELR( Value ) \
                                    DMA_MAKE_CSELR( Value, UOEVSE_DMA_TX_CHANNEL )
#define UOEVSE_DMA_TX_ISRIFCR( Value ) \
                                    DMA_MAKE_ISRIFCR( Value, UOEVSE_DMA_TX_CHANNEL )

#endif /* __HARD_H */
