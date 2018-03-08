/******************************************************************************/
/*                                                                            */
/*                                 Hard.h                                     */
/*                                                                            */
/******************************************************************************/
/* Created on:    4 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "stm32l0xx_hal.h"


/*----------------------------------------------------------------------------*/
/* IO pins definitions                                                        */
/*----------------------------------------------------------------------------*/

                                       /* Led from nucleo example */
#define LED2_PIN                       GPIO_PIN_5
#define LED2_GPIO_PORT                 GPIOA
#define LED2_GPIO_CLK_ENABLE()         __GPIOA_CLK_ENABLE()
#define LED2_GPIO_CLK_DISABLE()        __GPIOA_CLK_DISABLE()


/*----------------------------------------------------------------------------*/
/* IRQ Priority                                                               */
/*----------------------------------------------------------------------------*/

#define TIMLED_IRQPri           3


/*----------------------------------------------------------------------------*/
/* IRQ Timer example                                                          */
/*----------------------------------------------------------------------------*/

#define TIMLED                  TIM2
#define TIMLED_CLK_ENABLE()     __TIM2_CLK_ENABLE()
#define TIMLED_CLK_DISABLE()    __TIM2_CLK_DISABLE()

#define TIMLED_IRQn             TIM2_IRQn
#define TIMLED_IRQHandler       TIM2_IRQHandler
