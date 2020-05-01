/******************************************************************************/
/*                                  System.h                                  */
/******************************************************************************/
/*
   System header

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
   @history 1.0, 11 mar. 2018, creation
*/


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

#define NB_DAYS_WEEK        7          /* day numbers over the week */

void clk_Init( void ) ;
BOOL clk_IsDateTimeLost( void ) ;
SDWORD clk_GetCalib( DWORD * o_dwNbCalibActions ) ;

void clk_SetDateTime( s_DateTime C* i_psDateTime, BYTE i_byErrorSecMax ) ;
void clk_GetDateTime( s_DateTime * o_psDateTime, BYTE * o_pbyWeekday ) ;

BOOL clk_IsValid( s_DateTime C* i_psDateTime ) ;
BOOL clk_IsValidStr( CHAR C* i_pszDateTime, s_DateTime * o_psDateTime ) ;

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
   DWORD dwAutoAdjust ;
} s_CalData ;

typedef struct                         /* eeprom strcuture for wifi SSID and password */
{
   char szWifiSSID [128] ;             /* wifi SSID */
   char szWifiPassword [128] ;         /* wifi password */
   DWORD dwWifiSecurity ;              /* wifi security */
} s_WifiConInfo ;

typedef struct
{
   DWORD dwForceState ;
   DWORD dwCurrentMinStop ;
} s_ChargeStateData ;

typedef struct                         /* eeprom data structure definition */
{
   s_CalData sCalData ;                /* calendar module eeprom data */
   s_WifiConInfo sWifiConInfo ;        /* wifi SSID and password */
   s_ChargeStateData sChargeStateData ;
} s_DataEeprom ;

                                       /* global for eeprom data access */
#define g_sDataEeprom    ((s_DataEeprom *) DATA_EEPROM_BASE )

RESULT eep_WriteWifiId( BOOL i_bIsSsid, char C* i_szParam ) ;
void eep_write( DWORD i_dwAddress, DWORD i_dwValue ) ;


#endif /* __SYSTEM_H */