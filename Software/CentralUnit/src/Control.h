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
void cal_SetDayVals( BYTE i_byWeekday,
                     s_Time const * i_psStartTime, s_Time const * i_psEndTime ) ;
BOOL cal_IsChargeEnable( void ) ;


/*----------------------------------------------------------------------------*/
/* ChargeState.c                                                              */
/*----------------------------------------------------------------------------*/

void cstate_Init( void ) ;
void cstate_TaskCyc( void ) ;


#endif /* __CONTROL_H */