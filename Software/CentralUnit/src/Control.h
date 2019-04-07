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
/* SysLed.c                                                                    */
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

typedef enum
{
   CSTATE_NULL,
   CSTATE_OFF,
   CSTATE_FORCE_WAIT,
   CSTATE_ON_WAIT,
   CSTATE_CHARGING,
   CSTATE_EOC_LOWCUR,
   CSTATE_EOC,
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
void cstate_EnterSaveMode( void ) ;

void cstate_SetCurrentMinStop( DWORD i_dwCurrentMinStop ) ;
DWORD cstate_GetCurrentMinStop( void ) ;

void cstate_ToggleForce( void ) ;
e_cstateForceSt cstate_GetForceState( void ) ;
e_cstateChargeSt cstate_GetChargeState( void ) ;
BOOL cstate_IsEvPlugged( void ) ;
void cstate_TaskCyc( void ) ;


#endif /* __CONTROL_H */