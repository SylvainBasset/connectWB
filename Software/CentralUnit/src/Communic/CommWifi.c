/******************************************************************************/
/*                                  CommWifi.c                                */
/******************************************************************************/
/*
   Wifi communication module

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
   @history 1.0, 07 apr. 2018, creation
   @brief
   Implement communication with IDW01M1 wifi daughter board.

   Communication is done by AT frames throught the UartWifi module.
   Frames are terminated by '\r\n' characters.
   There are 2 kind of AT frames:

   - Commands : In this case this moduile is the initiator, it sends
   a command, and wait for WIFI module responce. the response format
   is 'OK' or 'ERROR' (depnding of command traitement) maybe followed
   by data (depending on the AT command). This module use a reduced
   set of command, described by LIST_CMD() macro.
   The result (string) may be stored in l_szResp... static variable (if the
   command isdescribed by Opr()), or even treated by a cwifi_CmdCallBack...
   callbacks Opf() macro) which can handle command's response. Some command
   may not request responses from the wifi module (see last argument of LIST_CMD()
   structure).
   Commands are sent by calling cwifi_AddCmdFifo(). They are stored in FIFO
   (l_CmdFifo). If FIFO contain at least 1 element and the command sending is
   ready (Wifi module ready and no pending command) the last FIFO command is
   sent.
   When a response is required, a timeout duration of CWIFI_CMD_TIMEOUT ms
   is checked.

   - Wind message : these frame are sent asynchonously by the module,
   to describe an event. Only Wind event described by LIST_WIND() macro are
   treated. The wind data (string) may be stored in l_szWind... static variables
   (if the command is described by Opr()), or even treated by a
   cwifi_CmdCallBack... callbacks (Opf() macro)) which can handle command's
   response.
   The LIST_WIND() can also define varaible adresse (see n-1 arg) and a value
   (last arg). If the addresse is not NULL, the pointed variable is loaded with
   the value at each corresponding Wind event.

   At system initialisation, the wifi module is reseted. Then it may need time
   to initialize. Once initialized (booleans l_bPowerOn,... set by the reception
   of wind commands), wifi configuration is sent to the module
   (see cwifi_ConnectFSM()), and the module wait for the connection (booelan l_bWifiUp)

   The system can connect to a home wifi (standard mode) or create a standalone wifi
   (maintenance mode). The type of connection can be set by cwifi_SetMaintMode().

   Once a connection is established, it is possible to send and receive data via
   sockets. Socket (data) mode is activated by sending a CWIFI_C_CMDTODATA command
   ("AT+S."). In data mode, all charaters wrote to the UART are sent to the socket.
   To leave data mode, the string "at+s." followed by a silence (~100ms) must be sent.

   Socket data are stored in the buffer l_DataBuf by calling buffercwifi_AddExtData()
   Then, cwifi_AskFlushData() is used to flush data into socket.
   The caller must add the "at+s." before flushing in order to return to command mode.
   In any case, if no data are sent during CWIFI_DATAMODE_TIMEOUT ms, the module returns
   to command mode.

   To wind message are used for HTTP control :

   - CGI message, which act as a wind message but using "+GCI:" prefix, are used to
   monitor item modification on the HTTP page. Two identifier, following the prefix,
   and separated by ":" are used to identify the modified item. These informations
   are given to HtmlInfo.c module.

   - wind INPUT message are used to send dynamic information to the HTML server.
   This message is followed by two identifier, separated by ":" (like CGI), but
   without the final '\r\n'. In this case, data is asked to HtmlInfo.c (given the
   identifiers) and immedialtly sent to the UART with the final '\r\n'.
*/


#include "Define.h"
#include "Communic.h"
#include "Communic/l_Communic.h"
#include "Control.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/



#define CWIFI_RESET_DURATION      200        /* wifi module reset duration (ms) */
#define CWIFI_PWRUP_DURATION        1        /* duration to wait after reset release (ms) */

#define CWIFI_CMD_TIMEOUT       30000        /* timeout temporisation to receive response
                                                from command (ms) */
#define CWIFI_DATAMODE_TIMEOUT  30000        /* timeout temporisation to exit data mode if no
                                                data is sent (ms) */
#define CWIFI_MAINT_TIMEOUT       300        /* maintenance mode activation duration (sec) */

#define CWIFI_ACT_PERIOD          60         /* Wifi actions (scan+date/time) period, sec */

#define CWIFI_INPUT_SEND_TIMEOUT  100        /* timeout temporisation for UART transmission for
                                                INPUT callaback (ms) */

#define CWIFI_WIND_PREFIX        "+WIND:"    /* WIND message prefix */
#define CWIFI_CGI_PREFIX         "+CGI:"     /* CGI message prefix */

#define CWIFI_RESP_OK            "OK\r\n"    /* valid response */
#define CWIFI_RESP_ERR           "ERROR"     /* error response */

                                             /* SSID name in maintenance mode */
#define CWIFI_MAINT_SSID         "WallyBox_Maint"
                                             /* password for maintenance mode : "superpasteque" */
#define CWIFI_MAINT_PWD          "73757065727061737465717565"


typedef RESULT (*f_WindCallback)( char C* i_pszProcessData, BOOL i_bPendingData ) ;

typedef RESULT (*f_CmdCallback)( char C* i_pszProcessData ) ;

typedef struct                               /* Wind description */
{
   char szWindNum [4] ;                      /* name string */
   BOOL * pbVar ;                            /* addr of boolaan to modify, NULL if no boolean */
   BOOL bValue ;                             /* value assigned if pbVar != NULL (TRUE or FALSE) */
   char * pszStrContent ;                    /* addr of the string to store content (NULL if not needed) */
   WORD wContentSize ;                       /* callback address */
   f_WindCallback fCallback ;
} s_WindDesc ;

typedef struct                               /* command description */
{
   char szCmdFmt [32] ;                      /* command format string */
   BOOL bIsResult ;                          /* requested response indicator */
   char * pszStrContent ;                    /* addr of the string to store response content (NULL if not needed) */
   WORD wContentSize ;                       /* maximum size for content string */
   f_CmdCallback fCallback ;                 /* callback address */
} s_CmdDesc ;

