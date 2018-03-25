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
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/


#define TIMSYSLED_FREQ_DIV    1000llu  /* set counter incr frequency to 1 ms */
#define TIMSYSLED_PERIOD      500llu   /* set period to 500 ms*/

                                       /* timer initialisation constant */
TIM_Base_InitTypeDef const k_sTimSysLedInit =
{
   .Prescaler     = ( HSYS_CLK / TIMSYSLED_FREQ_DIV ) - 1lu,
   .Period        = TIMSYSLED_PERIOD - 1,
   .ClockDivision = 0,
   .CounterMode   = TIM_COUNTERMODE_UP,
} ;
                                       /* sysled timer set init handle macro */
#define SET_TIMSYSLED_HANDLE_INIT( handle )                 \
   handle.Instance = TIMSYSLED ;                            \
   handle.State = HAL_TIM_STATE_READY ;                     \
   memcpy( &handle.Init, &k_sTimSysLedInit, sizeof(handle.Init) )

                                       /* system led gpio constant */
GPIO_InitTypeDef const k_sSysLedGpioInit =
{
   .Pin = SYSLED_PIN,
   .Mode = GPIO_MODE_OUTPUT_PP,
   .Pull = GPIO_PULLUP,
   .Speed = GPIO_SPEED_FAST,
} ;

#define CLK_TASK_PER    100


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
   DWORD dwTaskTmp ;
   s_DateTime sSetDT ;
   s_DateTime sReadDT ;

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

   sSetDT.byYear = 18 ;
   sSetDT.byMonth = 10 ;
   sSetDT.byDays = 28 ;
   sSetDT.byHours = 2 ;
   sSetDT.byMinutes = 59 ;
   sSetDT.bySeconds = 56 ;
   clk_SetDateTime( &sSetDT ) ;

   tim_StartMsTmp( &dwTmp ) ;

   while ( TRUE )                      /* Infinite loop */
   {
      clk_TaskCyc() ;

      tim_StartMsTmp( &dwTaskTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTaskTmp, 10 ) ) ;

      if ( tim_IsEndMsTmp( &dwTmp, 6000 ) )
      {
         clk_GetDateTime( &sReadDT ) ;

         BYTE val ;
         val = sReadDT.byHours ;
         REFPARM(val) ;
      }
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* System led initialization                                                  */
/*----------------------------------------------------------------------------*/

static void main_LedInit( void )
{
   GPIO_InitTypeDef sGpioInit ;
   TIM_HandleTypeDef sTimSysLed ;

      /* Note : it is not necessary to keep the System LED timer */
      /* handle <hTimSysLed> as global variable, because it is   */
      /* not reused after timer initialization.                  */

   SYSLED_GPIO_CLK_ENABLE() ;          /* enable the GPIO_LED Clock */
                                       /* configure SYSLED_PIN pin as output push-pull */

   memcpy( &sGpioInit, &k_sSysLedGpioInit, sizeof(sGpioInit) ) ;
   HAL_GPIO_Init( SYSLED_GPIO_PORT, &sGpioInit ) ;


   TIMSYSLED_CLK_ENABLE() ;            /* enable clock for system led timer */

   SET_TIMSYSLED_HANDLE_INIT( sTimSysLed ) ;
                                       /* set timer configuration */
   if ( HAL_TIM_Base_Init( &sTimSysLed ) != HAL_OK )
   {
	   ERR_FATAL() ;
   }
                                       /* set the TIMx priority */
   HAL_NVIC_SetPriority( TIMSYSLED_IRQn, 3, 0 ) ;
                                       /* enable the TIMx global Interrupt */
   HAL_NVIC_EnableIRQ( TIMSYSLED_IRQn ) ;
                                       /* start timer and enble IT */
   HAL_TIM_Base_Start_IT( &sTimSysLed ) ;
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
