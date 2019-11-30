/******************************************************************************/
/*                                  Clock.c                                   */
/******************************************************************************/
/*
   STM32 RTC clock managment

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
   @history 1.0, 12 mars 2018, creation
   @brief

   This module configure the STM32 RTC peripheral to maintain date/time.

   The date/time is stored with the use of s_DateTime struture.

   It is possible to set a new date/time using this structure with clk_SetDateTime(),
   read date/time with clk_GetDateTime(), as well as determine if date/time is
   lost with clk_IsDateTimeLost().

   This module automatically check if the current date/time is in summer hour,
   and set the summer time bit (RTC_CR_BCK) accordingly.

   The RTC 32kHz is also compared to the system clock by the timer21 input
   capture channel 1. The 32kHz clock is therefore ajusted, to compensate
   potential frequence variations.
*/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define LSE_FREQ           32768llu    /* LSE clock frequency */

#define CLK_ASYNC_PREDIV   63llu       /* RTC asynchronous prediv */
#define CLK_SYNC_PREDIV    511llu      /* RTC synchronous prediv */

RTC_InitTypeDef const k_sRtcInit =     /* HAL RTC initialisation constants */
{
   .HourFormat = RTC_HOURFORMAT_24,
   .AsynchPrediv = CLK_ASYNC_PREDIV,
   .SynchPrediv = CLK_SYNC_PREDIV,
   .OutPut = RTC_OUTPUT_DISABLE,
   .OutPutRemap = RTC_OUTPUT_REMAP_NONE,
   .OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH,
   .OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN,
} ;

                                       /* RTC set handle macro */
#define SET_RTC_HANDLE( handle )        \
   handle.Instance = RTC ;              \
   handle.State = HAL_RTC_STATE_READY ; \
   handle.Lock = HAL_UNLOCKED ;

//SBA: v�rifier tous les lock, et les champs non initialis�s


                                       /* RTC set init handle macro */
#define SET_RTC_HANDLE_INIT( handle )                       \
   handle.Instance = RTC ;                                  \
   handle.State = HAL_RTC_STATE_READY ;                     \
   handle.Lock = HAL_UNLOCKED ;                             \
   memcpy( &handle.Init, &k_sRtcInit, sizeof(handle.Init) )


#define CLK_DATETIME_SIGN  0x5C5C5C5C  /* mark to idientify is datetime is valid */

s_DateTime const k_sDateTimeInit =     /* datetime value for intializtion */
{
   .byYear = 0,
   .byMonth = 1,
   .byDays = 1,
   .byHours = 0,
   .byMinutes = 0,
   .bySeconds = 0,
} ;

#define CLK_SPR_MONTH      3           /* month of winter to summer daylight change */
#define CLK_AUT_MONTH      10          /* month of summer to winter daylight change */
#define CLK_SPR_HOUR       2           /* hour of winter to summer daylight change */
#define CLK_AUT_HOUR       3           /* hour of summer to winter daylight change */

typedef enum                           /* daytlight datetime type */
{
   CLK_WINTER = 0,                     /* winter datetime type */
   CLK_SUMMER_1,                       /* first part of summer datetime type */
   CLK_SUMMER_2,                       /* second part of summer datetime type */
} e_SummerState ;

                                       /* month/minute/hour represenattion for
                                          middle of summer */
DWORD const k_dwReprMDHMidSum = ( 7 * 100 * 100 ) + ( 1 * 100 ) ;

#include "ClockConst.h"                /* import constants to manage datetime operations */


#define CLK_CALIB_NBMEAS      64       /* number of mesures for averaging */
                                       /* low calibration limit */
#define CLK_CALIB_LO          ( ( ( ( APB1_CLK * 997  ) / LSE_FREQ ) / 1000 ) + 1 )
                                       /* high calibration limit */
#define CLK_CALIB_HI          ( ( ( APB1_CLK * 1003 ) / LSE_FREQ ) / 1000 )

