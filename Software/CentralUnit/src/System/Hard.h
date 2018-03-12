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
#define SYSLED_PIN                  GPIO_PIN_5
#define SYSLED_GPIO_PORT            GPIOA
#define SYSLED_GPIO_CLK_ENABLE()    __GPIOA_CLK_ENABLE()
#define SYSLED_GPIO_CLK_DISABLE()   __GPIOA_CLK_DISABLE()


/*----------------------------------------------------------------------------*/
/* IRQs Priorities                                                            */
/*----------------------------------------------------------------------------*/

#define TIMSYSLED_IRQPri   3           /* system Led timer IRQ priority */


/*----------------------------------------------------------------------------*/
/* IRQ definitions for system LED Timer (example)                             */
/*----------------------------------------------------------------------------*/

#define TIMSYSLED    TIM6              /* timer driver for system LED */
                                       /* system Led timer clock enable/disable */
#define TIMSYSLED_CLK_ENABLE()     __TIM6_CLK_ENABLE()
#define TIMSYSLED_CLK_DISABLE()    __TIM6_CLK_DISABLE()

#define TIMSYSLED_IRQn             TIM6_IRQn
#define TIMSYSLED_IRQHandler       TIM6_DAC_IRQHandler

#endif /* __HARD_H */