typedef enum                                 /* command processing status */
{
   CWIFI_CMDST_NONE,                         /* no command processing */
   CWIFI_CMDST_PROCESSING,                   /* a command sending is in progress (waiting for response) */
   CWIFI_CMDST_END_OK,                       /* command (and response) is done without error (transcient state) */
   CWIFI_CMDST_END_ERR,                      /* command (and response) is done with error (transcient state) */
} e_CmdStatus ;

typedef enum                                 /* Wifi module state */
{
   CWIFI_STATE_OFF = 0,                      /* Off : hardware is not started */
   CWIFI_STATE_IDLE,                         /* Idle : configuration is processing */
   CWIFI_STATE_CONNECTING,                   /* Waiting for wifi connexion (home wifi or stand-alone AP) */
   CWIFI_STATE_CONNECTED,                    /* connection done (at least once) */
} e_WifiState ;


/*----------------------------------------------------------------------------*/
/* Wind table definition                                                      */
/*----------------------------------------------------------------------------*/

                                             /* boolean for wind states */
static BOOL l_bPowerOn ;
static BOOL l_bConsoleRdy ;
static BOOL l_bHrdStarted ;
static BOOL l_bWifiUp ;
static BOOL l_bSocketConnected ;
static BOOL l_bDataMode ;


static char l_szWindWifiUpIp[32] ;           /* device current IP */
static char l_szWindSocketConIp[16] ;        /* IP for connected socket */

   /* Note : Macro mecanism :                                              */
   /* Op()  = Simple wind frame                                            */
   /* Opr() = wind frame with response string to be stored (l_szWindxxx)   */
   /* Opf() = wind frame with callback function (cwifi_WindCallBackxxx())  */
   /* Arg1 = command name upper case                                       */
   /* Arg2 = command name lower case                                       */
   /* Arg3 = wind number string                                            */
   /* Arg4 = Boolean varaible to be modified at wind reception             */
   /* Arg5 = valeur to be written in (Arg4) at wind reception              */

                                             /* general macro for handled wind messages */
#define LIST_WIND( Op, Opr, Opf ) \
   Op(  CONSOLE_RDY, ConsoleRdy,  "0:",  &l_bConsoleRdy,      TRUE  ) \
   Opf( POWER_ON,    PowerOn,     "1:",  &l_bPowerOn,         TRUE  ) \
   Opf( RESET,       Reset,       "2:",  NULL,                0     ) \
   Op(  HRD_STARTED, HrdStarted,  "32:", &l_bHrdStarted,      TRUE  ) \
   Opr( WIFI_UP_IP,  WifiUpIp,    "24:", &l_bWifiUp,          TRUE  ) \
   Opf( INPUT,       Input,       "56:", NULL,                0     ) \
   Opf( CMDMODE,     CmdMode,     "59:", &l_bDataMode,        FALSE ) \
   Opf( DATAMODE,    DataMode,    "60:", &l_bDataMode,        TRUE  ) \
   Opr( SOCKETCONIP, SocketConIp, "61:", &l_bSocketConnected, TRUE  ) \
   Op(  SOCKETDIS,   SocketDis,   "62:", &l_bSocketConnected, FALSE ) \
   Opf( SOCKETDATA,  SocketData,  "64:", NULL,                0     )

typedef enum                                 /* Wind identifiers (not used for now) */
{
   CWIFI_WIND_NONE = 0,
   LIST_WIND( CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM )
   CWIFI_WIND_LAST
} e_WindId ;
                                             /* Wind callback definition */
LIST_WIND( CWIFI_W_NULL, CWIFI_W_NULL, CWIFI_W_CALLBACK )

                                             /* list of wind descriptor */
static s_WindDesc const k_aWindDesc [] =
{
   LIST_WIND( CWIFI_W_OPER, CWIFI_W_OPER_R, CWIFI_W_OPER_F )
} ;


/*----------------------------------------------------------------------------*/
/* Commands/responses table definition                                        */
/*----------------------------------------------------------------------------*/

static char l_szRespPing [1] ;               /* ping result (not used for now)  */
static char l_szRespGCfg [1] ;               /* Conf getter result  (not used for now)  */

   /* Note : Macro mecanism :                                         */
   /* Op()  = Simple command                                          */
   /* Opr() = Command with response string to be stored l_szRespxxx   */
   /* Opf() = Command with callback function (cwifi_CmdCallBackxxx()) */
   /* Arg1 = command name upper case                                  */
   /* Arg2 = command name lower case                                  */
   /* Arg3 = string formatting                                        */
   /* Arg4 = needed response (from Wifi board) indicator              */

                                             /* general macro for handled commands */
#define LIST_CMD( Op, Opr, Opf )                              \
   Op(  AT,        At,        "AT\r",                 TRUE  ) \
   Op(  SCFG,      SCfg,      "AT+S.SCFG=%s,%s\r",    TRUE  ) \
   Opr( GCFG,      GCfg,      "AT+S.GCFG=%s\r",       TRUE  ) \
   Op(  SETSSID,   SetSsid,   "AT+S.SSIDTXT=%s\r",    TRUE  ) \
   Op(  CFUN,      CFun,      "AT+CFUN=%s\r",         FALSE ) \
   Op(  SAVE,      Save,      "AT&W\r",               TRUE  ) \
   Op(  FACTRESET, FactReset, "AT&F\r",               TRUE  ) \
   Opr( PING,      Ping,      "AT+S.PING=%s\r",       TRUE  ) \
   Op(  SOCKD,     Sockd,     "AT+S.SOCKD=%s\r",      TRUE  ) \
   Op(  CMDTODATA, CmdToData, "AT+S.\r",              FALSE ) \
   Op(  FSL,       Fsl,       "AT+S.FSL\r",           TRUE  ) \
   Op(  SCAN,      Scan,      "AT+S.SCAN=a,m,%s\r",   TRUE  ) \
   Opf( HTTPGET,   HttpGet,   "AT+S.HTTPGET=%s,%s\r", TRUE  ) \
   Opf( EXT,       Ext,       "",                     TRUE  ) \

