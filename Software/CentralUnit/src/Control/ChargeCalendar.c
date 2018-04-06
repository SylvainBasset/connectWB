/******************************************************************************/
/*                                                                            */
/*                              ChargeCalendar.c                              */
/*                                                                            */
/******************************************************************************/
/* Created on:   27 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "Control.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* Module description                                                         */
/* //TBD                                                                      */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/
                                       /* maximum time value in second */
#define CAL_TIMESEC_MAX    ( ( 23 * 60 * 60 ) + ( 59 * 60 ) + 59 )


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

                                       /* table of starting times in second */
DWORD l_adwTimeSecStart [ NB_DAYS_WEEK ] ;
                                       /* table of ending times in second */
DWORD l_adwTimeSecEnd [ NB_DAYS_WEEK ] ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static DWORD cal_CalcTimeSecValue( s_Time const * i_psTime ) ;


/*----------------------------------------------------------------------------*/
/* Calendar module initialization                                             */
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
      if ( ( dwTimeSecStart > CAL_TIMESEC_MAX ) || ( dwTimeSecEnd > CAL_TIMESEC_MAX ) ||
           ( dwTimeSecStart > dwTimeSecEnd ) )
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
/* Set calendard Start/End time                                               */
/*    - <i_byWeekday> day of the week (0=Monday, 6=Sunday)                    */
/*    - <i_psStartTime> sarting time for this day                             */
/*    - <i_psEndTime> ending time for this day                                */
/*----------------------------------------------------------------------------*/

void cal_SetDayVals( BYTE i_byWeekday,
                     s_Time const * i_psStartTime, s_Time const * i_psEndTime )
{
   DWORD dwStartValue ;
   DWORD dwEndValue ;
                                       /* if week day outside limit */
   ERR_FATAL_IF( i_byWeekday >= NB_DAYS_WEEK ) ;

                                       /* calulate starting time in second */
   dwStartValue = cal_CalcTimeSecValue( i_psStartTime ) ;
                                       /* calculate ending time in second */
   dwEndValue = cal_CalcTimeSecValue( i_psEndTime ) ;

                                       /* if end time is after start time */
   ERR_FATAL_IF( dwStartValue > dwEndValue ) ;
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
      dwTimeSecCurrent = cal_CalcTimeSecValue( &sCurDateTime.sTime ) ;

                                       /* get starting time in second */
      dwTimeSecStart = l_adwTimeSecStart[byWeekday] ;
                                       /* get ending time in second */
      dwTimeSecEnd = l_adwTimeSecEnd[byWeekday] ;

                                       /* if end time is after start time */
      ERR_FATAL_IF( dwTimeSecEnd < dwTimeSecStart ) ;

                                       /* starting time = ending time means */
                                       /* charge is discarded this day */
      if ( dwTimeSecEnd != dwTimeSecStart )
      {                                /* if current time is between starting */
                                       /* and ending time */
         if ( ( dwTimeSecCurrent >= dwTimeSecStart ) &&
              ( dwTimeSecCurrent < dwTimeSecEnd ) )
         {
            bRet = TRUE ;              /* charge is allowed */
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

static DWORD cal_CalcTimeSecValue( s_Time const * i_psTime )
{
   BYTE byHours ;
   BYTE byMinutes ;
   BYTE bySeconds ;

   byHours = i_psTime->byHours ;       /* get hour */
   byMinutes = i_psTime->byMinutes ;   /* get minute */
   bySeconds = i_psTime->bySeconds ;   /* get second */

   ERR_FATAL_IF( byHours > 23 ) ;
   ERR_FATAL_IF( byMinutes > 59 ) ;
   ERR_FATAL_IF( bySeconds > 59 ) ;
                                       /* return second value for this time */
   return ( ( byHours * 60 * 60 ) + ( byMinutes * 60 ) + bySeconds ) ;
}