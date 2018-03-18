/******************************************************************************/
/*                                                                            */
/*                                 Main.c                                     */
/*                                                                            */
/******************************************************************************/
/* Created on:    4 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "Main.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void main_LedInit( void ) ;
static void main_LedOn( void ) ;


/*----------------------------------------------------------------------------*/
/* Main program                                                               */
/*----------------------------------------------------------------------------*/

int main( void )
{
   DWORD dwTmp ;
   DateTime sReadDT1 ;
   DateTime sReadDT2 ;
   DateTime sReadDT3 ;
   DWORD byVal1 ;
   DWORD byVal2 ;
   DWORD byVal3 ;

      /* Note: The call to HAL_Init() perform these oprations:               */
      /* - Configure the Flash prefetch, Flash preread and Buffer caches     */
      /* - Systick timer is configured by default as source of time base,    */
      /*   but can eventually implement his proper time base source (a       */
      /*   general purpose timer for example or other time source), keeping  */
      /*   in mind that Time base duration should be kept 1ms since          */
      /*   PPP_TIMEOUT_VALUEs are defined and handled in milliseconds basis. */
      /*	- Low Level Initialization                                          */

   HAL_Init() ;                        /* STM32L0xx HAL library initialization */

   clk_Init() ;

   main_LedInit() ;                    /* Configure system LED */

   main_LedOn() ;                      /* Turn on system LED */

   while ( TRUE )                      /* Infinite loop */
   {

      tim_StartMsTmp( &dwTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTmp, 3000 ) ) ;
      clk_GetDateTime( &sReadDT1 ) ;
      byVal1 = sReadDT1.bySeconds ;

      tim_StartMsTmp( &dwTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTmp, 3000 ) ) ;
      clk_GetDateTime( &sReadDT2 ) ;
      byVal2 = sReadDT2.bySeconds ;

      tim_StartMsTmp( &dwTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTmp, 3000 ) ) ;
      clk_GetDateTime( &sReadDT3 ) ;
      byVal3 = sReadDT3.bySeconds ;

      volatile int i ;
      i++ ;

      REFPARM(byVal1) ;
      REFPARM(byVal2) ;
      REFPARM(byVal3) ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* System led initialization                                                  */
/*----------------------------------------------------------------------------*/

static void main_LedInit( void )
{
   GPIO_InitTypeDef sGpioInit ;
   TIM_HandleTypeDef hTimSysLed ;

      /* Note : it is not necessary to keep the System LED timer */
      /* handle <hTimSysLed> as global variable, because it is   */
      /* not reused after timer initialization.                  */

   SYSLED_GPIO_CLK_ENABLE() ;          /* enable the GPIO_LED Clock */
                                       /* configure SYSLED_PIN pin as output push-pull */
   sGpioInit.Pin = SYSLED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_PULLUP ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   HAL_GPIO_Init( SYSLED_GPIO_PORT, &sGpioInit ) ;


   TIMSYSLED_CLK_ENABLE() ;            /* enable clock for system led timer */

   hTimSysLed.Instance = TIMSYSLED ;   /* set timer instance */
                                       /* set counter incr frequency to 1 ms */
   hTimSysLed.Init.Prescaler     = ( SystemCoreClock / 1000lu ) - 1lu ;
                                       /* set period to 500 ms */
   hTimSysLed.Init.Period        = 500 - 1 ;
   hTimSysLed.Init.ClockDivision = 0 ;
   hTimSysLed.Init.CounterMode   = TIM_COUNTERMODE_UP ;
   hTimSysLed.Lock = HAL_UNLOCKED ;
   hTimSysLed.State = HAL_TIM_STATE_RESET ;

                                       /* set timer configuration */
   if ( HAL_TIM_Base_Init( &hTimSysLed ) != HAL_OK )
   {
	   ERR_FATAL() ;
   }
                                       /* set the TIMx priority */
   HAL_NVIC_SetPriority( TIMSYSLED_IRQn, 3, 0 ) ;
                                       /* enable the TIMx global Interrupt */
   HAL_NVIC_EnableIRQ( TIMSYSLED_IRQn ) ;
                                       /* start timer and enble IT */
   HAL_TIM_Base_Start_IT( &hTimSysLed ) ;
}


/*----------------------------------------------------------------------------*/
/* Set system led On                                                          */
/*----------------------------------------------------------------------------*/

static void main_LedOn( void )
{
                                       /* activate system LED pin */
   HAL_GPIO_WritePin( SYSLED_GPIO_PORT, SYSLED_PIN, GPIO_PIN_SET ) ;
}


/*----------------------------------------------------------------------------*/
/* IRQ system LED Timer                                                       */
/*----------------------------------------------------------------------------*/

void TIMSYSLED_IRQHandler( void )
{
                                       /* toggle system LED pin */
   HAL_GPIO_TogglePin( SYSLED_GPIO_PORT, SYSLED_PIN ) ;

   TIMSYSLED->SR = ~TIM_IT_UPDATE ;    /* IRQ acknowledgment */
}
