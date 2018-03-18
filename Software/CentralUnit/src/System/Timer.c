/******************************************************************************/
/*                                                                            */
/*                                  Timer.c                                   */
/*                                                                            */
/******************************************************************************/
/* Created on:   12 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "System.h"
#include "Main.h"


/*----------------------------------------------------------------------------*/
/* Definitions                                                                */
/*----------------------------------------------------------------------------*/

#define SYSTICK_FREQ    1000llu
#define TMP_MSB         0x80000000
#define TMP_START_FLAG  TMP_MSB


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static DWORD tim_ComRemainTmp( DWORD * io_pdwTempo, DWORD i_dwDelay, BOOL i_bIsMs ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static DWORD l_dwSecCnt ;


/*----------------------------------------------------------------------------*/
/* Start a millisecond-based temporisation                                    */
/*----------------------------------------------------------------------------*/

void tim_StartMsTmp( DWORD * io_pdwTempo )
{
   *io_pdwTempo = HAL_GetTick() ;      /* get millisecond counter value */
   *io_pdwTempo |= TMP_START_FLAG ;    /* add flag to indicate tempo is started */
}


/*----------------------------------------------------------------------------*/
/* Start a second-based temporisation                                         */
/*----------------------------------------------------------------------------*/

void tim_StartSecTmp( DWORD * io_pdwTempo )
{
   *io_pdwTempo = l_dwSecCnt ;          /* get second counter value */
   *io_pdwTempo |= TMP_START_FLAG ;     /* add flag to indicate tempo is started */
}


/*----------------------------------------------------------------------------*/
/* Test the end of a millisecond-based temporisation                          */
/*----------------------------------------------------------------------------*/

BOOL tim_IsEndMsTmp( DWORD * io_pdwTempo, DWORD i_dwDelay )
{
   BOOL bRet ;

   bRet = FALSE ;
                                       /* a started tempo is ending */
   if ( ( ISSET( *io_pdwTempo, TMP_START_FLAG ) ) &&
        ( tim_ComRemainTmp( io_pdwTempo, i_dwDelay, TRUE ) == 0 ) )
   {
      *io_pdwTempo = 0 ;               /* clear value, including #TMP_START_FLAG */
      bRet = TRUE ;                    /* tempo no longer started */
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Test the end of a second-based temporisation                               */
/*----------------------------------------------------------------------------*/

BOOL tim_IsEndSecTmp( DWORD * io_pdwTempo, DWORD i_dwDelay )
{
   BOOL bRet ;

   bRet = FALSE ;
                                       /* a started tempo is ending */
   if ( ( ISSET( *io_pdwTempo, TMP_START_FLAG ) ) &&
        ( tim_ComRemainTmp( io_pdwTempo, i_dwDelay, FALSE ) == 0 ) )
   {
      *io_pdwTempo = 0 ;               /* clear value, including #TMP_START_FLAG */
      bRet = TRUE ;                    /* tempo no longer started */
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Get remain time of a millisecond-based temporisation                       */
/*----------------------------------------------------------------------------*/

DWORD tim_GetRemainMsTmp( DWORD * io_pdwTempo, DWORD i_dwDelay )
{
   DWORD dwRemainTime ;
                                       /* get remain value */
   dwRemainTime = tim_ComRemainTmp( io_pdwTempo, i_dwDelay, TRUE ) ;

   return dwRemainTime ;
}


/*----------------------------------------------------------------------------*/
/* Get remain time of a second-based temporisation                            */
/*----------------------------------------------------------------------------*/

DWORD tim_GetRemainSecTmp( DWORD * io_pdwTempo, DWORD i_dwDelay )
{
   DWORD dwRemainTime ;
                                       /* get remain value */
   dwRemainTime = tim_ComRemainTmp( io_pdwTempo, i_dwDelay, FALSE ) ;

   return dwRemainTime ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Common for remain time calculation                                         */
/*----------------------------------------------------------------------------*/

static DWORD tim_ComRemainTmp( DWORD * io_pdwTempo, DWORD i_dwDelay, BOOL i_bIsMs )
{
   DWORD dwRet ;
   DWORD dwCurTime  ;
   DWORD dwElapsedTime ;

                                       /* test if delays overflows maximum */
   ERR_FATAL_IF( i_dwDelay >= ( TMP_START_FLAG - ( 2 * TASKCALL_PER_MS ) ) )

      /* Note: With the use of #TMP_START_FLAG as tempo runing state flag,    */
      /* it is not possible to count until #TMP_START_FLAG value (0x80000000) */
      /* (the start value of watched counter is coded over 31 bits).          */
      /* Moreover, for "near limit" temporisation delay, it is hard to catch  */
      /* the end of temporisation: tim_ComRemainTmp() must be called between  */
      /* end of temporisation and "Elapsed Time" overflow (which occurs at    */
      /* 0x80000000). So, as temporsation are mostly used in cyclic tasks,    */
      /* the delay limit is reduced from 2 times the task call period.        */

   dwRet = 0 ;                         /* Remaining time at 0 by default */
                                       /* is temporisation started ? */
   if ( ISSET( *io_pdwTempo, TMP_START_FLAG ) )
   {
      if ( i_bIsMs )                      /* is millisecond counter ? */
      {
         dwCurTime = HAL_GetTick() ;      /* get millisecond counter */
      }
      else
      {
         dwCurTime = l_dwSecCnt ;         /* get second counter */
      }
                                          /* calculate elapsed time without */
                                          /* #TMP_START_FLAG bit  */
      dwElapsedTime = ( ( dwCurTime & ~TMP_START_FLAG ) -
                        ( *io_pdwTempo & ~TMP_START_FLAG ) ) ;

                                          /* does substartction overflows ? */
      if ( ISSET( dwElapsedTime, TMP_MSB ) )
      {
          dwElapsedTime &= ~TMP_MSB ;     /* clear MSB to get the absolute value */
      }

      if ( dwElapsedTime < i_dwDelay )    /* elapsed time doesn't reachs delay ? */
      {                                   /* calculate difference between */
                                          /* elapsed time and delay*/
         dwRet = i_dwDelay - dwElapsedTime ;
      }
         /* Note: if elapsed time reachs delay, dwRet */
         /* (remaining time) equals 0 by default.     */
   }

   return dwRet ;
}


/*----------------------------------------------------------------------------*/
/* IRQ SysTick                                                                */
/*----------------------------------------------------------------------------*/

void SysTick_Handler(void)
{
   HAL_IncTick();                         /* HAL milli-second counter increment */
                                          /* is thousand multiple ? */
   if ( HAL_GetTick() % SYSTICK_FREQ == 0 )
   {
      l_dwSecCnt++ ;                      /* second counter increment */
   }
}