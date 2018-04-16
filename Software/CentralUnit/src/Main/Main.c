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
#include "Control.h"
#include "Communic.h"


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

#define TASK_PER_LOOP   100                        //SBA: doit être multiple de toutes les periodes

#define CLK_TASK_PER    100
#define CLK_TASK_ORDER    0


const s_Time k_TimeStart = { .byHours = 06, .byMinutes = 00, .bySeconds = 00 } ;  //SBA
const s_Time k_TimeEnd = { .byHours = 07, .byMinutes = 00, .bySeconds = 00 } ;  //SBA

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
   DWORD dwTaskTmp ;
   s_DateTime sSetDT ;
   BYTE byTaskPerCnt ;
   s_Time sTimeS ;
   s_Time sTimeE ;


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
   cal_Init() ;

   //uwifi_Init() ;
   cwifi_Init() ;
   cwifi_Reset() ;
   uwifi_Init() ;

   BYTE abyData[4] ;
   abyData[0] = 0 ; abyData[1] = 1 ;
   abyData[2] = 2 ; abyData[3] = 3 ;
   //uwifi_Transmit( abyData, sizeof(abyData) ) ;

   cwifi_UnReset() ;
   while(1) ;

   main_LedInit() ;                    /* Configure system LED */

   main_LedOn() ;                      /* Turn on system LED */

   sSetDT.byYear = 18 ;
   sSetDT.byMonth = 04 ;
   sSetDT.byDays = 01 ;
   sSetDT.byHours = 23 ;
   sSetDT.byMinutes = 59 ;
   sSetDT.bySeconds = 50 ;
   clk_SetDateTime( &sSetDT ) ;

   sTimeS.byHours = 6 ;
   sTimeS.byMinutes = 0 ;
   sTimeS.bySeconds = 0 ;
   sTimeE.byHours = 6 ;
   sTimeE.byMinutes = 0 ;
   sTimeE.bySeconds = 0 ;

   cal_SetDayVals( 0, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 1, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 2, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 3, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 4, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 5, &sTimeS, &sTimeE ) ;
   cal_SetDayVals( 6, &sTimeS, &sTimeE ) ;

   byTaskPerCnt = 0 ;

   while ( TRUE )                      /* Infinite loop */
   {
      if ( ( byTaskPerCnt % CLK_TASK_PER ) == CLK_TASK_ORDER )
      {
         clk_TaskCyc() ;

         if ( cal_IsChargeEnable() )
         {
            main_LedOn() ;
         }
         else
         {
            HAL_GPIO_WritePin( SYSLED_GPIO_PORT, SYSLED_PIN, GPIO_PIN_RESET ) ;
         }
      }

      tim_StartMsTmp( &dwTaskTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTaskTmp, 10 ) ) ;

      byTaskPerCnt = ( byTaskPerCnt + 1 ) % TASK_PER_LOOP ;
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