typedef enum                                 /* Command identifiers */
{
   CWIFI_CMD_NONE = 0,
   LIST_CMD( CWIFI_C_ENUM, CWIFI_C_ENUM, CWIFI_C_ENUM )
   CWIFI_CMD_LAST,
} e_CmdId ;
                                             /* Responses (from command) callback definition */
LIST_CMD( CWIFI_C_NULL, CWIFI_C_NULL, CWIFI_C_CALLBACK )

                                             /* list of command descriptor */
static s_CmdDesc const k_aCmdDesc [] =
{
   LIST_CMD( CWIFI_C_OPER, CWIFI_C_OPER_R, CWIFI_C_OPER_F )
} ;

typedef struct                               /* command/response datas */
{
   e_CmdId eCmdId ;                          /* current command ID (CWIFI_CMD_NONE if no command is processing) */
   e_CmdStatus eStatus ;                     /* command status */
   WORD wStrContentIdx ;                     /* string index to store response content (eg. l_szRespGCfg) */
   DWORD dwTmpCmdTimeout ;                   /* command/response timeout */
} s_CmdCurData ;

typedef struct                               /* command FIFO's item */
{
   e_CmdId eCmdId ;                          /* command ID */
   char szStrCmd [128] ;                     /* command string (already formatted) to send */
} s_CmdItem ;

typedef struct                               /* command FIFO */
{
   BYTE byIdxIn ;                            /* input index */
   BYTE byIdxOut ;                           /* output index */
   s_CmdItem aCmdItems [10] ;                /* FIFO elements */
} s_CmdFifo ;


#define CWIFI_DATABUF_SIZE  1024             /* socket data buffer size, in bytes */

typedef struct                               /* socket data buffer (ping/pong buffer) */
{
   BOOL bAskFlush ;                          /* ask for fushing indicator */
   WORD wNbChar ;                            /* number of char stored in buffer */
   WORD wNbCharInTx ;                        /* number of byte currently in DMA transfer */
                                             /* 0 if no DMA transfer pending */
   CHAR sDataBuf [CWIFI_DATABUF_SIZE] ;      /* buffer item 0 */
} s_DataBuf ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cwifi_ConnectFSM( void ) ;
static void wifi_DoSetMaintMode( BOOL i_bMaintmode ) ;

static RESULT cwifi_FmtAddCmdFifo( e_CmdId i_eCmdId, char C* i_szArg1,
                                                     char C* i_szArg2 ) ;
static RESULT cwifi_AddCmdFifo( e_CmdId i_eCmdId, char C* i_szStrCmd ) ;
static void cwifi_ExecSendCmd( void ) ;

static void cwifi_AddDataBuffer( char C* i_szStrData ) ;
static void cwifi_ExecSendData( void ) ;

static void cwifi_ProcessRec( void ) ;
static void cwifi_ProcessRecWind( char * io_pszProcessData, BOOL i_bPendingData  ) ;
static void cwifi_ProcessRecCgi( char C* i_pszProcessData ) ;
static void cwifi_ProcessRecResp( char * io_pszProcessData ) ;

static char C* cwifi_RSplit( char C* i_pszStr, char C* i_pszDelim ) ;
static void cwifi_ResetVar( void ) ;

static void cwifi_HrdInit( void ) ;
static void cwifi_HrdSetResetModule( BOOL i_bReset );


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_WifiState l_eWifiState ;      /* Wifi module state */
static s_CmdCurData l_CmdCurStatus ;   /* command/response datas */

static DWORD l_dwTmpDataMode ;         /* data mode (socket) timeout */
static DWORD l_dwTmpMaintMode ;        /* maintenance mode timeout */
static DWORD l_dwTmpScan ;             /* temporisation for periodic scan */

static BOOL l_bMaintMode ;             /* maintenance mode indicator */
static BOOL l_bConfigDone ;            /* all config command have been sent */
static BOOL l_bCmdToDataInFifo ;       /* data (socket) open command sent to commands FIFO */

static BOOL l_bInhPendingData ;        /* pending data (not complete message) processing is inhibited */

static f_ScktDataProc l_fScktDataProc ; /* data (socket) processing callback */
static f_PostResProc l_fPostResProc ;  /* external command's response callback */

static f_htmlSsi l_fHtmlSsi ;          /* SSI callback */
static f_htmlCgi l_fHtmlCgi ;          /* CGI callback */

static s_CmdFifo l_CmdFifo ;           /* command FIFO */
static s_DataBuf l_DataBuf ;           /* data (socket) buffer */


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void )
{
   cwifi_HrdInit() ;
   cwifi_HrdSetResetModule( TRUE ) ;
   cwifi_HrdSetResetModule( FALSE ) ;
   cwifi_ResetVar() ;
   l_bMaintMode = FALSE ;
   l_bConfigDone = FALSE ;
}


/*----------------------------------------------------------------------------*/
/* Activate Wifi power save state (all reset)                                 */
/*----------------------------------------------------------------------------*/

void cwifi_EnterSaveMode( void )
{
   cwifi_HrdSetResetModule( TRUE ) ;
   cwifi_ResetVar() ;
   l_bMaintMode = FALSE ;
   l_bConfigDone = FALSE ;
}


/*----------------------------------------------------------------------------*/
/* register data/external responses callbacks                                 */
/*----------------------------------------------------------------------------*/

void cwifi_RegisterScktFunc( f_ScktDataProc i_fScktDataProc, f_PostResProc i_fPostResProc )
{
   l_fScktDataProc = i_fScktDataProc ;
   l_fPostResProc = i_fPostResProc ;
}


/*----------------------------------------------------------------------------*/
/* register SSI/CGI callbacks                                                 */
/*----------------------------------------------------------------------------*/

void cwifi_RegisterHtmlFunc( f_htmlSsi i_fHtmlSsi, f_htmlCgi i_fHtmlCgi )
{
   l_fHtmlSsi = i_fHtmlSsi ;
   l_fHtmlCgi = i_fHtmlCgi ;
}


