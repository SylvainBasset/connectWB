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
/* Definitions                                                                */
/*----------------------------------------------------------------------------*/

#define CLK_ASYNC_PREDIV   64llu
#define CLK_SYNC_PREDIV   512llu

RTC_InitTypeDef const k_sRtcInit =
{
   .HourFormat = RTC_HOURFORMAT_24,
   .AsynchPrediv = CLK_ASYNC_PREDIV,
   .SynchPrediv = CLK_SYNC_PREDIV,
   .OutPut = RTC_OUTPUT_DISABLE,
   .OutPutRemap = RTC_OUTPUT_REMAP_NONE,
   .OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH,
   .OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN,
} ;

#define SET_RTC_HANDLE( handle ) \
   handle.Instance = RTC

#define SET_RTC_HANDLE_INIT( handle )                          \
   handle.Instance = RTC ;                                     \
   memcpy( &handle.Init, &k_sRtcInit, sizeof(handle.Init) )


DateTime const k_sDateTimeInit =
{
   .byYear = 0,
   .byMonth = 1,
   .byDate = 1,
   .byHours = 0,
   .byMinutes = 0,
   .bySeconds = 0,
} ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( DateTime C* i_psDateTime ) ;
static void clk_RtcInit( void ) ;
static void clk_32kHzInit( void ) ;


/*----------------------------------------------------------------------------*/
/* RTC init                                                                   */
/*----------------------------------------------------------------------------*/

void clk_Init( void )
{
   __HAL_RCC_BACKUPRESET_FORCE() ;
   __HAL_RCC_BACKUPRESET_RELEASE() ;

   clk_32kHzInit() ;

   clk_RtcInit() ;

   clk_ComSetDateTime( &k_sDateTimeInit ) ;
}


/*----------------------------------------------------------------------------*/
/* Date/time writing                                                          */
/*----------------------------------------------------------------------------*/

void clk_SetDateTime( DateTime C* i_psDateTime )
{
   clk_ComSetDateTime( i_psDateTime ) ;
}


/*----------------------------------------------------------------------------*/
/* Date/time reading                                                          */
/*----------------------------------------------------------------------------*/

void clk_GetDateTime( DateTime * o_psDateTime )
{
   RTC_HandleTypeDef sRtcHandle ;
   RTC_DateTypeDef sRtcDate ;
   RTC_TimeTypeDef sRtcTime ;

   SET_RTC_HANDLE( sRtcHandle ) ;

   HAL_RTC_GetTime( &sRtcHandle, &sRtcTime, RTC_FORMAT_BIN ) ;

   o_psDateTime->byHours = sRtcTime.Hours ;
   o_psDateTime->byMinutes = sRtcTime.Minutes ;
   o_psDateTime->bySeconds = sRtcTime.Seconds ;

   HAL_RTC_GetDate( &sRtcHandle, &sRtcDate, RTC_FORMAT_BIN ) ;

   o_psDateTime->byYear = sRtcDate.Year ;
   o_psDateTime->byMonth = sRtcDate.Month ;
   o_psDateTime->byDate = sRtcDate.Date ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Common function for date/time setting                                      */
/*----------------------------------------------------------------------------*/

static void clk_ComSetDateTime( DateTime C* i_psDateTime )
{
   RTC_HandleTypeDef sRtcHandle ;
   RTC_DateTypeDef sRtcDate ;
   RTC_TimeTypeDef sRtcTime ;

   SET_RTC_HANDLE( sRtcHandle ) ;

   sRtcDate.Year = i_psDateTime->byYear ;
   sRtcDate.Month = i_psDateTime->byMonth ;
   sRtcDate.Date = i_psDateTime->byDate ;

   HAL_RTC_SetDate( &sRtcHandle, &sRtcDate, RTC_FORMAT_BIN ) ;

   sRtcTime.Hours = i_psDateTime->byHours ;
   sRtcTime.Minutes = i_psDateTime->byMinutes ;
   sRtcTime.Seconds = i_psDateTime->bySeconds ;

   HAL_RTC_SetTime( &sRtcHandle, &sRtcTime, RTC_FORMAT_BIN ) ;
}


/*----------------------------------------------------------------------------*/
/* RTC module init                                                            */
/*----------------------------------------------------------------------------*/

static void clk_RtcInit( void )
{
   RTC_HandleTypeDef sRtcHandle ;

   __HAL_RCC_RTC_ENABLE() ;
   __HAL_RCC_RTC_CONFIG( RCC_RTCCLKSOURCE_LSE ) ;

   HAL_PWR_EnableBkUpAccess() ;

   SET_RTC_HANDLE_INIT( sRtcHandle ) ;

   HAL_RTC_Init( &sRtcHandle ) ;
}


/*----------------------------------------------------------------------------*/
/* 32 kHz clock init                                                          */
/*----------------------------------------------------------------------------*/

static void clk_32kHzInit( void )
{
   RCC_OscInitTypeDef sRtcClkInitStruct ;

                                       /* set LSE driving strenght to "medium low" */
   //SET_BIT( RCC->CSR, RCC_CSR_LSEDRV_0 ) ;
   //CLEAR_BIT( RCC->CSR, RCC_CSR_LSEDRV_1 ) ;
                                       /* Enable HSI Oscillator + PLL */
   sRtcClkInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE ;
   sRtcClkInitStruct.LSEState = RCC_LSE_ON ;
   sRtcClkInitStruct.PLL.PLLState = RCC_PLL_NONE ;
   HAL_RCC_OscConfig(&sRtcClkInitStruct) ;
}