/******************************************************************************/
/*                                                                            */
/*                                  Control.h                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:    27 mars 2018   Sylvain BASSET        Version 0.1            */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __CONTROL_H                    /* to prevent recursive inclusion */
#define __CONTROL_H


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

typedef enum
{
   CSTATE_OFF,
   CSTATE_FORCE_AMPMIN_WAIT_VE,
   CSTATE_FORCE_ALL_WAIT_VE,
   CSTATE_DATE_TIME_LOST,
   CSTATE_WAIT_CALENDAR,
   CSTATE_CHARGING,
   CSTATE_END_OF_CHARGE,
} e_cstateChargeSt ;

typedef enum
{
   CSTATE_FORCE_NONE,
   CSTATE_FORCE_AMPMIN,
   CSTATE_FORCE_ALL,
} e_cstateForceSt ;              /* forced charge status */


#define CSTATE_CURRENT_MIN_MAX   16
#define CSTATE_CURRENT_MIN_MIN    0

void cstate_Init( void ) ;

void cstate_SetCurrentMinStop( DWORD i_dwCurrentMinStop ) ;
DWORD cstate_GetCurrentMinStop( void ) ;

void cstate_ToggleForce( void ) ;
e_cstateForceSt cstate_GetForceState( void ) ;
e_cstateChargeSt cstate_GetChargeState( void ) ;
BOOL cstate_IsEvPlugged( void ) ;
void cstate_TaskCyc( void ) ;


#endif /* __CONTROL_H */