/******************************************************************************/
/*                                 SysLed.c                                   */
/******************************************************************************/
/*
   System Led (on nucleo board) management

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
   @history 1.0, 14 mar. 2019, creation
*/


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
/* Set power save mode (reset LED)                                            */
/*----------------------------------------------------------------------------*/

void sysled_EnterSaveMode( void )
{
   HAL_GPIO_WritePin( SYSLED_GPIO, SYSLED_PIN, GPIO_PIN_RESET ) ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
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
