/******************************************************************************/
/*                              ChargeCalendar.c                              */
/******************************************************************************/
/*
   Calendar for charging periods

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
   @history 1.0, 04 oct. 2018, creation
   @brief
   The calendar is based on hardware RTC

   Week day periods are set by calling cal_SetDayVals(), they are stored in
   eeprom for power-off retention. They are recovered at initialisation.
   cal_GetDayVals() is used to get start and end time of charing for one day.
   And cal_IsChargeEnable() allows to determine if charge is enable at this
   instant.
*/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "Control.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CAL_HOUR_MAX       23
#define CAL_MINUTES_MAX    59
#define CAL_SECONDS_MAX    59

                                       /* maximum time value in second */
#define CAL_TIMESEC_MAX    ( ( CAL_HOUR_MAX * 60 * 60 ) + \
                             ( CAL_MINUTES_MAX * 60 ) + CAL_SECONDS_MAX )


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

                                       /* table of starting times in second */
static DWORD l_adwTimeSecStart [ NB_DAYS_WEEK ] ;
                                       /* table of ending times in second */
static DWORD l_adwTimeSecEnd [ NB_DAYS_WEEK ] ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static DWORD cal_CalcCntFromStruct( s_Time C* i_psTime ) ;
static void cal_CalcStructFromCnt( DWORD i_dwTimeSec, s_Time * o_pTime ) ;


/*----------------------------------------------------------------------------*/
/* Calendar module initialization                                             */
/* Note : read eeprom to get memorised weekday charging periods               */
/*----------------------------------------------------------------------------*/

void cal_Init( void )
{
   BYTE byWeekDay ;
   DWORD dwTimeSecStart ;
   DWORD dwTimeSecEnd ;

                                       /* for each day in the week */
   for ( byWeekDay = 0 ; byWeekDay < NB_DAYS_WEEK ; byWeekDay++ )
   {                                   /* get starting time from eeprom */
      dwTimeSecStart = g_sDataEeprom->sCalData.adwTimeSecStart[byWeekDay] ;
                                       /* get ending time from eeprom */
      dwTimeSecEnd = g_sDataEeprom->sCalData.adwTimeSecEnd[byWeekDay] ;

                                       /* if read value are outside valid ranges or */
                                       /* starting time is after ending time */
      if ( ( dwTimeSecStart > CAL_TIMESEC_MAX ) || ( dwTimeSecEnd > CAL_TIMESEC_MAX ) )
      {
         dwTimeSecStart = 0 ;          /* starting time is set to 0 (initial value) */
         dwTimeSecEnd = 0 ;            /* ending time is set to 0 (initial value) */

                                       /* eeprom value is cleared */
         eep_write( (DWORD)&g_sDataEeprom->sCalData.adwTimeSecStart[byWeekDay], 0 ) ;
         eep_write( (DWORD)&g_sDataEeprom->sCalData.adwTimeSecEnd[byWeekDay], 0 ) ;
      }
                                       /* set starting time */
      l_adwTimeSecStart[byWeekDay] = dwTimeSecStart ;
                                       /* set ending time */
      l_adwTimeSecEnd[byWeekDay] = dwTimeSecEnd ;
   }
}


/*----------------------------------------------------------------------------*/
/* Check if start/end time for weekday is valid                               */
/*    - <i_pStartTime> sarting time to be tested  for this day                */
/*    - <i_pEndTime> ending time to be tested  for this day                   */
/*----------------------------------------------------------------------------*/

BOOL cal_IsValid( s_Time C* i_pStartTime, s_Time C* i_pEndTime )
{
   BOOL bValid ;

   if ( ( i_pStartTime->byHours > CAL_HOUR_MAX ) || ( i_pEndTime->byHours > CAL_HOUR_MAX ) ||
        ( i_pStartTime->byMinutes > CAL_MINUTES_MAX ) || ( i_pEndTime->byMinutes > CAL_MINUTES_MAX ) ||
        ( i_pStartTime->bySeconds > CAL_SECONDS_MAX ) || ( i_pEndTime->bySeconds > CAL_SECONDS_MAX ) )
   {
      bValid = FALSE ;
   }
   else
   {
      bValid = TRUE ;
   }
   return bValid ;
}