/*----------------------------------------------------------------------------*/
/* maintenance mode setting                                                   */
/*----------------------------------------------------------------------------*/

void cwifi_SetMaintMode( BOOL i_bMaintmode )
{
   wifi_DoSetMaintMode( i_bMaintmode ) ;
}


/*----------------------------------------------------------------------------*/
/* test if wifi is connected (to home netword or stand-alone AP)              */
/*----------------------------------------------------------------------------*/

BOOL cwifi_IsConnected( void )
{
   BOOL bConnect ;

   bConnect = ( l_eWifiState == CWIFI_STATE_CONNECTED ) ;

   return bConnect ;
}


/*----------------------------------------------------------------------------*/
/* test if socket is connected                                                */
/*----------------------------------------------------------------------------*/

BOOL cwifi_IsSocketConnected( void )
{
   return l_bSocketConnected ;
}


/*----------------------------------------------------------------------------*/
/* test if maintenance mode is on                                             */
/*----------------------------------------------------------------------------*/

BOOL cwifi_IsMaintMode( void )
{
   return l_bMaintMode ;
}


/*----------------------------------------------------------------------------*/
/* add external command to command's FIFO                                     */
/*----------------------------------------------------------------------------*/

RESULT cwifi_AddExtCmd( char C* i_szStrCmd )
{
   return cwifi_AddCmdFifo( CWIFI_CMD_EXT, i_szStrCmd ) ;
}


/*----------------------------------------------------------------------------*/
/* Public add external data to data (socket) buffer                           */
/*----------------------------------------------------------------------------*/

void cwifi_AddExtData( char C* i_szStrData )
{
   cwifi_AddDataBuffer( i_szStrData ) ;
}


/*----------------------------------------------------------------------------*/
/* ask for flushing data (socket) buffer                                      */
/*----------------------------------------------------------------------------*/

void cwifi_AskFlushData( void )
{
   l_DataBuf.bAskFlush = TRUE ;
}


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 10 msec )                                           */
/*----------------------------------------------------------------------------*/

void cwifi_TaskCyc( void )
{
   cwifi_ProcessRec() ;

   if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_ERR )
   {
      l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
      l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;
   }
   else if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_OK )
   {
      l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
      l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;
   }
   else           /* l_CmdCurStatus.eStatus == NONE or PROCESSING */
   {
   }

   cwifi_ConnectFSM() ;

   if ( l_bDataMode )
   {
      if ( tim_IsEndMsTmp( &l_dwTmpDataMode, CWIFI_DATAMODE_TIMEOUT ) )
      {
         cwifi_AddDataBuffer( "at+s." ) ;
         l_DataBuf.bAskFlush = TRUE ;
      }
   }

   if ( l_DataBuf.wNbCharInTx != 0 )
   {
      if ( uwifi_IsSendDone() )
      {
         l_DataBuf.wNbCharInTx = 0 ;
      }
   }
   else
   {
      if ( l_bDataMode )
      {
         cwifi_ExecSendData() ;
         l_bCmdToDataInFifo = FALSE ;
      }
      else
      {
         if ( l_bSocketConnected )
         {
            if ( ( l_DataBuf.bAskFlush ) && ( ! l_bCmdToDataInFifo ) &&
                 ( l_CmdFifo.byIdxIn == l_CmdFifo.byIdxOut ) )
            {
               cwifi_FmtAddCmdFifo( CWIFI_CMD_CMDTODATA, "", "" ) ;
               l_bCmdToDataInFifo = TRUE ;
            }
         }
         else
         {
            l_bCmdToDataInFifo = FALSE ;
         }
         cwifi_ExecSendCmd() ;
      }
   }

   if ( l_bMaintMode && tim_IsEndSecTmp( &l_dwTmpMaintMode, CWIFI_MAINT_TIMEOUT ) )
   {
      wifi_DoSetMaintMode( FALSE ) ;
   }
}

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Finite state machine for connecting processing                             */
/*----------------------------------------------------------------------------*/

static void cwifi_ConnectFSM( void )
{
   RESULT rRet ;
   char * pszWifiSSID ;
   char * pszWifiPassword ;
   char szSecurity[2] ;

   switch ( l_eWifiState )
   {
      case CWIFI_STATE_OFF :
         if ( l_bPowerOn && l_bConsoleRdy && l_bHrdStarted )
         {
            l_eWifiState = CWIFI_STATE_IDLE ;
         }
         break ;

      case CWIFI_STATE_IDLE :
         if ( l_bConfigDone )
         {
            l_eWifiState = CWIFI_STATE_CONNECTING ;
         }
         else
         {
            rRet = OK ;

               /* This command is mandatory to reset the Wifi    */
               /* module command reader, which could be unstable */
               /* after a reset during handshake association.   */

            rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;

            if ( ! l_bMaintMode )
            {
               szSecurity[0] = '0' + GETMIN( g_sDataEeprom->sWifiConInfo.dwWifiSecurity, 2 ) ;
               szSecurity[1] = '\0' ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_priv_mode", szSecurity ) ;

               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_mode", "1" ) ;

               pszWifiPassword = g_sDataEeprom->sWifiConInfo.szWifiPassword ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_wpa_psk_text", pszWifiPassword ) ;
               pszWifiSSID = g_sDataEeprom->sWifiConInfo.szWifiSSID ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SETSSID, pszWifiSSID, "" ) ;
            }
            else
            {
               // set ip
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SETSSID, CWIFI_MAINT_SSID, "" ) ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_wep_keys[0]", CWIFI_MAINT_PWD ) ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_wep_key_lens", "0D" ) ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_auth_type", "0" ) ;

               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_priv_mode", "1" ) ;
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_mode", "3" ) ;
            }

            rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SAVE, "", "" ) ;
            rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_CFUN, "0", "" ) ;


            if ( rRet == OK )
            {
               l_bConfigDone = TRUE ;
               l_eWifiState = CWIFI_STATE_CONNECTING ;
            }
         }
         break ;

      case CWIFI_STATE_CONNECTING :
         if ( l_bWifiUp )
         {
            rRet = cwifi_FmtAddCmdFifo( CWIFI_CMD_SOCKD, "15555", "" ) ;
            if ( rRet == OK )
            {
               l_eWifiState = CWIFI_STATE_CONNECTED ;
               if ( l_bMaintMode )
               {
                  cwifi_FmtAddCmdFifo( CWIFI_CMD_SCAN, "/scan.txt", "" ) ;
               }
               tim_StartSecTmp( &l_dwTmpScan ) ;
            }
         }
         break ;

      case CWIFI_STATE_CONNECTED :
         if ( tim_IsEndSecTmp( &l_dwTmpScan, CWIFI_ACT_PERIOD ) )
         {                             /* vicinity Wifi scan demand */
            cwifi_FmtAddCmdFifo( CWIFI_CMD_SCAN, "/scan.txt", "" ) ;
                                       /* read date/time demand */
            cwifi_FmtAddCmdFifo( CWIFI_CMD_HTTPGET, "192.168.1.16", "/" ) ;
            tim_StartSecTmp( &l_dwTmpScan ) ;
         }
         break ;
   }
}