#define CLK_CALIB_MAXPPM      5000ll   /* maximum ppm delta for error setting */
#define CLK_CALIB_MINPPM      -5000ll  /* minimum ppm delta for error setting */

#define CLK_CALIB_MINTRIMM    0        /* minimum value for HSI trimm */
#define CLK_CALIB_MAXTRIMM    0x1F     /* maximum value for HSI trimm */


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( s_DateTime C* i_psDateTime ) ;
static BOOL clk_ComIsValid( s_DateTime C* i_pDateTime ) ;
static void clk_ComGetDateTime( s_DateTime * o_psDateTime, BYTE * o_pbyWeekday ) ;
static e_SummerState clk_GetSummerState( s_DateTime C* i_psDateTime ) ;
static DWORD clk_CalcReprMDH( BYTE i_byMonth, BYTE i_byDay, BYTE i_byHour ) ;
static void clk_RtcInit( void ) ;
static void clk_32kHzInit( void ) ;
static void clk_CalibInit( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static DWORD l_dwCalibSum ;            /* calibration total sum couter */
static BYTE l_byCalibCnt ;             /* calibration measurements counter */
static WORD l_wCalibMoy ;              /* average calibration value */

static BYTE l_byHSITrim ;              /* trimm value */
static DWORD l_dwNbCalibActions ;      /* number of clock changes counter */


/*----------------------------------------------------------------------------*/
/* Clock module initialization                                                */
/*----------------------------------------------------------------------------*/

void clk_Init( void )
{
   DWORD dwInitTmp ;
   SWORD sdwCalibPpmErr ;

   // ajout capa sur schema elec (uniquement sur partie backup ou g�n�ral)
   // v�rification si besoin de plus de tests si les appels � la HAL

   /* Note: at module initialition, the system verifies if the datetime is */
   /* lost by checking 32kHz clock ready but, RTC enable bit, and datetime */
   /* valid signature inside backup registers. If datetime is lost,        */
   /* intialization of LSE clock and RTC are performed. In this case,      */
   /* datetime is set "temporarly" to 01/01/2000 00:00:00. Datetime is     */
   /*  considered as valid when clk_ComSetDateTime() is called             */

   HAL_PWR_EnableBkUpAccess() ;        /* enable backup and RTC domain access */

                                       /* test if datetime is lost */
   if ( ( ! ISSET( RCC->CSR, RCC_CSR_LSERDY ) ) ||
        ( ! ISSET( RCC->CSR, RCC_CSR_RTCEN ) ) ||
        ( clk_IsDateTimeLost() ) )
   {
      __HAL_RCC_BACKUPRESET_FORCE() ;  /* reset RTC module */
      __HAL_RCC_BACKUPRESET_RELEASE() ;

      clk_32kHzInit() ;                /* LSE 32kHz clock initialization */

      clk_RtcInit() ;                  /* RTC initialization */

                                       /* set initial datetime (at this point, */
                                       /* datetime is not yet considered as valid */
      clk_ComSetDateTime( &k_sDateTimeInit ) ;
   }

   clk_CalibInit() ;                   /* system clock calibration */

   tim_StartMsTmp( &dwInitTmp ) ;      /* wait clock trim to stabilize */
   while ( ! tim_IsEndMsTmp( &dwInitTmp, 20 ) ) ;

   sdwCalibPpmErr = clk_GetCalib( NULL ) ;
   if ( ( sdwCalibPpmErr > CLK_CALIB_MAXPPM ) ||
        ( sdwCalibPpmErr < CLK_CALIB_MINPPM ) )
   {
      ERR_FATAL() ;
   }
}


/*----------------------------------------------------------------------------*/
/* Test if datetime is lost from RTC module                                   */
/*----------------------------------------------------------------------------*/

BOOL clk_IsDateTimeLost( void )
{
   BOOL bRet ;
                                       /* test if datetime valid signature is present */
   if ( RTC->BKP0R == CLK_DATETIME_SIGN )
   {
      bRet = FALSE ;                   /* datetime valid */
   }
   else
   {
      bRet = TRUE ;                    /* datetime lost */
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Get calibration informations                                               */
/*----------------------------------------------------------------------------*/

SDWORD clk_GetCalib( DWORD * o_dwNbCalibActions )
{
   DWORD dwThTimeRef ;
   DWORD dwThTimeVal ;
   SDWORD sdwPpmErr ;

   if ( o_dwNbCalibActions != NULL )
   {
      *o_dwNbCalibActions = l_dwNbCalibActions ;
   }

   dwThTimeRef = ( ( APB1_CLK * 1000llu ) / LSE_FREQ ) ;
   dwThTimeVal = l_wCalibMoy * 1000llu ;

   sdwPpmErr = ( ( (SDWORD)dwThTimeVal - (SDWORD)dwThTimeRef ) * 1000000 ) / (SDWORD)dwThTimeRef ;

   return sdwPpmErr ;
}


/*----------------------------------------------------------------------------*/
/* Date/time writing                                                          */
/*----------------------------------------------------------------------------*/

void clk_SetDateTime( s_DateTime C* i_psDateTime )
{
   clk_ComSetDateTime( i_psDateTime ) ; /* set datetime */

   RTC->BKP0R = CLK_DATETIME_SIGN ;    /* mark as datetime valid */
}


/*----------------------------------------------------------------------------*/
/* Date/time reading                                                          */
/*----------------------------------------------------------------------------*/

void clk_GetDateTime( s_DateTime * o_psDateTime, BYTE * o_pbyWeekday )
{
   clk_ComGetDateTime( o_psDateTime, o_pbyWeekday ) ;
}


/*----------------------------------------------------------------------------*/
/* Check if Date/time is valid                                                */
/*----------------------------------------------------------------------------*/

BOOL clk_IsValid( s_DateTime C* i_psDateTime )
{
   return clk_ComIsValid( i_psDateTime ) ;
}


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 1sec )                                              */
/*----------------------------------------------------------------------------*/

void clk_TaskCyc( void )
{
   s_DateTime sDateTime ;
   e_SummerState eSummerState ;
   SWORD sdwCalibPpmErr ;

   sdwCalibPpmErr = clk_GetCalib( NULL ) ;
   if ( ( sdwCalibPpmErr > CLK_CALIB_MAXPPM ) ||
        ( sdwCalibPpmErr < CLK_CALIB_MINPPM ) )
   {
      ERR_FATAL() ;
   }

                                       /* current datetime read */
   clk_ComGetDateTime( &sDateTime, NULL ) ;
                                       /* if current datetime reach 31/12/2099 */
   if ( ( sDateTime.byYear == 99 ) && ( sDateTime.byMonth == 12 ) && ( sDateTime.byDays == 31 ) )
   {                                   /* set current Datetime to init value */
      clk_ComSetDateTime( &k_sDateTimeInit ) ;
      RTC->BKP0R = 0 ;                 /* mark as lost datetime */
   }
   else
   {                                   /* get daylight status for this datetime */
      eSummerState = clk_GetSummerState( &sDateTime ) ;

      RTC->WPR = 0xCAU ;               /* disable RTC register write protection */
      RTC->WPR = 0x53U ;
                                       /* test if datetime is in first part of summer */
                                       /* while summer +1hour has not been added */
      if ( ( eSummerState == CLK_SUMMER_1 ) && ( ! ISSET( RTC->CR, RTC_CR_BCK ) ) )
      {                                /* indicate the +1 hour operation is done */
                                       /* (summer time) */
         SET_BIT( RTC->CR, RTC_CR_BCK ) ;
                                       /* add 1 hour to current datetime */
         SET_BIT( RTC->CR, RTC_CR_ADD1H ) ;
      }

      if ( ( eSummerState == CLK_WINTER ) && ( ISSET( RTC->CR, RTC_CR_BCK ) ) )
      {                                   /* indicate the -1hour operation is done */
                                          /* (winter time) */
         CLEAR_BIT( RTC->CR, RTC_CR_BCK ) ;
                                          /* add 1 hour from current datetime */
         SET_BIT( RTC->CR, RTC_CR_SUB1H ) ;
      }
      RTC->WPR = 0xFFU ;                  /* enable RTC register write protection */
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Common function for date/time setting                                      */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( s_DateTime C* i_psDateTime )
{
   RTC_HandleTypeDef sRtcHandle ;
   RTC_DateTypeDef sRtcDate ;
   BYTE byYear ;
   BYTE byMonth ;
   BYTE byDays ;
   BYTE by1StWeekday ;
   BYTE byWeekday ;
   WORD wIdxFirstDayOfMonth ;
   RTC_TimeTypeDef sRtcTime ;
   e_SummerState eSummerState ;


   ERR_FATAL_IF( ! clk_ComIsValid( i_psDateTime ) ) ;


   SET_RTC_HANDLE( sRtcHandle ) ;      /* RTC set handle */

   byYear = i_psDateTime->byYear ;
   byMonth = i_psDateTime->byMonth ;
   byDays = i_psDateTime->byDays ;
                                       /* set HAL date structure */
   memset( &sRtcDate, 0, sizeof(sRtcDate) ) ;
   sRtcDate.Year = byYear ;
   sRtcDate.Month = byMonth ;
   sRtcDate.Date = byDays ;
                                       /* calculate index to find first day of */
                                       /* month weekeday value */
   wIdxFirstDayOfMonth = ( byYear * 12 + ( byMonth - 1 ) ) / 2 ;

      /* Note: each element of <k_abyFirstDayOfMonth> containt   */
      /* the weekday for 2 months, which explain the /2 division */

   ERR_FATAL_IF( wIdxFirstDayOfMonth >= ARRAY_SIZE(k_abyFirstDayOfMonth) ) ;

   if ( ( byMonth - 1 ) % 2 == 0 )     /* if month is odd (jan., march, etc..) */
   {                                   /* weekday of 1st day of this month is */
                                       /* the low 4 bits of element */
      by1StWeekday = LO4B( k_abyFirstDayOfMonth[wIdxFirstDayOfMonth] ) ;
   }
   else
   {                                   /* weekday of 1st day of this month is */
                                       /* the high 4 bits of element */
      by1StWeekday = HI4B( k_abyFirstDayOfMonth[wIdxFirstDayOfMonth] ) ;
   }

      /* Note : in the STM32L0 Rtc, weekday is coded from 1 (monday) */
      /* to 7 (sunday). So, it is necessary to add +1 to the value   */

                                       /* get the weekday of <byDays> in this month */
   byWeekday = ( ( by1StWeekday + ( byDays - 1 ) ) % NB_DAYS_WEEK ) + 1 ;

   sRtcDate.WeekDay = byWeekday ;      /* set weekday value */

      /* Note: To ensure data integrity both shadows registers are    */
      /* copied to calendar register when the TR register is written. */
      /* Hence, DT (date) register must be written before TR (time).  */

                                       /* write date to RTC module */
   if ( HAL_RTC_SetDate( &sRtcHandle, &sRtcDate, RTC_FORMAT_BIN ) != HAL_OK )
   {
      ERR_FATAL() ;
   }
                                       /* set HAL time structure */
   memset( &sRtcTime, 0, sizeof(sRtcTime) ) ;
   sRtcTime.Hours = i_psDateTime->byHours ;
   sRtcTime.Minutes = i_psDateTime->byMinutes ;
   sRtcTime.Seconds = i_psDateTime->bySeconds ;

                                       /* write date to RTC module */
   if ( HAL_RTC_SetTime( &sRtcHandle, &sRtcTime, RTC_FORMAT_BIN ) != HAL_OK )
   {
      ERR_FATAL() ;
   }
                                       /* get daylight status for this datetime */
   eSummerState = clk_GetSummerState( i_psDateTime ) ;

   RTC->WPR = 0xCAU ;                  /* disable RTC register write protection */
   RTC->WPR = 0x53U ;
                                       /* test if datetime is summer type */
   if ( ( eSummerState == CLK_SUMMER_1 ) || ( eSummerState == CLK_SUMMER_2 ) )
   {                                   /* indicate the +1hour operation is already */
                                       /* done (summer time) */
      SET_BIT( RTC->CR, RTC_CR_BCK ) ;
   }
   else
   {                                   /* indicate the -1hour operation is already */
                                       /* done (winter time) */
      CLEAR_BIT( RTC->CR, RTC_CR_BCK ) ;
   }
   RTC->WPR = 0xFFU ;                  /* enable RTC register write protection */
}


/*----------------------------------------------------------------------------*/
/* Check if Date/time is valid                                                */
/*----------------------------------------------------------------------------*/

static BOOL clk_ComIsValid( s_DateTime C* i_pDateTime )
{
   BOOL bValid ;

   bValid = TRUE ;

   if ( ( i_pDateTime->byYear > 99 ) ||
        ( i_pDateTime->byMonth == 0 ) || ( i_pDateTime->byMonth > 12 ) ||
        ( i_pDateTime->byDays == 0 ) || ( i_pDateTime->byDays > 31 ) ||
        ( i_pDateTime->byHours > 23 ) ||
        ( i_pDateTime->byMinutes > 59 ) ||
        ( i_pDateTime->bySeconds > 59 ) )
   {
      bValid = FALSE ;
   }


   if ( ( i_pDateTime->byMonth == 2 ) || ( i_pDateTime->byMonth == 4 ) ||
        ( i_pDateTime->byMonth == 6 ) || ( i_pDateTime->byMonth == 9 ) ||
        ( i_pDateTime->byMonth == 11 ) )
   {
      if ( i_pDateTime->byDays == 31 )
      {
         bValid = FALSE ;
      }
   }


   if ( i_pDateTime->byMonth == 2 )
   {
      if ( i_pDateTime->byDays == 30 )
      {
         bValid = FALSE ;
      }
      if ( ( i_pDateTime->byDays == 29 ) && ( ( i_pDateTime->byYear % 4 ) != 0 ) )
      {
         bValid = FALSE ;
      }
   }
   return bValid ;
}


/*----------------------------------------------------------------------------*/
/* Date/time reading                                                          */
/*----------------------------------------------------------------------------*/

static void clk_ComGetDateTime( s_DateTime * o_psDateTime, BYTE * o_pbyWeekday )
{
   RTC_HandleTypeDef sRtcHandle ;
   RTC_DateTypeDef sRtcDate ;
   RTC_TimeTypeDef sRtcTime ;

   SET_RTC_HANDLE( sRtcHandle ) ;      /* RTC set handle */
                                       /* read time by HAL method */
   HAL_RTC_GetTime( &sRtcHandle, &sRtcTime, RTC_FORMAT_BIN ) ;
                                       /* copy time read results */
   o_psDateTime->byHours = sRtcTime.Hours ;
   o_psDateTime->byMinutes = sRtcTime.Minutes ;
   o_psDateTime->bySeconds = sRtcTime.Seconds ;

      /* Note : Once DT (date) register is read, shadows registers */
      /* update is stopped until TR (time) is read. This mecanism  */
      /* ensure data integrity between the two registers. Hence it */
      /* is necessary to read DT register first, then TR register  */

                                       /* read date by HAL method */
   HAL_RTC_GetDate( &sRtcHandle, &sRtcDate, RTC_FORMAT_BIN ) ;
                                       /* copy date read results */
   o_psDateTime->byYear = sRtcDate.Year ;
   o_psDateTime->byMonth = sRtcDate.Month ;
   o_psDateTime->byDays = sRtcDate.Date ;

   if ( o_pbyWeekday != NULL )         /* if weekday is requested */
   {                                   /* set weekday */
      *o_pbyWeekday = sRtcDate.WeekDay - 1 ;
   }
}


/*----------------------------------------------------------------------------*/
/* Get daylgigt saving status from one particular datetime                    */
/*----------------------------------------------------------------------------*/

static e_SummerState clk_GetSummerState( s_DateTime C* i_psDateTime )
{
   e_SummerState eSumState ;
   DWORD dwReprMDHCur ;
   DWORD dwReprMDHSpr ;
   DWORD dwReprMDHAut ;
                                       /* calculate month/date/hour representation */
                                       /* of current datetime */
   dwReprMDHCur = clk_CalcReprMDH( i_psDateTime->byMonth,
                                   i_psDateTime->byDays, i_psDateTime->byHours ) ;

                                       /* calculate month/date/hour representation */
                                       /* of winter-to-summer datetime */
   dwReprMDHSpr = clk_CalcReprMDH( CLK_SPR_MONTH, k_abyDaySumSpr[i_psDateTime->byYear], CLK_SPR_HOUR ) ;
                                       /* calculate month/date/hour representation */
                                       /* of summer-to-winter datetime */
   dwReprMDHAut = clk_CalcReprMDH( CLK_AUT_MONTH, k_abyDaySumAut[i_psDateTime->byYear], CLK_AUT_HOUR ) ;

                                       /* test is datetime is outside summer */
   if ( ( dwReprMDHCur < dwReprMDHSpr ) || ( dwReprMDHCur >= dwReprMDHAut ) )
   {
      eSumState = CLK_WINTER ;
   }                                   /* test if datetime is on first part of summer */
   else if ( dwReprMDHCur < k_dwReprMDHMidSum )
   {
      eSumState = CLK_SUMMER_1 ;       /* 1st part */
   }
   else
   {
      eSumState = CLK_SUMMER_2 ;       /* 2nd part */
   }

   return eSumState ;
}


/*----------------------------------------------------------------------------*/
/* Calculate month/day/hour representation of a particular datetime.          */
/* Note: Hour value is added to day * 100 and month * 10000.                  */
/* This representation enable simple DWORD comparisation for daylight saving  */
/* calculations. (This representation does not take years, minutes and sec in */
/* account.                                                                   */
/*----------------------------------------------------------------------------*/

static DWORD clk_CalcReprMDH( BYTE i_byMonth, BYTE i_byDay, BYTE i_byHour )
{
   return ( ( i_byMonth * 100 * 100 ) + ( i_byDay * 100 ) + i_byHour ) ;
}


/*----------------------------------------------------------------------------*/
/* RTC module init                                                            */
/*----------------------------------------------------------------------------*/

static void clk_RtcInit( void )
{
   RTC_HandleTypeDef sRtcHandle ;
                                       /* set 32kHz LSE as RTC input */
   __HAL_RCC_RTC_CONFIG( RCC_RTCCLKSOURCE_LSE ) ;
   __HAL_RCC_RTC_ENABLE() ;            /* enable RTC module */

   SET_RTC_HANDLE_INIT( sRtcHandle ) ; /* RTC set init handle */

                                       /* HAL initialization method */
   if ( HAL_RTC_Init( &sRtcHandle ) != HAL_OK )
   {
      ERR_FATAL() ;                    /* go to error if it fails */
   }
}


/*----------------------------------------------------------------------------*/
/* LSE 32 kHz clock init                                                      */
/*----------------------------------------------------------------------------*/

static void clk_32kHzInit( void )
{
   RCC_OscInitTypeDef sRtcClkInitStruct ;

                                       /* Enable LSE Oscillator */
   sRtcClkInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE ;
   sRtcClkInitStruct.LSEState = RCC_LSE_ON ;
   sRtcClkInitStruct.PLL.PLLState = RCC_PLL_NONE ;
   HAL_RCC_OscConfig(&sRtcClkInitStruct) ;
}


/*----------------------------------------------------------------------------*/
/* Internal system clock calibration by 32.768 kHz Quartz                     */
/*----------------------------------------------------------------------------*/

static void clk_CalibInit( void )
{
   TIMCALIB_CLK_ENABLE() ;             /* enable calbration timer clock */
                                       /* set TI1FP1 as timer event source */
   TIMCALIB->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 ;
   TIMCALIB->SMCR |= TIM_SMCR_SMS_2 ;  /* timer event reset the timer */

   TIMCALIB->ARR = 0xFFFF ;            /* no reload value, as timer is reset by event */
   TIMCALIB->PSC = 0 ;                 /* no prescaller */

   TIMCALIB->CCMR1 = TIM_CCMR1_CC1S_0 ; /* map input capture source to IC1 */
   TIMCALIB->CCER = TIM_CCER_CC1E ;    /* enable input capture 1 */

   TIMCALIB->OR = TIM21_OR_TI1_RMP_2 ; /* TI1 input connected to LSE clock */

   TIMCALIB->DIER = TIM_DIER_CC1IE ;   /* enable input capture 1 interruption */

                                       /* set the calbration timer priority */
   HAL_NVIC_SetPriority( TIMCALIB_IRQn, TIMCALIB_IRQPri, 0 ) ;
                                       /* enable the calbration timer global interrupt */
   HAL_NVIC_EnableIRQ( TIMCALIB_IRQn ) ;

   TIMCALIB->CR1 = TIM_CR1_CEN ;       /* start the timer */

   l_dwCalibSum = 0 ;                  /* initialize total sum couter */
   l_byCalibCnt = 0 ;                  /* initialize calibration measurements counter */
   l_dwNbCalibActions = 0 ;            /* initialize number of clock changes counter */

   l_byHSITrim = 0 ;                   /* initialize trimm value */
   RCC->ICSCR = 0 ;                    /* set actual trimm to 0 */
}


/*----------------------------------------------------------------------------*/
/* IRQ calbration timer                                                       */
/*----------------------------------------------------------------------------*/

void TIMCALIB_IRQHandler( void )
{
   WORD wCalibMoy ;

   l_dwCalibSum += TIMCALIB->CCR1 ;    /* add input capture value */
   l_byCalibCnt++ ;                    /* add 1 measure to counter */
                                       /* all measurements done */
   if ( l_byCalibCnt == CLK_CALIB_NBMEAS )
   {                                   /* get average value */
      wCalibMoy = l_dwCalibSum / CLK_CALIB_NBMEAS ;
      l_wCalibMoy = wCalibMoy ;
                                       /* if average value is below low limit */
                                       /* and not maximum trimm value */
      if ( ( wCalibMoy < CLK_CALIB_LO ) && ( l_byHSITrim < CLK_CALIB_MAXTRIMM ) )
      {
         l_dwNbCalibActions++ ;        /* increment action counter */
         l_byHSITrim++ ;               /* increment HSI trimm value */
                                       /* set new HSI trimm value */
         RCC->ICSCR = ( ( l_byHSITrim & CLK_CALIB_MAXTRIMM ) << RCC_ICSCR_HSITRIM_Pos ) ;
      }
                                       /* if average value is above high limit */
                                       /* and not minimum trimm value */
      else if ( ( wCalibMoy > CLK_CALIB_HI ) && ( l_byHSITrim > CLK_CALIB_MINTRIMM ) )
      {
         l_dwNbCalibActions++ ;        /* increment action counter */
         l_byHSITrim-- ;               /* decrement HSI trimm value */
                                       /* set new HSI trimm value */
         RCC->ICSCR = ( ( l_byHSITrim & CLK_CALIB_MAXTRIMM ) << RCC_ICSCR_HSITRIM_Pos ) ;
      }

      l_dwCalibSum = 0 ;               /* re-initialize total sum couter */
      l_byCalibCnt = 0 ;               /* re-initialize calibration measurements counter */
   }
}
