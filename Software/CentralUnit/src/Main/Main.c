/******************************************************************************/
/*                                 Main.c                                     */
/******************************************************************************/
/*
   Wallybox Main

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

#define MAIN_SAVEMODE_PIN_DUR  100         /* ms */

#define TIMSYSLED_FREQ_DIV    1000llu     /* set counter incr frequency to 1 ms */
#define TIMSYSLED_PERIOD      500llu      /* set period to 500 ms*/

#define TASK_PER_LOOP    1000             /* calling loop period (must be multiple for all periods) */

#define CLK_TASK_PER     1000             /* Clock.c module call period */
#define CLK_TASK_ORDER      0

#define CSTATE_TASK_PER    10             /* ChargeState.c module call period */
#define CSTATE_TASK_ORDER   0

#define CWIFI_TASK_PER      1             /* CommWifi.c module call period */
#define CWIFI_TASK_ORDER    0

#define COEVSE_TASK_PER    10             /* CommOEvse.c module call period */
#define COEVSE_TASK_ORDER   0

#define SYSLED_TASK_PER       10          /* SysLed.c module call period */
#define SYSLED_TASK_ORDER      0

#define TASK_CALL( prefixlow, prefixup )                                         \
   if ( ( byTaskPerCnt % prefixup##_TASK_PER ) == prefixup##_TASK_ORDER )    \
   {                                                                             \
      prefixlow##_TaskCyc() ;                                                  \
   }


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void main_SaveModeInit( void ) ;
static void main_EnterSaveMode( void ) ;


/*----------------------------------------------------------------------------*/
/* Main program                                                               */
/*----------------------------------------------------------------------------*/

int main( void )
{
   DWORD dwTaskTmp ;
   BYTE byTaskPerCnt ;
   DWORD dwTmpSaveMode ;

      /* Note: The call to HAL_Init() perform these oprations:               */
      /* - Configure the Flash prefetch, Flash preread and Buffer caches     */
      /* - Systick timer is configured by default as source of time base,    */
      /*   but can eventually implement his proper time base source (a       */
      /*   general purpose timer for example or other time source), keeping  */
      /*   in mind that Time base duration should be kept 1ms since          */
      /*   PPP_TIMEOUT_VALUEs are defined and handled in milliseconds basis. */
      /*	- Low Level Initialization                                          */


   HAL_Init() ;                        /* STM32L0xx HAL library initialization */
   GPIO_CLK_ENABLE() ;

   clk_Init() ;
   cal_Init() ;

   cstate_Init() ;
   cwifi_Init() ;
   sfrm_Init() ;
   html_Init() ;

   coevse_Init() ;
   sysled_Init() ;

   main_SaveModeInit() ;               /* Configure powermode pin */

   byTaskPerCnt = 0 ;
   tim_StartMsTmp( &dwTaskTmp ) ;

   while ( TRUE )                      /* Infinite loop */
   {
      if ( HAL_GPIO_ReadPin( MAIN_SAVEMODE_GPIO, MAIN_SAVEMODE_PIN) == GPIO_PIN_RESET )
      {
         if ( dwTmpSaveMode == 0 ) //
         {
            tim_StartMsTmp( &dwTmpSaveMode ) ;
         }
         else
         {
            if ( tim_IsEndMsTmp( &dwTmpSaveMode, MAIN_SAVEMODE_PIN_DUR ) )
            {
               main_EnterSaveMode() ;
            }
         }
      }

      TASK_CALL( clk, CLK ) ;
      TASK_CALL( cstate, CSTATE ) ;
      TASK_CALL( cwifi, CWIFI ) ;
      TASK_CALL( coevse, COEVSE ) ;
      TASK_CALL( sysled, SYSLED ) ;

      while ( ! tim_IsEndMsTmp( &dwTaskTmp, 1 ) ) ;
      tim_StartMsTmp( &dwTaskTmp ) ;

      byTaskPerCnt = ( byTaskPerCnt + 1 ) % TASK_PER_LOOP ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Configure mains checking pin                                               */
/*----------------------------------------------------------------------------*/

static void main_SaveModeInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

   sGpioInit.Pin = MAIN_SAVEMODE_PIN ;
   sGpioInit.Mode = GPIO_MODE_INPUT ;
   sGpioInit.Pull = GPIO_PULLDOWN ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   sGpioInit.Alternate = MAIN_SAVEMODE_AF ;
   HAL_GPIO_Init( MAIN_SAVEMODE_GPIO, &sGpioInit ) ;
}

/*----------------------------------------------------------------------------*/
/* enter power save mode (propagate to power conuming modules)                */
/*----------------------------------------------------------------------------*/

static void main_EnterSaveMode( void )
{
   cwifi_EnterSaveMode() ;
   cstate_EnterSaveMode() ;
   sysled_EnterSaveMode() ;

   while ( HAL_GPIO_ReadPin( MAIN_SAVEMODE_GPIO, MAIN_SAVEMODE_PIN) == GPIO_PIN_RESET ) ;

   NVIC_SystemReset() ;
}
