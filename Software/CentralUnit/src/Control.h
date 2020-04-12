/******************************************************************************/
/*                                  Control.h                                 */
/******************************************************************************/
/*
   control header

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
   @history 1.0, 27 mar. 2018, creation
*/



#ifndef __CONTROL_H                    /* to prevent recursive inclusion */
#define __CONTROL_H


/*----------------------------------------------------------------------------*/
/* SysLed.c                                                                   */
/*----------------------------------------------------------------------------*/

void sysled_Init( void ) ;
void sysled_EnterSaveMode( void ) ;
void sysled_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* ChargeCalendar.c                                                           */
/*----------------------------------------------------------------------------*/

void cal_Init( void ) ;
void cal_SetDayVals( BYTE i_byWeekday, s_Time C* i_pStartTime, s_Time C* i_pEndTime ) ;
void cal_GetDayVals( BYTE i_byWeekday, s_Time * o_pStartTime, s_Time * o_pEndTime,
                                       DWORD * o_pdwCntStart, DWORD * o_pdwCntEnd ) ;
BOOL cal_IsValid( s_Time C* i_pStartTime, s_Time C* i_pEndTime ) ;
BOOL cal_IsChargeEnable( void ) ;


/*----------------------------------------------------------------------------*/
/* ChargeState.c                                                              */
/*----------------------------------------------------------------------------*/

typedef enum                     /* FSM charge state */
{
   CSTATE_NULL,
   CSTATE_OFF,                   /* idle state, charge disabled */
   CSTATE_FORCE_WAIT,            /* forced and waiting for EV, charge enabled */
   CSTATE_ON_WAIT,               /* calendar enabled and waiting for EV, charge enabled */
   CSTATE_CHARGING,              /* charging in progress, charge enabled */
   CSTATE_EOC_LOWCUR,            /* end of charge for low current, charge disabled */
} e_cstateChargeSt ;

typedef enum                     /* forced charge status */
{
   CSTATE_FORCE_NONE,            /* no force */
   CSTATE_FORCE_AMPMIN,          /* minimum current (ampere) forced */
   CSTATE_FORCE_ALL,             /* always forced */
} e_cstateForceSt ;

#define CSTATE_CURRENT_MINSTOP_MIN    0
#define CSTATE_CURRENT_MINSTOP_MAX   16

void cstate_Init( void ) ;
void cstate_EnterSaveMode( void ) ;

void cstate_SetCurrentMinStop( DWORD i_dwCurrentMinStop ) ;
DWORD cstate_GetCurrentMinStop( void ) ;

void cstate_ToggleForce( void ) ;
e_cstateForceSt cstate_GetForceState( void ) ;
e_cstateChargeSt cstate_GetChargeState( void ) ;

void cstate_GetHistState( CHAR * o_pszHistState, WORD i_wSize ) ;
WORD cstate_GetAdcVal( CHAR * o_pszAdcVal, WORD i_wSize ) ;

void cstate_TaskCyc( void ) ;


#endif /* __CONTROL_H */