/******************************************************************************/
/*                                                                            */
/*                                  System.h                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:    11 mars 2018   Sylvain BASSET        Version 0.1            */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __SYSTEM_H                     /* to prevent recursive inclusion */
#define __SYSTEM_H


#define HSYS_CLK   32000000llu         /* System clock frequency */
#define AHB_CLK    32000000llu         /* AHB bus clock frequency */
#define APB1_CLK   32000000llu         /* APB1 bus clock frequency */
#define APB2_CLK   32000000llu         /* APB2 bus clock frequency */


/*----------------------------------------------------------------------------*/
/* Error.c                                                                    */
/*----------------------------------------------------------------------------*/

                                       /* fatal error macro */
#define ERR_FATAL() \
   err_FatalError()
                                       /* fatal error under condition macro */
#define ERR_FATAL_IF( cond ) \
   if ( cond )               \
   {                         \
      err_FatalError() ;     \
   }


void err_FatalError( void ) ;


/*----------------------------------------------------------------------------*/
/* Timer.c                                                                    */
/*----------------------------------------------------------------------------*/

void tim_StartMsTmp( DWORD* io_pdwTempo ) ;
void tim_StartSecTmp( DWORD* io_pdwTempo ) ;

BOOL tim_IsEndMsTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;
BOOL tim_IsEndSecTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;

DWORD tim_GetRemainMsTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;
DWORD tim_GetRemainSecTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;


/*----------------------------------------------------------------------------*/
/* Clock.c                                                                    */
/*----------------------------------------------------------------------------*/

#define NB_DAYS_WEEK   7               /* day numbers over the week */

void clk_Init( void ) ;
BOOL clk_IsDateTimeLost( void ) ;
void clk_GetDateTime( s_DateTime * o_psDateTime, BYTE * o_pbyWeekday ) ;
void clk_SetDateTime( s_DateTime const * i_psDateTime ) ;
void clk_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* Eeprom.c                                                                   */
/*----------------------------------------------------------------------------*/

   /* Note : Elements to be written in eeprom must be DWORD only. */
   /* See "flash HAL extended" to enable WORD/BYTE writing        */

typedef struct                         /* eeprom structure definition for calendar module */
{
   DWORD adwTimeSecStart [ NB_DAYS_WEEK ] ;
   DWORD adwTimeSecEnd [ NB_DAYS_WEEK ] ;
} s_CalData ;

typedef struct                         /* eeprom data structure definition */
{
   s_CalData sCalData ;                /* calendar module eeprom data */
} s_DataEeprom ;

                                       /* global for eeprom data access */
#define g_sDataEeprom    ((s_DataEeprom *) DATA_EEPROM_BASE )


void eep_write( DWORD i_dwAddress, DWORD i_dwValue ) ;


#endif /* __SYSTEM_H */