/*----------------------------------------------------------------------------*/
/* Maintenance mode setting                                                   */
/*----------------------------------------------------------------------------*/

static void wifi_DoSetMaintMode( BOOL i_bMaintmode )
{
   l_bMaintMode = i_bMaintmode ;

   l_bConfigDone = FALSE ;
   l_CmdFifo.byIdxIn = 0 ;
   l_CmdFifo.byIdxOut = 0 ;

   if ( i_bMaintmode )
   {
      tim_StartSecTmp( &l_dwTmpMaintMode ) ;
   }

   cwifi_FmtAddCmdFifo( CWIFI_CMD_CFUN, "0", "" ) ;
}


/*----------------------------------------------------------------------------*/
/* Format and add command to FIFO                                             */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_FmtAddCmdFifo( e_CmdId i_eCmdId, char C* i_szArg1, char C* i_szArg2 )
{
   char C* pszStrCmdFormat ;
   char szStrCmd[128] ;
   RESULT rRet ;

   pszStrCmdFormat = k_aCmdDesc[ (BYTE)(i_eCmdId) - 1 ].szCmdFmt ;

   sprintf( szStrCmd, pszStrCmdFormat, i_szArg1, i_szArg2 ) ;

   rRet = cwifi_AddCmdFifo( i_eCmdId, szStrCmd ) ;

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* Add command to FIFO                                                        */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_AddCmdFifo( e_CmdId i_eCmdId, char C* i_szStrCmd )
{
   BYTE byCurIdxIn ;
   BYTE byNextIdxIn ;
   s_CmdItem * pCmdItem ;
   RESULT rRet ;

   rRet = OK ;

   byCurIdxIn = l_CmdFifo.byIdxIn ;

   byNextIdxIn = NEXTIDX( byCurIdxIn, l_CmdFifo.aCmdItems ) ;

   if ( byNextIdxIn == l_CmdFifo.byIdxOut )
   {
      rRet = ERR ;
   }
   else
   {
      pCmdItem = &l_CmdFifo.aCmdItems[byCurIdxIn] ;
      pCmdItem->eCmdId = i_eCmdId ;
      strncpy( pCmdItem->szStrCmd, i_szStrCmd, sizeof(pCmdItem->szStrCmd) ) ;
      l_CmdFifo.byIdxIn = byNextIdxIn ;
   }

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* Sent incoming item from command FIFO                                       */
/*----------------------------------------------------------------------------*/

static void cwifi_ExecSendCmd( void )
{
   BYTE byIdxOut ;
   s_CmdItem * pCmdItem ;
   BOOL bUartAccept ;
   e_CmdId eCmdId ;
   BOOL bIsResult ;

   byIdxOut = l_CmdFifo.byIdxOut ;

   if ( ( l_eWifiState != CWIFI_STATE_OFF ) &&
        ( l_CmdCurStatus.eStatus == CWIFI_CMDST_NONE ) &&
        ( byIdxOut != l_CmdFifo.byIdxIn ) )
   {
      pCmdItem = &l_CmdFifo.aCmdItems[byIdxOut] ;
      eCmdId = pCmdItem->eCmdId ;

      ERR_FATAL_IF( ! uwifi_IsSendDone() ) ;

      bUartAccept = uwifi_Send( pCmdItem->szStrCmd, strlen(pCmdItem->szStrCmd) ) ;
      ERR_FATAL_IF( ! bUartAccept ) ;

      bIsResult = k_aCmdDesc[ (BYTE)(eCmdId) - 1 ].bIsResult ;

      if ( bIsResult )
      {
         l_CmdCurStatus.eStatus = CWIFI_CMDST_PROCESSING ;
         tim_StartMsTmp( &l_CmdCurStatus.dwTmpCmdTimeout ) ;
      }
      else
      {
         l_CmdCurStatus.eStatus = CWIFI_CMDST_END_OK ;
      }
      l_CmdCurStatus.eCmdId = eCmdId ;
      l_CmdCurStatus.wStrContentIdx = 0 ;

      byIdxOut = NEXTIDX( byIdxOut, l_CmdFifo.aCmdItems )  ;
      l_CmdFifo.byIdxOut = byIdxOut ;
   }
}


/*----------------------------------------------------------------------------*/
/* Add external data to data (socket) buffer                                  */
/*----------------------------------------------------------------------------*/

static void cwifi_AddDataBuffer( char C* i_szStrData )
{
   WORD wNbChar ;
   WORD wFreeSpace ;
   WORD wNbEffectiveCpy ;
   CHAR * psDataBuf ;
   CHAR C* psDataBufEnd  ;

   wNbChar = l_DataBuf.wNbChar ;

   if ( l_DataBuf.wNbCharInTx == 0 )     /* if no DMA transfer is ongoing */
   {
      wFreeSpace = sizeof(l_DataBuf.sDataBuf) - wNbChar ;
   }
   else
   {                                      /* take the space left from DMA transfer */
      wFreeSpace = ( l_DataBuf.wNbCharInTx - uwifi_GetRemainingSend() ) - wNbChar ;
                                          /* defensive prog : limit to buffer size */
      if ( wFreeSpace > sizeof(l_DataBuf.sDataBuf) )
      {
         wFreeSpace = sizeof(l_DataBuf.sDataBuf) ;
      }
   }
                                       /* if space remaining + defensive prog */
   if ( ( wFreeSpace != 0 ) && ( wNbChar < sizeof(l_DataBuf.sDataBuf) ) )
   {                                   /* pointer to first free character */
      psDataBuf = &l_DataBuf.sDataBuf[wNbChar] ;
                                       /* copy into buffer */
      psDataBufEnd = stpncpy( psDataBuf, i_szStrData, wFreeSpace ) ;
      wNbEffectiveCpy = psDataBufEnd - psDataBuf ;

      if ( ( wNbChar + wNbEffectiveCpy ) < sizeof(l_DataBuf.sDataBuf) )
      {
         wNbChar += wNbEffectiveCpy  ; /* update number of character in buffer */
      }
      else                             /* defensive prog : limit to buffer size */
      {
         wNbChar = sizeof( l_DataBuf.sDataBuf) ;
      }
      l_DataBuf.wNbChar = wNbChar ;
   }
}


/*----------------------------------------------------------------------------*/
/* Sent data (socket) buffer to Wifi module                                   */
/*----------------------------------------------------------------------------*/

static void cwifi_ExecSendData( void )
{
   BOOL bUartAccept ;
                                       /* if flush asked and no DMA transfer onging */
   if ( ( l_DataBuf.bAskFlush ) &&  ( l_DataBuf.wNbCharInTx == 0 ) &&
        uwifi_IsSendDone() )
   {
      if ( l_DataBuf.wNbChar != 0 )    /* if data to send */
      {
         bUartAccept = uwifi_Send( l_DataBuf.sDataBuf, l_DataBuf.wNbChar ) ;
      }
      else
      {
         bUartAccept = TRUE ;
      }

      if ( bUartAccept )
      {
         l_DataBuf.bAskFlush = FALSE ; /* no more ask for flushing */
                                       /* indicate DMA transfer, and set number characters */
         l_DataBuf.wNbCharInTx = l_DataBuf.wNbChar ;
         l_DataBuf.wNbChar = 0 ;       /* buffer is set as free */
                                       /* restart tempo, action on data transfer*/
         tim_StartMsTmp( &l_dwTmpDataMode ) ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/* Processing global read from Wifi module                                    */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRec( void )
{
   BYTE abyReadData[512] ;
   WORD wNbReadVal ;
   BYTE * pbyProcessData ;
   BOOL bPendingData ;

   bPendingData = FALSE ;
   wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), FALSE ) ;

   if ( wNbReadVal == 0 )
   {
      if ( ! l_bInhPendingData )
      {                                /* tempoary read data before final CR/LF */
         wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), TRUE ) ;
         bPendingData = TRUE ;
      }
   }
   else
   {
      l_bInhPendingData = FALSE ;
   }

   while ( wNbReadVal != 0 )
   {
      if ( wNbReadVal > 2 )
      {
         if ( strncmp( (char*)abyReadData, CWIFI_WIND_PREFIX, strlen(CWIFI_WIND_PREFIX) ) == 0 )
         {
            pbyProcessData = &abyReadData[sizeof(CWIFI_WIND_PREFIX)-1] ;
            cwifi_ProcessRecWind( (char*)pbyProcessData, bPendingData ) ;
         }
         else if ( ! bPendingData )
         {
            if ( strncmp( (char*)abyReadData, CWIFI_CGI_PREFIX, strlen(CWIFI_CGI_PREFIX) ) == 0 )
            {
               pbyProcessData = &abyReadData[sizeof(CWIFI_CGI_PREFIX)-1] ;
               cwifi_ProcessRecCgi( (char*)pbyProcessData ) ;
            }
            else if ( l_bDataMode && ( l_eWifiState == CWIFI_STATE_CONNECTED ) &&
                      l_bSocketConnected )
            {
               if ( l_fScktDataProc != NULL )
               {
                  pbyProcessData = &abyReadData[0] ;
                  (*l_fScktDataProc)((char*)pbyProcessData) ;

                  tim_StartMsTmp( &l_dwTmpDataMode ) ; /* restarting data mode tempo */
               }
            }
            else
            {
               pbyProcessData = abyReadData ;
               cwifi_ProcessRecResp( (char*)pbyProcessData ) ;
            }
         }
      }
      if ( bPendingData )
      {
         break ;
      }
      wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), FALSE ) ;
   }

   if ( ( l_CmdCurStatus.eStatus == CWIFI_CMDST_PROCESSING ) &&
        ( tim_IsEndMsTmp( &l_CmdCurStatus.dwTmpCmdTimeout, CWIFI_CMD_TIMEOUT ) ) )
   {
      l_CmdCurStatus.eStatus = CWIFI_CMDST_END_ERR ;

      if ( ( l_CmdCurStatus.eCmdId == CWIFI_CMD_EXT ) && ( l_fPostResProc != NULL ) )
      {
         (*l_fPostResProc)( "ERROR: Timeout\r\n", TRUE ) ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/* Processing wind treatments                                                 */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecWind( char * io_pszProcessData, BOOL i_bPendingData )
{
   s_WindDesc C* pWindDesc ;
   BYTE byIdx ;
   RESULT rRet ;
   char * pszContent ;

   pWindDesc = &k_aWindDesc[0] ;
   for ( byIdx = 0 ; byIdx < ARRAY_SIZE( k_aWindDesc ) ; byIdx ++ )
   {
      if ( strncmp( io_pszProcessData, pWindDesc->szWindNum, strlen(pWindDesc->szWindNum) ) == 0 )
      {
         rRet = OK ;

         if ( pWindDesc->fCallback != NULL )
         {
             rRet = pWindDesc->fCallback( io_pszProcessData, i_bPendingData ) ;
         }
         if ( ( rRet == OK ) && ( ! i_bPendingData ) )
         {
            if ( pWindDesc->pbVar != NULL )
            {
               *( pWindDesc->pbVar ) = pWindDesc->bValue ;
            }
            if ( pWindDesc->pszStrContent != NULL )
            {
               pszContent = (char*)cwifi_RSplit( io_pszProcessData, ":" ) ;
               pszContent[ strlen(pszContent) - 2 ] = '\0' ;
               strncpy( pWindDesc->pszStrContent, pszContent, pWindDesc->wContentSize ) ;
            }
         }
         break ;
      }
      pWindDesc++ ;
   }
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_POWER_ON callback                                          */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackPowerOn( char C* i_pszProcessData, BOOL i_bPendingData )
{
   RESULT rRet ;

   if ( ! i_bPendingData )
   {
      if ( strstr( i_pszProcessData, "SPWF01S" ) != NULL )
      {
         rRet = OK ;
      }
      else
      {
         rRet = ERR ;
      }
   }
   else
   {
      rRet = OK ;
   }

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_RESET callback                                             */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackReset( char C* i_pszProcessData, BOOL i_bPendingData )
{
   cwifi_ResetVar() ;

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_CMDMODE callback                                           */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackCmdMode( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      l_DataBuf.bAskFlush = FALSE ;
      l_DataBuf.wNbChar = 0 ;
      l_DataBuf.wNbCharInTx = 0 ;
      l_dwTmpDataMode = 0 ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_DATAMODE callback                                          */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackDataMode( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      tim_StartMsTmp( &l_dwTmpDataMode ) ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_SOCKETDATA callback                                        */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackSocketData( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      cwifi_FmtAddCmdFifo( CWIFI_CMD_CMDTODATA, "", "" ) ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* Wind CWIFI_WIND_INPUT callback                                             */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackInput( char C* i_pszProcessData, BOOL i_bPendingData )
{
   BYTE byNbComma ;
   CHAR C* pszChar ;
   DWORD dwValParam1 ;
   DWORD dwValParam2 ;
   char szOutput [512] ;
   WORD wSize ;
   DWORD dwTmpSend ;


   if ( ( i_bPendingData ) && ( l_fHtmlSsi != NULL ) )
   {
      pszChar = i_pszProcessData ;
      byNbComma = 0 ;
      dwValParam1 = 0 ;
      dwValParam2 = 0 ;

      while ( *pszChar != '\0' )
      {
         if ( *pszChar != ':'  )
         {
            if ( byNbComma == 2 )
            {
               if ( ( *pszChar >= '0' ) && ( *pszChar <= '9' ) )
               {
                  dwValParam1 *= 10 ;
                  dwValParam1 += ( *pszChar - '0' ) ;
               }
            }

            if ( byNbComma == 3 )
            {
               if ( ( *pszChar >= '0' ) && ( *pszChar <= '9' ) )
               {
                  dwValParam2 *= 10 ;
                  dwValParam2 += ( *pszChar - '0' ) ;
               }
            }
         }
         else
         {
            byNbComma++ ;
         }
         pszChar++ ;
      }

      if ( byNbComma == 4 )
      {
         (*l_fHtmlSsi)(dwValParam1, dwValParam2, szOutput, ( sizeof(szOutput) - 1 ) ) ;

         wSize = strlen( szOutput ) ;
         szOutput[wSize] = '\r' ;
         wSize++ ;
         szOutput[wSize] = '\n' ;
         wSize++ ;

         tim_StartMsTmp( &dwTmpSend ) ;
         while( ! uwifi_IsSendDone() )
         {
            if ( tim_IsEndMsTmp( &dwTmpSend, CWIFI_INPUT_SEND_TIMEOUT ) )
            {
               ERR_FATAL() ;
            }
         }

         uwifi_Send( szOutput, wSize ) ;

         tim_StartMsTmp( &dwTmpSend ) ;
         while( ! uwifi_IsSendDone() )
         {
            if ( tim_IsEndMsTmp( &dwTmpSend, CWIFI_INPUT_SEND_TIMEOUT ) )
            {
               ERR_FATAL() ;
            }
         }
         l_bInhPendingData = TRUE ;
      }
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* GCI reception processing                                                   */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecCgi( char C* i_pszProcessData )
{
   BYTE byNbComma ;
   CHAR C* pszChar ;
   DWORD dwValParam1 ;
   DWORD dwValParam2 ;

   pszChar = i_pszProcessData ;
   byNbComma = 0 ;
   dwValParam1 = 0 ;
   dwValParam2 = 0 ;

   while ( ( *pszChar != '\0' ) && ( byNbComma < 2 ) )
   {
      if ( *pszChar != ':'  )
      {
         if ( byNbComma == 0 )
         {
            if ( ( *pszChar >= '0' ) && ( *pszChar <= '9' ) )
            {
               dwValParam1 *= 10 ;
               dwValParam1 += ( *pszChar - '0' ) ;
            }
         }

         if ( byNbComma == 1 )
         {
            if ( ( *pszChar >= '0' ) && ( *pszChar <= '9' ) )
            {
               dwValParam2 *= 10 ;
               dwValParam2 += ( *pszChar - '0' ) ;
            }
         }
      }
      else
      {
         byNbComma++ ;
      }
      pszChar++ ;
   }
   if ( ( l_fHtmlCgi != NULL ) && ( byNbComma == 2 ) )
   {
      (*l_fHtmlCgi)(dwValParam1, dwValParam2, pszChar ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Processing response (from command) treatments                              */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecResp( char * io_pszProcessData )
{
   e_CmdStatus eStatus ;
   s_CmdDesc C* pCmdDesc ;
   WORD wContentSize ;
   WORD wStrContentIdx ;
   WORD wNbRemainChar ;
   WORD wNbCharToCopy ;

   eStatus = l_CmdCurStatus.eStatus ;

   if ( eStatus == CWIFI_CMDST_PROCESSING )
   {
      pCmdDesc = &k_aCmdDesc[l_CmdCurStatus.eCmdId -1] ;

      if ( strncmp( io_pszProcessData, CWIFI_RESP_ERR, strlen(CWIFI_RESP_ERR) ) == 0 )
      {
         eStatus = CWIFI_CMDST_END_ERR ;
      }
      else if ( strncmp( io_pszProcessData, CWIFI_RESP_OK, strlen(CWIFI_RESP_OK) ) == 0 )
      {
         pCmdDesc->pszStrContent[l_CmdCurStatus.wStrContentIdx] = '\0' ;

         eStatus = CWIFI_CMDST_END_OK ;
      }
      else
      {
         if ( pCmdDesc->pszStrContent != NULL )
         {
            wContentSize = pCmdDesc->wContentSize - 1 ;
            wStrContentIdx = l_CmdCurStatus.wStrContentIdx ;

            if ( wContentSize > wStrContentIdx )
            {
               wNbRemainChar = wContentSize - wStrContentIdx ;
               if ( wNbRemainChar <= strlen(io_pszProcessData) )
               {
                  wNbCharToCopy = wNbRemainChar ;
               }
               else
               {
                  wNbCharToCopy = strlen(io_pszProcessData) ;
               }
            }
            else
            {
               wNbCharToCopy = 0 ;
            }
            memcpy( &pCmdDesc->pszStrContent[wStrContentIdx], io_pszProcessData,
                    wNbCharToCopy ) ;

            l_CmdCurStatus.wStrContentIdx = wStrContentIdx + wNbCharToCopy ;
         }
      }
      l_CmdCurStatus.eStatus = eStatus ;

      if ( pCmdDesc->fCallback != NULL )
      {
         pCmdDesc->fCallback( io_pszProcessData ) ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/* response from CWIFI_CMD_HTTPGET command callback                               */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_CmdCallBackHttpGet( char C* i_pszProcData )
{
   char C* pszProcData ;
   s_DateTime DateTime ;
   BOOL bValidDt ;

   pszProcData = i_pszProcData ;
                                       /* if response line start by "DT=" */
   if ( *pszProcData == 'D' && *( pszProcData + 1 ) == 'T' && *( pszProcData + 2 ) == '=' )
   {
      pszProcData += 3 ;
                                       /* check if the following string is a valid date/time */
      bValidDt = clk_IsValidStr( pszProcData, &DateTime ) ;
      if ( bValidDt )                  /* if date/time is valid */
      {                                /* set new date/time only if it differ more than */
                                       /* 60 seconds from current date/time */
         clk_SetDateTime( &DateTime, 60 ) ;
      }
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* response from CWIFI_CMD_EXT command callback                               */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_CmdCallBackExt( char C* i_pszProcData )
{
   BOOL bLastCall ;

   bLastCall = ( ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_OK ) ||
                 ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_ERR ) ) ;

   if ( l_fPostResProc != NULL )
   {
      (*l_fPostResProc)( i_pszProcData, bLastCall ) ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/* split string from the right with specific delimiter                        */
/*----------------------------------------------------------------------------*/

static char C* cwifi_RSplit( char C* i_pszStr, char C* i_pszDelim )
{
   char C* pszPosDelim ;
   char C* pszDesc ;

   pszPosDelim = i_pszStr ;
   pszDesc = pszPosDelim ;
   pszPosDelim = strstr( pszDesc, i_pszDelim ) ;

   while ( pszPosDelim != NULL )
   {
      pszPosDelim += strlen( i_pszDelim ) ;
      pszDesc = pszPosDelim ;
      pszPosDelim = strstr( pszPosDelim, i_pszDelim ) ;
   }

   return pszDesc ;
}

/*----------------------------------------------------------------------------*/
/* Reset variables after reset                                                */
/*----------------------------------------------------------------------------*/
static void cwifi_ResetVar( void )
{
   l_eWifiState = CWIFI_STATE_OFF ;
   l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
   l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;

   l_bPowerOn = FALSE ;
   l_bConsoleRdy = FALSE ;
   l_bHrdStarted = FALSE ;
   l_bWifiUp = FALSE ;
   l_bSocketConnected = FALSE ;
   l_bDataMode = FALSE ;

   memset( l_szWindWifiUpIp, 0, sizeof(l_szWindWifiUpIp) ) ;
   memset( l_szWindSocketConIp, 0, sizeof(l_szWindSocketConIp) ) ;

   l_dwTmpDataMode = 0 ;
   l_dwTmpScan = 0 ;
}


/*----------------------------------------------------------------------------*/
/* Low level init                                                             */
/*----------------------------------------------------------------------------*/

static void cwifi_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

                                          /* set reset pin to 1 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO, WIFI_RESET_PIN, GPIO_PIN_SET ) ;

   sGpioInit.Pin = WIFI_RESET_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_PULLUP ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   sGpioInit.Alternate = WIFI_RESET_AF ;
   HAL_GPIO_Init( WIFI_RESET_GPIO, &sGpioInit ) ;
}



/*----------------------------------------------------------------------------*/
/* Wifi module reset                                                          */
/*----------------------------------------------------------------------------*/

static void cwifi_HrdSetResetModule( BOOL i_bReset )
{
   DWORD dwTmp ;

   if ( i_bReset )
   {                                    /* set reset pin to 0 */
      HAL_GPIO_WritePin( WIFI_RESET_GPIO, WIFI_RESET_PIN, GPIO_PIN_RESET ) ;
      tim_StartMsTmp( &dwTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_RESET_DURATION ) ) ;
   }
   else
   {
      uwifi_Init() ;
      uwifi_SetErrorDetection( FALSE ) ;

      HAL_GPIO_WritePin( WIFI_RESET_GPIO, WIFI_RESET_PIN, GPIO_PIN_SET ) ;
      tim_StartMsTmp( &dwTmp ) ;          /* set power-up tempo */
      while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_PWRUP_DURATION ) ) ;

      uwifi_SetErrorDetection( TRUE ) ;
   }
}
