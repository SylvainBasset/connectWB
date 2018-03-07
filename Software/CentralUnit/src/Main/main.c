/******************************************************************************/
/*                                                                            */
/*                                 Main.c                                     */
/*                                                                            */
/******************************************************************************/
/* Created on:    4 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "main.h"
#include "System/hard.h"


static void main_AliveLedInit( void ) ;   //
static void BSP_LED_On( void ) ;     //
static void BSP_LED_Toggle( void ) ; //

/*----------------------------------------------------------------------------*/
/* Main program                                                               */
/*----------------------------------------------------------------------------*/
int main( void )
{
      /* Note: The call to HAL_Init() perform these oprations:               */
      /* - Configure the Flash prefetch, Flash preread and Buffer caches     */
      /* - Systick timer is configured by default as source of time base,    */
      /*   but can eventually implement his proper time base source (a       */
      /*   general purpose timer for example or other time source), keeping  */
      /*   in mind that Time base duration should be kept 1ms since          */
      /*   PPP_TIMEOUT_VALUEs are defined and handled in milliseconds basis. */
      /*	- Low Level Initialization                                          */


   HAL_Init();                         /* STM32L0xx HAL library initialization */

   main_AliveLedInit();                /* Configure LED2 */

   BSP_LED_On();                       /* Turn on LED2 */

   BSP_LED_Toggle();                   /* Toggle LED2 */

   volatile int i ;
   for( i = 0 ; i < 10000000 ; i++ ) {}

   while (1)                           /* Infinite loop */
   {
      BSP_LED_Toggle();                /* Toggle LED2 */
      HAL_Delay(500);                   /* Insert 50 ms delay */
   }
}

/*----------------------------------------------------------------------------*/
//
static void main_AliveLedInit( void )
{
   GPIO_InitTypeDef  GPIO_InitStruct ;

   LED2_GPIO_CLK_ENABLE() ;            /* Enable the GPIO_LED Clock */
                                       /* Configure the GPIO_LED pin */
   GPIO_InitStruct.Pin = LED2_PIN ;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP ;
   GPIO_InitStruct.Pull = GPIO_PULLUP ;
   GPIO_InitStruct.Speed = GPIO_SPEED_FAST ;
   HAL_GPIO_Init( LED2_GPIO_PORT, &GPIO_InitStruct ) ;

   //timer init
}

/*----------------------------------------------------------------------------*/
//
static void BSP_LED_On( void )
{
   HAL_GPIO_WritePin( LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_SET ) ;
}

/*----------------------------------------------------------------------------*/
//
static void BSP_LED_Toggle( void )
{
   HAL_GPIO_TogglePin( LED2_GPIO_PORT, LED2_PIN ) ;
}