/*----------------------------------------------------------------------------*/
/* Set calendard Start/End time                                               */
/*    - <i_byWeekday> day of the week (0=Monday, 6=Sunday)                    */
/*    - <i_pStartTime> sarting time for this day                              */
/*    - <i_pEndTime> ending time for this day                                 */
/*----------------------------------------------------------------------------*/

void cal_SetDayVals( BYTE i_byWeekday, s_Time C* i_pStartTime, s_Time C* i_pEndTime )
{
   DWORD dwStartValue ;
   DWORD dwEndValue ;
                                       /* if week day outside limit */
   ERR_FATAL_IF( i_byWeekday >= NB_DAYS_WEEK ) ;

                                       /* calulate starting time in second */
   dwStartValue = cal_CalcCntFromStruct( i_pStartTime ) ;
                                       /* calculate ending time in second */
   dwEndValue = cal_CalcCntFromStruct( i_pEndTime ) ;

                                       /* set starting time */
   l_adwTimeSecStart[i_byWeekday] = dwStartValue ;
                                       /* set ending time */
   l_adwTimeSecEnd[i_byWeekday] = dwEndValue ;

                                       /* write starting time in eeprom */
   eep_write( (DWORD)&g_sDataEeprom->sCalData.adwTimeSecStart[i_byWeekday], dwStartValue ) ;
                                       /* write ending time in eeprom */
   eep_write( (DWORD)&g_sDataEeprom->sCalData.adwTimeSecEnd[i_byWeekday], dwEndValue ) ;
}


/*----------------------------------------------------------------------------*/
/* Get start/end time for a day                                               */
/*    - <i_byWeekday> week day to read                                        */
/*    - <o_pStartTime> start time struct for this weekday                     */
/*    - <o_pEndTime> end time struct for this weekday                         */
/*    - <o_pdwCntStart> start time counter for this weekday                   */
/*    - <o_pdwCntEnd> end time counter for this weekday                       */
/*----------------------------------------------------------------------------*/

void cal_GetDayVals( BYTE i_byWeekday, s_Time * o_pStartTime, s_Time * o_pEndTime,
                                       DWORD * o_pdwCntStart, DWORD * o_pdwCntEnd )
{
   DWORD dwCntStart ;
   DWORD dwCntEnd ;

                                       /* if week day outside limit */
   ERR_FATAL_IF( i_byWeekday >= NB_DAYS_WEEK ) ;

   dwCntStart = l_adwTimeSecStart[i_byWeekday] ;
   dwCntEnd = l_adwTimeSecEnd[i_byWeekday] ;

   cal_CalcStructFromCnt( dwCntStart, o_pStartTime ) ;
   cal_CalcStructFromCnt( dwCntEnd, o_pEndTime ) ;

   if ( o_pdwCntStart != NULL )
   {
      *o_pdwCntStart = dwCntStart ;
   }

   if ( o_pdwCntEnd != NULL )
   {
      *o_pdwCntEnd = dwCntEnd ;
   }
}


/*----------------------------------------------------------------------------*/
/* Get charge enable status                                                   */
/* Return:                                                                    */
/*    - charge allowed state                                                  */
/*----------------------------------------------------------------------------*/

