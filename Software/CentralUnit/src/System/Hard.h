/******************************************************************************/
/*                                                                            */
/*                                   Hard.h                                   */
/*                                                                            */
/******************************************************************************/
/* Created on:    4 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __HARD_H                     /* to prevent recursive inclusion */
#define __HARD_H

#include "stm32l0xx_hal.h"


/*----------------------------------------------------------------------------*/
/* IOs pins definitions                                                       */
/*----------------------------------------------------------------------------*/

                                       /* User Led from nucleo board (LD2) */
#define SYSLED_PIN                     GPIO_PIN_5
#define SYSLED_GPIO_PORT               GPIOA
#define SYSLED_AF                      0
#define SYSLED_GPIO_CLK_ENABLE()       __GPIOA_CLK_ENABLE()
#define SYSLED_GPIO_CLK_DISABLE()      __GPIOA_CLK_DISABLE()

#define WIFI_TX_PIN                    GPIO_PIN_9
#define WIFI_TX_GPIO_PORT              GPIOA
#define WIFI_TX_AF                     GPIO_AF4_USART1
#define WIFI_TX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()

#define WIFI_RX_PIN                    GPIO_PIN_10
#define WIFI_RX_GPIO_PORT              GPIOA
#define WIFI_RX_AF                     GPIO_AF4_USART1
#define WIFI_RX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()

#define WIFI_RTS_PIN                   GPIO_PIN_12
#define WIFI_RTS_GPIO_PORT             GPIOA
#define WIFI_RTS_AF                    GPIO_AF4_USART1
#define WIFI_RTS_GPIO_CLK_ENABLE()     __GPIOA_CLK_ENABLE()

#define WIFI_CTS_PIN                   GPIO_PIN_11
#define WIFI_CTS_GPIO_PORT             GPIOA
#define WIFI_CTS_AF                    GPIO_AF4_USART1
#define WIFI_CTS_GPIO_CLK_ENABLE()     __GPIOA_CLK_ENABLE()

#define WIFI_RESET_PIN                 GPIO_PIN_12
#define WIFI_RESET_GPIO_PORT           GPIOC
#define WIFI_RESET_AF                  0
#define WIFI_RESET_GPIO_CLK_ENABLE()  __GPIOC_CLK_ENABLE()

#define WIFI_WAKEUP_PIN                GPIO_PIN_8
#define WIFI_WAKEUP_GPIO_PORT          GPIOC
#define WIFI_WAKEUP_AF                 0
#define WIFI_WAKEUP_GPIO_CLK_ENABLE()  __GPIOC_CLK_ENABLE()


/*----------------------------------------------------------------------------*/
/* IRQs Priorities                                                            */
/*----------------------------------------------------------------------------*/

#define TIMCALIB_IRQPri    0           /* calibration timer IRQ priority */
#define UWIFI_DMA_IRQPri   1           /* Wifi DMA UART */
#define UWIFI_IRQPri       2           /* Wifi UART */
#define TIMSYSLED_IRQPri   3           /* system Led timer IRQ priority */


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
#define UWIFI_IRQHandler           USART1_DAC_IRQHandler


/*----------------------------------------------------------------------------*/
/* definitions for DMAs                                                       */
/*----------------------------------------------------------------------------*/

#define DMA_MAKE_CSELR( Value, Channel )   \
   ( ( Value & DMA_CSELR_C1S_Msk ) << ( 4 * ( Channel - 1 ) ) )

#define DMA_MAKE_ISRIFCR( Value, Channel )  \
   ( ( Value & 0x0F ) << ( 4 * ( Channel - 1 ) ) )


#define UWIFI_DMA                   DMA1
#define UWIFI_DMA_CSELR             DMA1_CSELR

#define UWIFI_DMA_TX_CLK_ENABLE()   __HAL_RCC_DMA1_CLK_ENABLE()
#define UWIFI_DMA_TX                DMA1_Channel2
#define UWIFI_DMA_TX_CHANNEL        2
#define UWIFI_DMA_TX_REQ            DMA_REQUEST_3
#define UWIFI_DMA_TX_CSELR( Value ) DMA_MAKE_CSELR( Value, UWIFI_DMA_TX_CHANNEL )
#define UWIFI_DMA_TX_ISRIFCR( Value ) \
                                    DMA_MAKE_ISRIFCR( Value, UWIFI_DMA_TX_CHANNEL )

#define UWIFI_DMA_RX_CLK_ENABLE()   __HAL_RCC_DMA1_CLK_ENABLE()
#define UWIFI_DMA_RX                DMA1_Channel3
#define UWIFI_DMA_RX_CHANNEL        3
#define UWIFI_DMA_RX_REQ            DMA_REQUEST_3
#define UWIFI_DMA_RX_CSELR( Value ) DMA_MAKE_CSELR( Value, UWIFI_DMA_RX_CHANNEL )
#define UWIFI_DMA_RX_ISRIFCR( Value ) \
                                    DMA_MAKE_ISRIFCR( Value, UWIFI_DMA_RX_CHANNEL )

#define UWIFI_DMA_IRQn             DMA1_Channel2_3_IRQn
#define UWIFI_DMA_IRQHandler       DMA1_Channel2_3_IRQHandler

#endif /* __HARD_H */
