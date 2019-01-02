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

#define  HTML_PAGE_STATUS        0
#define  HTML_PAGE_CALENDAR      1
#define  HTML_PAGE_WIFI          2


#define  HTML_CALENDAR_SSI_WEEKDAY          0
#define  HTML_CALENDAR_SSI_DAY              1
#define  HTML_CALENDAR_SSI_MONTH            2
#define  HTML_CALENDAR_SSI_YEAR             3
#define  HTML_CALENDAR_SSI_HOURS            4
#define  HTML_CALENDAR_SSI_MINUTES          5
#define  HTML_CALENDAR_SSI_SECONDS          6

#define  HTML_CALENDAR_SSI_CAL_MONDAY       7
#define  HTML_CALENDAR_SSI_CAL_TUESDAY      8
#define  HTML_CALENDAR_SSI_CAL_WEDNESDAY    9
#define  HTML_CALENDAR_SSI_CAL_THURSDAY    10
#define  HTML_CALENDAR_SSI_CAL_FRIDAY      11
#define  HTML_CALENDAR_SSI_CAL_SATURDAY    12
#define  HTML_CALENDAR_SSI_CAL_SUNDAY      13


#define HTML_CALENDAR_CGI_DATE              0
#define HTML_CALENDAR_CGI_DAYSET            1



typedef void (*f_htmlSsi)( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutPut, WORD i_byOutSize ) ;
typedef void (*f_htmlCgi)( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;

void html_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void ) ;

void cwifi_RegisterScktFunc( f_ScktGetFrame i_fScktGetFrame, f_ScktGetResExt i_fScktGetResExt ) ;
void cwifi_RegisterHtmlFunc( f_htmlSsi i_fHtmlSsi, f_htmlCgi i_fHtmlCgi ) ;

void cwifi_AskForRestart( void ) ;
void cwifi_SetMaintMode( BOOL i_bMaintmode ) ;
BOOL cwifi_IsConnected( void ) ;
BOOL cwifi_IsSocketConnected( void ) ;
BOOL cwifi_IsMaintMode( void ) ;

void cwifi_AddExtCmd( char C* i_szStrCmd ) ;
void cwifi_AddExtData( char C* i_szStrCmd ) ;
void cwifi_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* UartWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

#define UWIFI_ERROR_RX        1u
#define UWIFI_ERROR_DMA_TX    2u
#define UWIFI_ERROR_DMA_RX    4u

void uwifi_Init( void ) ;
WORD uwifi_Read( BYTE * o_pbyData, WORD i_dwMaxSize, BOOL i_bGetPending ) ;
BOOL uwifi_Send( void const* i_pvData, DWORD i_dwSize ) ;
BOOL uWifi_IsSendDone( void ) ;
void uwifi_SetErrorDetection( BOOL i_bEnable ) ;
BYTE uwifi_GetError( BOOL i_bReset ) ;


/*----------------------------------------------------------------------------*/
/* CommOEvse.c                                                                 */
/*----------------------------------------------------------------------------*/

#define COEVSE_DATA_ITEM_SIZE    100

typedef struct
{
   BOOL bEvConnect ;
   SDWORD sdwCurrentCapMin ;
   SDWORD sdwCurrentCapMax ;
   SDWORD sdwChargeVoltage ;
   SDWORD sdwChargeCurrent ;
   SDWORD sdwCurWh ;
   SDWORD sdwAccWh ;

   DWORD dwErrGfiTripCnt ;
   DWORD dwNoGndTripCnt ;
   DWORD dwStuckRelayTripCnt ;
} s_coevseStatus ;

void coevse_Init( void ) ;

void coevse_SetEnable( BOOL i_bEnable ) ;
void coevse_SetCurrentCap( WORD i_wCurrent ) ;

BOOL coevse_IsPlugged( void ) ;
BOOL coevse_IsCharging( void ) ;
s_coevseStatus coevse_GetStatus( void ) ;

void coevse_TaskCyc( void ) ;

#endif /* __COMMUNIC_H */