BOOL cal_IsChargeEnable( void )
{
   BOOL bRet ;
   BOOL bDtLost ;
   s_DateTime sCurDateTime ;
   BYTE byWeekday ;
   DWORD dwTimeSecCurrent ;
   DWORD dwTimeSecStart ;
   DWORD dwTimeSecEnd ;


   bRet = FALSE ;                      /* charge is not allowed by default */

   bDtLost = clk_IsDateTimeLost() ;    /* get datetime lost status */

   if ( ! bDtLost )                    /* if datetime is valid */
   {                                   /* read current datetime */
      clk_GetDateTime( &sCurDateTime, &byWeekday ) ;
                                       /* if week day outside limit */
      ERR_FATAL_IF( byWeekday >= NB_DAYS_WEEK ) ;
                                       /* calculate current time in second */
      dwTimeSecCurrent = cal_CalcCntFromStruct( &sCurDateTime.sTime ) ;

                                       /* get starting time in second */
      dwTimeSecStart = l_adwTimeSecStart[byWeekday] ;
                                       /* get ending time in second */
      dwTimeSecEnd = l_adwTimeSecEnd[byWeekday] ;

                                       /* starting time = ending time means */
                                       /* charge is discarded this day */
      if ( dwTimeSecEnd != dwTimeSecStart )
      {
         if ( dwTimeSecStart < dwTimeSecEnd )
         {                             /* if current time is between starting */
                                       /* and ending time */
            if ( ( dwTimeSecCurrent >= dwTimeSecStart ) &&
                 ( dwTimeSecCurrent < dwTimeSecEnd ) )
            {
               bRet = TRUE ;              /* charge is allowed */
            }
         }
         else
         {                                /* if current time is outside starting */
                                          /* and ending time */
            if ( ( dwTimeSecCurrent >= dwTimeSecStart ) ||
                 ( dwTimeSecCurrent < dwTimeSecEnd ) )
            {
               bRet = TRUE ;              /* charge is allowed */
            }
         }
      }
   }

   return bRet ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Calculate second count representation of a particular time.                */
/*    - <i_psTime> time structure to convert                                  */
/* Return:                                                                    */
/*    - second value for this time                                            */
/*                                                                            */
/* Note: Second value is added to minute * 60 and hour * 3600.                */
/* This representation enable simple DWORD comparisation for charge calendar  */
/* calculations.                                                              */
/*----------------------------------------------------------------------------*/

static DWORD cal_CalcCntFromStruct( s_Time C* i_psTime )
{
   BYTE byHours ;
   BYTE byMinutes ;
   BYTE bySeconds ;

   byHours = i_psTime->byHours ;       /* get hour */
   byMinutes = i_psTime->byMinutes ;   /* get minute */
   bySeconds = i_psTime->bySeconds ;   /* get second */

   ERR_FATAL_IF( byHours > CAL_HOUR_MAX ) ;
   ERR_FATAL_IF( byMinutes > CAL_MINUTES_MAX ) ;
   ERR_FATAL_IF( bySeconds > CAL_SECONDS_MAX ) ;
                                       /* return second value for this time */
   return ( ( byHours * 60 * 60 ) + ( byMinutes * 60 ) + bySeconds ) ;
}


/*----------------------------------------------------------------------------*/
/* Build time struct from second counter                                      */
/*    - <i_dwTimeSec> second counter                                          */
/*    - <o_pTime> output time struct                                          */
/*----------------------------------------------------------------------------*/

static void cal_CalcStructFromCnt( DWORD i_dwTimeSec, s_Time * o_pTime )
{
   BYTE byHours ;
   BYTE byMinutes ;
   BYTE bySeconds ;
   DWORD dwTimeSec ;

   dwTimeSec = i_dwTimeSec ;

   bySeconds = ( dwTimeSec % 60 ) ;
   dwTimeSec = dwTimeSec / 60 ;

   byMinutes = ( dwTimeSec % 60 ) ;
   dwTimeSec = dwTimeSec / 60 ;

   byHours = dwTimeSec ;

   ERR_FATAL_IF( byHours > CAL_HOUR_MAX ) ;
   ERR_FATAL_IF( byMinutes > CAL_MINUTES_MAX ) ;
   ERR_FATAL_IF( bySeconds > CAL_SECONDS_MAX ) ;

   o_pTime->byHours = byHours ;
   o_pTime->byMinutes = byMinutes ;
   o_pTime->bySeconds = bySeconds ;
}