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

void cstate_Init( void ) ;
void cstate_TaskCyc( void ) ;


#endif /* __CONTROL_H */