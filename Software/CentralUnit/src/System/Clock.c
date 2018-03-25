/******************************************************************************/
/*                                                                            */
/*                                  Clock.c                                   */
/*                                                                            */
/******************************************************************************/
/* Created on:   12 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CLK_ASYNC_PREDIV   64llu       /* RTC asynchronous prediv */
#define CLK_SYNC_PREDIV    512llu      /* RTC synchronous prediv */

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
#define SET_RTC_HANDLE( handle ) \
   handle.Instance = RTC ;       \
   handle.State = HAL_RTC_STATE_READY

                                       /* RTC set init handle macro */
#define SET_RTC_HANDLE_INIT( handle )                       \
   handle.Instance = RTC ;                                  \
   handle.State = HAL_RTC_STATE_READY ;                     \
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

                                       /* table of winter to summer day change */
BYTE const k_abyDaySumSpr[] = { 26,25,31,30,28,27,26,25,30,29,28,27,25,31,30,29,27,
                              26,25,31,29,28,27,26,31,30,29,28,26,25,31,30,28,27,
                              26,25,30,29,28,27,25,31,30,29,27,26,25,31,29,28,27,
                              26,31,30,29,28,26,25,31,30,28,27,26,25,30,29,28,27,
                              25,31,30,29,27,26,25,31,29,28,27,26,31,30,29,28,26,
                              25,31,30,28,27,26,25,30,29,28,27,25,31,30,29 } ;

                                       /* table of summer to winter day change */
BYTE const k_abyDaySumAut[] = { 29,28,27,26,31,30,29,28,26,25,31,30,28,27,26,25,30,
                              29,28,27,25,31,30,29,27,26,25,31,29,28,27,26,31,30,
                              29,28,26,25,31,30,28,27,26,25,30,29,28,27,25,31,30,
                              29,27,26,25,31,29,28,27,26,31,30,29,28,26,25,31,30,
                              28,27,26,25,30,29,28,27,25,31,30,29,27,26,25,31,29,
                              28,27,26,31,30,29,28,26,25,31,30,28,27,26,25 } ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( s_DateTime const* i_psDateTime ) ;
static void clk_ComGetDateTime( s_DateTime * o_psDateTime ) ;
static e_SummerState clk_GetSummerState( s_DateTime const* i_psDateTime ) ;
static DWORD clk_CalcReprMDH( BYTE i_byMonth, BYTE i_byDay, BYTE i_byHour ) ;
static void clk_RtcInit( void ) ;
static void clk_32kHzInit( void ) ;


/*----------------------------------------------------------------------------*/
/* RTC init                                                                   */
/*----------------------------------------------------------------------------*/

void clk_Init( void )
{
   // ajout capa sur schema elec (uniquement sur partie backup ou général)
   // vérification si besoin de plus de tests si les appels à la HAL

   /* Note: at module initialition, the system verifies if the datetime is */
   /* lost by checking 32kHz clock ready but, RTC enable bit, and datetime */
   /* valid signature inside backup registers. If datetime is lost,        */
   /* intialization of LSE clock and RTC are performed. In this case,      */
   /* datetime is set "temporarly" to 01/01/2000 00:00:00. Datetime is     */
   /*  considered as valid when clk_ComSetDateTime() is called             */

   HAL_PWR_EnableBkUpAccess() ;        /* Enable backup and RTC domain access */

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
/* Date/time writing                                                          */
/*----------------------------------------------------------------------------*/

void clk_SetDateTime( s_DateTime const* i_psDateTime )
{
   clk_ComSetDateTime( i_psDateTime ) ; /* set datetime */

   RTC->BKP0R = CLK_DATETIME_SIGN ;    /* mark as datetime valid */
}


/*----------------------------------------------------------------------------*/
/* Date/time reading                                                          */
/*----------------------------------------------------------------------------*/

void clk_GetDateTime( s_DateTime * o_psDateTime )
{
   clk_ComGetDateTime( o_psDateTime ) ;
}


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 1sec )                                              */
/*----------------------------------------------------------------------------*/

void clk_TaskCyc( void )
{
   s_DateTime sDateTime ;
   e_SummerState eSummerState ;

   clk_ComGetDateTime( &sDateTime ) ;  /* current datetime read */

                                       /* get daylight status for this datetime */
   eSummerState = clk_GetSummerState( &sDateTime ) ;

   RTC->WPR = 0xCAU ;                  /* disable RTC register write protection */
   RTC->WPR = 0x53U ;

                                       /* test if datetime is in first part of summer */
                                       /* while summer +1hour has not been added */
   if ( ( eSummerState == CLK_SUMMER_1 ) && ( ! ISSET( RTC->CR, RTC_CR_BCK ) ) )
   {                                   /* indicate the +1hour operation is done */
                                       /* (summer time) */
      SET_BIT( RTC->CR, RTC_CR_BCK ) ;
      SET_BIT( RTC->CR, RTC_CR_ADD1H ) ; /* add 1 hour to current datetime */
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


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Common function for date/time setting                                      */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( s_DateTime const* i_psDateTime )
{
   RTC_HandleTypeDef sRtcHandle ;
   RTC_DateTypeDef sRtcDate ;
   RTC_TimeTypeDef sRtcTime ;
   e_SummerState eSummerState ;

   SET_RTC_HANDLE( sRtcHandle ) ;      /* RTC set handle */

                                       /* set HAL date structure */
   memset( &sRtcDate, 0, sizeof(sRtcDate) ) ;
   sRtcDate.Year = i_psDateTime->byYear ;
   sRtcDate.Month = i_psDateTime->byMonth ;
   sRtcDate.Date = i_psDateTime->byDays ;

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
/* Date/time reading                                                          */
/*----------------------------------------------------------------------------*/

static void clk_ComGetDateTime( s_DateTime * o_psDateTime )
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
}


/*----------------------------------------------------------------------------*/
/* Get daylgigt saving status from one particular datetime                    */
/*----------------------------------------------------------------------------*/

static e_SummerState clk_GetSummerState( s_DateTime const* i_psDateTime )
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