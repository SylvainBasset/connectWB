/******************************************************************************/
/*                                 Communic.h                                 */
/******************************************************************************/
/*
   communications header

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
   @history 1.0, 08 apr. 2018, creation
*/


#ifndef __COMMUNIC_H                   /* to prevent recursive inclusion */
#define __COMMUNIC_H


/*----------------------------------------------------------------------------*/
/* ScktFrame.c                                                                */
/*----------------------------------------------------------------------------*/

typedef void (*f_ScktDataProc)( char * i_pszFrame ) ;
typedef void (*f_PostResProc)( char C* i_pszResExt, BOOL i_bLastCall ) ;

void sfrm_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* HtmlInfo.c                                                                 */
/*----------------------------------------------------------------------------*/

typedef void (*f_htmlSsi)( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutPut, WORD i_byOutSize ) ;
typedef void (*f_htmlCgi)( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;

void html_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void ) ;

void cwifi_RegisterScktFunc( f_ScktDataProc i_fScktDataProc, f_PostResProc i_fPostResProc ) ;
void cwifi_RegisterHtmlFunc( f_htmlSsi i_fHtmlSsi, f_htmlCgi i_fHtmlCgi ) ;

void cwifi_SetMaintMode( BOOL i_bMaintmode ) ;
BOOL cwifi_IsConnected( void ) ;
BOOL cwifi_IsSocketConnected( void ) ;
BOOL cwifi_IsMaintMode( void ) ;

RESULT cwifi_AddExtCmd( char C* i_szStrCmd ) ;
void cwifi_AddExtData( char C* i_szStrData ) ;
void cwifi_AskFlushData( void ) ;
void cwifi_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* UartWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

#define UWIFI_ERROR_RX        1u        /* UART reception error */
#define UWIFI_ERROR_DMA_TX    2u        /* transmission DMA error */
#define UWIFI_ERROR_DMA_RX    4u        /* reception DMA error */

void uwifi_Init( void ) ;
WORD uwifi_Read( BYTE * o_pbyData, WORD i_dwMaxSize, BOOL i_bGetPending ) ;
BOOL uwifi_Send( void C* i_pvData, DWORD i_dwSize ) ;
DWORD uwifi_GetRemainingSend( void ) ;
BOOL uwifi_IsSendDone( void ) ;
void uwifi_SetErrorDetection( BOOL i_bEnable ) ;
BYTE uwifi_GetError( BOOL i_bReset ) ;


/*----------------------------------------------------------------------------*/
/* CommOEvse.c                                                                 */
/*----------------------------------------------------------------------------*/

typedef enum                                    /* EVSE state */
{
   COEVSE_STATE_UNKNOWN = 0,
   COEVSE_STATE_NOTCONNECTED,
   COEVSE_STATE_CONNECTED,
   COEVSE_STATE_CHARGING,
} e_coevseEvseState ;

#define COEVSE_CURRENT_CAPMAX_MAX   16			/* maximum current capacity */
#define COEVSE_CURRENT_CAPMAX_MIN    6			/* minimum current capacity */


void coevse_Init( void ) ;
void coevse_RegisterRetScktFunc( f_PostResProc i_fPostResProc ) ;

void coevse_SetChargeEnable( BOOL i_bEnable ) ;
void coevse_SetCurrentCap( BYTE i_byCurrent ) ;
DWORD coevse_GetCurrentCap( void ) ;

e_coevseEvseState coevse_GetEvseState( void ) ;
BOOL coevse_IsPlugEvent( BOOL i_bReset ) ;
SDWORD coevse_GetCurrent( void ) ;
SDWORD coevse_GetVoltage( void ) ;
DWORD coevse_GetEnergy( void ) ;
void coevse_FmtInfo( CHAR * o_pszInfo, WORD i_wSize ) ;

RESULT coevse_AddExtCmd( char C* i_szStrCmd ) ;

void coevse_TaskCyc( void ) ;

#endif /* __COMMUNIC_H */
