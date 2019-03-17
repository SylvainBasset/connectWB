/******************************************************************************/
/*                                                                            */
/*                                 Communic.h                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:    8 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __COMMUNIC_H                   /* to prevent recursive inclusion */
#define __COMMUNIC_H


/*----------------------------------------------------------------------------*/
/* ScktFrame.c                                                                */
/*----------------------------------------------------------------------------*/

typedef void (*f_ScktGetFrame)( char * i_pszFrame ) ;
typedef void (*f_ScktGetResExt)( char C* i_pszResExt, BOOL i_bLastCall ) ;

void sfrm_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* HtmlInfo.c                                                                 */
/*----------------------------------------------------------------------------*/

#define HTML_PAGE_STATUS        0
#define HTML_PAGE_CALENDAR      1
#define HTML_PAGE_WIFI          2

#define HTML_CHARGE_SSI_FORCE             0
#define HTML_CHARGE_SSI_CALENDAR_OK       1
#define HTML_CHARGE_SSI_EVPLUGGED         2
#define HTML_CHARGE_SSI_STATE             3
#define HTML_CHARGE_SSI_CURRENT_MES       4
#define HTML_CHARGE_SSI_VOLTAGE_MES       5
#define HTML_CHARGE_SSI_ENERGY_MES        6
#define HTML_CHARGE_SSI_CURRENT_CAP       7
#define HTML_CHARGE_SSI_CURRENT_MIN       8

#define HTML_CALENDAR_SSI_DATETIME        0
#define HTML_CALENDAR_SSI_CAL_MONDAY      1
#define HTML_CALENDAR_SSI_CAL_TUESDAY     2
#define HTML_CALENDAR_SSI_CAL_WEDNESDAY   3
#define HTML_CALENDAR_SSI_CAL_THURSDAY    4
#define HTML_CALENDAR_SSI_CAL_FRIDAY      5
#define HTML_CALENDAR_SSI_CAL_SATURDAY    6
#define HTML_CALENDAR_SSI_CAL_SUNDAY      7

#define HTML_WIFI_SSI_WIFIHOME            0
#define HTML_WIFI_SSI_SECURITY            1
#define HTML_WIFI_SSI_MAINTMODE           2

#define HTML_CHARGE_CGI_CURRENT_CAPMAX    0
#define HTML_CHARGE_CGI_CURRENT_MIN       1
#define HTML_CHARGE_CGI_TOGGLE_FORCE      2

#define HTML_CALENDAR_CGI_DATE            0
#define HTML_CALENDAR_CGI_DAYSET          1

#define HTML_WIFI_CGI_WIFI                0



typedef void (*f_htmlSsi)( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutPut, WORD i_byOutSize ) ;
typedef void (*f_htmlCgi)( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;

void html_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void ) ;
void cwifi_EnterSaveMode( void ) ;

void cwifi_RegisterScktFunc( f_ScktGetFrame i_fScktGetFrame, f_ScktGetResExt i_fScktGetResExt ) ;
void cwifi_RegisterHtmlFunc( f_htmlSsi i_fHtmlSsi, f_htmlCgi i_fHtmlCgi ) ;

void cwifi_SetMaintMode( BOOL i_bMaintmode ) ;
BOOL cwifi_IsConnected( void ) ;
BOOL cwifi_IsSocketConnected( void ) ;
BOOL cwifi_IsMaintMode( void ) ;

RESULT cwifi_AddExtCmd( char C* i_szStrCmd ) ;
void cwifi_AddExtData( char C* i_szStrCmd ) ;
void cwifi_AskFlushData( void ) ;
void cwifi_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* UartWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

#define UWIFI_ERROR_RX        1u
#define UWIFI_ERROR_DMA_TX    2u
#define UWIFI_ERROR_DMA_RX    4u

void uwifi_Init( void ) ;
WORD uwifi_Read( BYTE * o_pbyData, WORD i_dwMaxSize, BOOL i_bGetPending ) ;
BOOL uwifi_Send( void C* i_pvData, DWORD i_dwSize ) ;
BOOL uWifi_IsSendDone( void ) ;
void uwifi_SetErrorDetection( BOOL i_bEnable ) ;
BYTE uwifi_GetError( BOOL i_bReset ) ;


/*----------------------------------------------------------------------------*/
/* CommOEvse.c                                                                 */
/*----------------------------------------------------------------------------*/

typedef enum
{
   COEVSE_EV_PLUGGED,
   COEVSE_EV_UNPLUGGED,
   COEVSE_EV_UNKNOWN,
} e_coevseEVPlugState ;

typedef struct
{
   e_coevseEVPlugState eEvPlugState ;
   DWORD dwCurrentCapMin ;
   DWORD dwCurrentCap ;
   SDWORD sdwChargeVoltage ;
   SDWORD sdwChargeCurrent ;
   DWORD dwCurWh ;
   DWORD dwAccWh ;

   DWORD dwErrGfiTripCnt ;
   DWORD dwNoGndTripCnt ;
   DWORD dwStuckRelayTripCnt ;
} s_coevseStatus ; //SBA: renommer en Data

#define COEVSE_CURRENT_CAPMAX_MAX   16
#define COEVSE_CURRENT_CAPMAX_MIN    6


void coevse_Init( void ) ;
void coevse_RegisterScktFunc( f_ScktGetResExt i_fScktGetResExt ) ;

void coevse_SetEnable( BOOL i_bEnable ) ;
void coevse_SetCurrentCap( BYTE i_byCurrent ) ;
DWORD coevse_GetCurrentCap( void ) ;

e_coevseEVPlugState coevse_GetPlugState( void ) ;
BOOL coevse_IsCharging( void ) ;
SDWORD coevse_GetCurrent( void ) ;
SDWORD coevse_GetVoltage( void ) ;
DWORD coevse_GetEnergy( void ) ;
void coevse_FmtInfo( CHAR * o_pszInfo, WORD i_wSize ) ;
s_coevseStatus coevse_GetStatus( void ) ;

RESULT coevse_AddExtCmd( char C* i_szStrCmd ) ;

void coevse_TaskCyc( void ) ;

#endif /* __COMMUNIC_H */
