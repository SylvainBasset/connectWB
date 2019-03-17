/******************************************************************************/
/*                                                                            */
/*                                 SysLed.c                                   */
/*                                                                            */
/******************************************************************************/
/* Created on:   14 mar. 2019   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Control.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                  */
/*----------------------------------------------------------------------------*/

#define  SYSLED_DUR_TMP    500         /* ms */


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static DWORD l_dwLedTmp ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void sysled_Init( void )
{
   GPIO_InitTypeDef sGpioInit ;

   sGpioInit.Pin = SYSLED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   sGpioInit.Alternate = SYSLED_AF ;
   HAL_GPIO_Init( SYSLED_GPIO, &sGpioInit ) ;

   tim_StartMsTmp( &l_dwLedTmp ) ;
}


/*----------------------------------------------------------------------------*/

void sysled_EnterSaveMode( void )
{
   HAL_GPIO_WritePin( SYSLED_GPIO, SYSLED_PIN, GPIO_PIN_RESET ) ;
}


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 10 msec )                                           */
/*----------------------------------------------------------------------------*/

void sysled_TaskCyc( void )
{
                                       /* if tempo ended */
   if ( tim_IsEndMsTmp( &l_dwLedTmp, SYSLED_DUR_TMP ) )
   {
      tim_StartMsTmp( &l_dwLedTmp ) ;
                                       /* toggle system LED pin */
      HAL_GPIO_TogglePin( SYSLED_GPIO, SYSLED_PIN ) ;
   }
}
