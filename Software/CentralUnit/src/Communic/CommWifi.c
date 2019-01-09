/******************************************************************************/
/*                                                                            */
/*                                  CommWifi.c                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   07 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "Communic/l_Communic.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CWIFI_SEND_TIMEOUT       100         /* ms */

#define CWIFI_RESET_DURATION     200         /* wifi module reset duration (ms) */
#define CWIFI_PWRUP_DURATION       1         /* duration to wait after reset release (ms) */

#define CWIFI_CMD_TIMEOUT      30000         /* ms */
#define CWIFI_DATAMODE_TIMEOUT 30000         /* ms */

#define CWIFI_MAINT_TIMEOUT      300         /* sec */

#define CWIFI_WIND_PREFIX       "+WIND:"
#define CWIFI_CGI_PREFIX        "+CGI:"

#define CWIFI_RESP_OK           "OK\r\n"
#define CWIFI_RESP_ERR          "ERROR"

#define CWIFI_MAINT_SSID         "WallyBox_Maint"
#define CWIFI_MAINT_PWD          "73757065727061737465717565"  //"superpasteque"

typedef RESULT (*f_WindCallback)( char C* i_pszProcessData, BOOL i_bPendingData ) ;

typedef RESULT (*f_CmdCallback)( char C* i_pszProcessData ) ;

typedef struct
{
   char szWindNum [4] ;
   BOOL * pbVar ;
   BOOL bValue ;
   char * pszStrContent ;
   WORD wContentSize ;
   f_WindCallback fCallback ;
} s_WindDesc ;

typedef struct
{
   char szCmdFmt [32] ;        // formatteur commande
   BOOL bIsResult ;
   char * pszStrContent ;      // adresse chaine pour r�ception r�sultat
   WORD wContentSize ;
   f_CmdCallback fCallback ;
} s_CmdDesc ;

typedef enum
{
   CWIFI_CMDST_NONE,
   CWIFI_CMDST_PROCESSING,
   CWIFI_CMDST_END_OK,
   CWIFI_CMDST_END_ERR,
} e_CmdStatus ;

typedef enum
{
   CWIFI_STATE_OFF = 0,
   CWIFI_STATE_IDLE,
   CWIFI_STATE_CONNECTING,
   CWIFI_STATE_CONNECTED,
} e_WifiState ;


/*----------------------------------------------------------------------------*/
/* Wind table definition                                                      */
/*----------------------------------------------------------------------------*/

static BOOL l_bPowerOn ;
static BOOL l_bConsoleRdy ;
static BOOL l_bHrdStarted ;
static BOOL l_bWifiUp ;
static BOOL l_bSocketConnected ;
static BOOL l_bDataMode ;


static char l_szWindWifiUpIp[32] ;
static char l_szWindSocketConIp[16] ;

#define LIST_WIND( Op, Opr, Opf ) \
   Op(  CONSOLE_RDY, ConsoleRdy,  "0:",  &l_bConsoleRdy,      TRUE )  \
   Opf( POWER_ON,    PowerOn,     "1:",  &l_bPowerOn,         TRUE )  \
   Opf( RESET,       Reset,       "2:",  NULL,                0 )     \
   Op(  HRD_STARTED, HrdStarted,  "32:", &l_bHrdStarted,      TRUE )  \
   Opr( WIFI_UP_IP,  WifiUpIp,    "24:", &l_bWifiUp,          TRUE )  \
   Opf( INPUT,       Input,       "56:", NULL,                0 )     \
   Opf( CMDMODE,     CmdMode,     "59:", &l_bDataMode,        FALSE ) \
   Opf( DATAMODE,    DataMode,    "60:", &l_bDataMode,        TRUE )  \
   Opr( SOCKETCONIP, SocketConIp, "61:", &l_bSocketConnected, TRUE )  \
   Op(  SOCKETDIS,   SocketDis,   "62:", &l_bSocketConnected, FALSE ) \
   Opf( SOCKETDATA,  SocketData,  "64:", NULL,                0 )

typedef enum //v�rifier d�callage avec 0
{
   CWIFI_WIND_NONE = 0,
   LIST_WIND( CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM )
   CWIFI_WIND_LAST
} e_WindId ;

LIST_WIND( CWIFI_W_NULL, CWIFI_W_NULL, CWIFI_W_CALLBACK )

static s_WindDesc const k_aWindDesc [] =
{
   LIST_WIND( CWIFI_W_OPER, CWIFI_W_OPER_R, CWIFI_W_OPER_F )
} ;


/*----------------------------------------------------------------------------*/
/* Commands/responses table definition                                        */
/*----------------------------------------------------------------------------*/

static char l_szRespPing [1] ;      //SBA temp
static char l_szRespGCfg [1] ;      //SBA temp

#define LIST_CMD( Op, Opr, Opf )                          \
   Op(  AT,        At,        "AT\r",              TRUE )  \
   Op(  SCFG,      SCfg,      "AT+S.SCFG=%s,%s\r", TRUE )  \
   Opr( GCFG,      GCfg,      "AT+S.GCFG=%s\r",    TRUE )  \
   Op(  SETSSID,   SetSsid,   "AT+S.SSIDTXT=%s\r", TRUE )  \
   Op(  CFUN,      CFun,      "AT+CFUN=%s\r",      FALSE )  \
   Op(  SAVE,      Save,      "AT&W\r",            TRUE )  \
   Op(  FACTRESET, FactReset, "AT&F\r",            TRUE )  \
   Opr( PING,      Ping,      "AT+S.PING=%s\r",    TRUE )  \
   Op(  SOCKD,     Sockd,     "AT+S.SOCKD=%s\r",   TRUE )  \
   Op(  CMDTODATA, CmdToData, "AT+S.\r",           FALSE ) \
   Op(  FSL,       Fsl,       "AT+S.FSL\r",        TRUE )  \
   Opf( EXT,       Ext,       "",                  TRUE ) \

typedef enum
{
   CWIFI_CMD_NONE = 0,
   LIST_CMD( CWIFI_C_ENUM, CWIFI_C_ENUM, CWIFI_C_ENUM )
   CWIFI_CMD_LAST,
} e_CmdId ;

LIST_CMD( CWIFI_C_NULL, CWIFI_C_NULL, CWIFI_C_CALLBACK )

static s_CmdDesc const k_aCmdDesc [] =
{
   LIST_CMD( CWIFI_C_OPER, CWIFI_C_OPER_R, CWIFI_C_OPER_F )
} ;

typedef struct
{
   e_CmdId eCmdId ;
   e_CmdStatus eStatus ;
   WORD wStrContentIdx ;
   DWORD dwTmpCmdTimeout ;
} s_CmdCurStatus ;

typedef struct
{
   e_CmdId eCmdId ;
   char szStrCmd [128] ;
} s_CmdItem ;

typedef struct
{
   BYTE byIdxIn ;
   BYTE byIdxOut ;
   s_CmdItem aCmdItems [10] ;
} s_CmdFifo ;

#define CWIFI_DATABUF_SIZE  512

typedef struct
{
   BOOL bAskFlush ;
   BOOL bPendingFlush ;
   DWORD dwNbChar ;
   CHAR sDataBuf [CWIFI_DATABUF_SIZE] ;
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

static RESULT cwifi_AddDataBuffer( char C* i_szStrData ) ;
static void cwifi_ExecSendData( void ) ;

static void cwifi_ProcessRec( void ) ;
static void cwifi_ProcessRecWind( char * io_pszProcessData, BOOL i_bPendingData  ) ;
static void cwifi_ProcessRecCgi( char C* i_pszProcessData ) ;
static void cwifi_ProcessRecResp( char * io_pszProcessData ) ;

static char C* cwifi_RSplit( char C* i_pszStr, char C* i_pszDelim ) ;
static void cwifi_ResetVar( void ) ;

static void cwifi_HrdInit( void ) ;
static void cwifi_HrdModuleReset( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_WifiState l_eWifiState ;
static s_CmdCurStatus l_CmdCurStatus ;
static DWORD l_dwTmpDataMode ;
static DWORD l_dwTmpMaintMode ;

static BOOL l_bMaintMode ;
static BOOL l_bConfigDone ;
static BOOL l_bCmdToDataInFifo ;

static BOOL l_bInhPendingData ;

static f_ScktGetFrame l_fScktGetFrame ;
static f_ScktGetResExt l_fScktGetResExt ;

static f_htmlSsi l_fHtmlSsi ;
static f_htmlCgi l_fHtmlCgi ;

static s_CmdFifo l_CmdFifo ;
static s_DataBuf l_DataBuf ;



/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void )
{
   cwifi_HrdInit() ;
   cwifi_HrdModuleReset() ;
   cwifi_ResetVar() ;
   l_bMaintMode = FALSE ;
   l_bConfigDone = FALSE ;
}


/*----------------------------------------------------------------------------*/

void cwifi_RegisterScktFunc( f_ScktGetFrame i_fScktGetFrame, f_ScktGetResExt i_fScktGetResExt )
{
   l_fScktGetFrame = i_fScktGetFrame ;
   l_fScktGetResExt = i_fScktGetResExt ;
}


/*----------------------------------------------------------------------------*/

void cwifi_RegisterHtmlFunc( f_htmlSsi i_fHtmlSsi, f_htmlCgi i_fHtmlCgi )
{
   l_fHtmlSsi = i_fHtmlSsi ;
   l_fHtmlCgi = i_fHtmlCgi ;
}


/*----------------------------------------------------------------------------*/

void cwifi_SetMaintMode( BOOL i_bMaintmode )
{
   if ( l_bMaintMode != i_bMaintmode )
   {
      wifi_DoSetMaintMode( i_bMaintmode ) ;
   }
}


/*----------------------------------------------------------------------------*/

BOOL cwifi_IsConnected( void )
{
   BOOL bConnect ;

   bConnect = ( l_eWifiState == CWIFI_STATE_CONNECTED ) ;

   return bConnect ;
}


/*----------------------------------------------------------------------------*/

BOOL cwifi_IsSocketConnected( void )
{
   return l_bSocketConnected ;
}


/*----------------------------------------------------------------------------*/

BOOL cwifi_IsMaintMode( void )
{
   return l_bMaintMode ;
}


/*----------------------------------------------------------------------------*/

void cwifi_AddExtCmd( char C* i_szStrCmd )
{
   cwifi_AddCmdFifo( CWIFI_CMD_EXT, i_szStrCmd ) ;
}


/*----------------------------------------------------------------------------*/
void cwifi_AddExtData( char C* i_szStrCmd )
{
   cwifi_AddDataBuffer( i_szStrCmd ) ;
}


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
      //flush buffer ;
      //if ( l_eWifiState == CWIFI_STATE_CONNECTING && ( ! l_bWifiUp ) )
      //{
      //   l_eWifiState = CWIFI_STATE_RDY ;
      //}
      l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
      l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;
   }
   else if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_OK )
   {
      l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
      l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;
   }
   else           /* NONE ou PROCESSING */
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


   if ( l_DataBuf.bPendingFlush )
   {
      if ( uWifi_IsSendDone() )
      {
         l_DataBuf.bAskFlush = FALSE ;
         l_DataBuf.bPendingFlush = FALSE ;
         l_DataBuf.dwNbChar = 0 ;
         memset( &l_DataBuf.sDataBuf, 0, sizeof(l_DataBuf.sDataBuf) ) ;
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
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ConnectFSM( void )
{
   RESULT rRet ;
   char * pszWifiSSID ;
   char * pszWifiPassword ;

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
               rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_priv_mode", "2" ) ;
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
            rRet = cwifi_FmtAddCmdFifo( CWIFI_CMD_SOCKD, "15555", "" ) ; //TODO: de temps en temps, la commande ne semble pas s'envoyer
            if ( rRet == OK )
            {
               l_eWifiState = CWIFI_STATE_CONNECTED ;
            }
         }
         break ;

      case CWIFI_STATE_CONNECTED :
         /* nothing to do */
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
/*                                                                            */
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

      ERR_FATAL_IF( ! uWifi_IsSendDone() ) ;

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

static RESULT cwifi_AddDataBuffer( char C* i_szStrData )
{
   WORD wBufSeparator ;
   CHAR C* pszChar ;
   RESULT rRet ;

   rRet = ERR ;

   if ( ! l_DataBuf.bPendingFlush )
   {
      wBufSeparator = l_DataBuf.dwNbChar % sizeof(l_DataBuf.sDataBuf) ;
      pszChar = i_szStrData ;

      while( *pszChar != 0 )
      {
         l_DataBuf.sDataBuf[wBufSeparator] = *pszChar ;
         wBufSeparator = ( ( wBufSeparator + 1 ) % sizeof(l_DataBuf.sDataBuf) ) ;
         pszChar++ ;
         l_DataBuf.dwNbChar++ ;
      }

      if ( l_DataBuf.dwNbChar > sizeof(l_DataBuf.sDataBuf) )
      {
         rRet = OK ;
      }
   }

   return rRet ;
}



/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ExecSendData( void )
{
   WORD wBufSize ;
   BOOL bUartAccept ;
   WORD wBufSeparator ;
   WORD wIdx1 ;
   WORD wIdx2 ;
   char sInterBuf[CWIFI_DATABUF_SIZE/2] ;
   char * sDataBuf ;

   if ( ( l_eWifiState != CWIFI_STATE_OFF ) && ( l_DataBuf.bAskFlush ) &&
        ( l_DataBuf.dwNbChar != 0 ) && uWifi_IsSendDone() )
   {
      sDataBuf = l_DataBuf.sDataBuf ;
      wBufSize = sizeof( l_DataBuf.sDataBuf ) ;


      if ( l_DataBuf.dwNbChar <= wBufSize )
      {
         bUartAccept = uwifi_Send( sDataBuf, l_DataBuf.dwNbChar ) ;
         ERR_FATAL_IF( ! bUartAccept ) ;
      }
      else
      {
         wBufSeparator = l_DataBuf.dwNbChar % wBufSize ;
         if ( wBufSeparator > 512 )
         {
            memcpy( sInterBuf, &sDataBuf[wBufSeparator], ( wBufSize - wBufSeparator ) ) ;
            wIdx2 = wBufSize - 1 ;
            for ( wIdx1 = wBufSeparator - 1 ; wIdx1 != 0 ; wIdx1-- )
            {
               sDataBuf[wIdx2] = sDataBuf[wIdx1] ;
               wIdx2-- ;
            }
            sDataBuf[wIdx2] = sDataBuf[wIdx1] ;
            memcpy( &sDataBuf[0], sInterBuf, ( wBufSize - wBufSeparator ) ) ;
         }
         else
         {
            memcpy( sInterBuf, &sDataBuf[0], wBufSeparator ) ;
            wIdx2 = 0 ;
            for ( wIdx1 = wBufSeparator ; wIdx1 < wBufSize ; wIdx1++ )
            {
               sDataBuf[wIdx2] = sDataBuf[wIdx1] ;
               wIdx2++ ;
            }
            memcpy( &sDataBuf[(wBufSize - wBufSeparator)], sInterBuf, wBufSeparator ) ;
         }

         bUartAccept = uwifi_Send( sDataBuf, wBufSize ) ;
         ERR_FATAL_IF( ! bUartAccept ) ;
      }

      l_DataBuf.bPendingFlush = TRUE ;
      tim_StartMsTmp( &l_dwTmpDataMode ) ; // red�marrage tempo
   }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
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
      {                                /* lecture temporaire données avant CR/LF final */
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
               if ( l_fScktGetFrame != NULL )
               {
                  pbyProcessData = &abyReadData[0] ;
                  (*l_fScktGetFrame)((char*)pbyProcessData) ;
                  tim_StartMsTmp( &l_dwTmpDataMode ) ; // redémarrage tempo
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
   {                                // force retour erreur
      l_CmdCurStatus.eStatus = CWIFI_CMDST_END_ERR ;

      if ( ( l_CmdCurStatus.eCmdId == CWIFI_CMD_EXT ) && ( l_fScktGetResExt != NULL ) )
      {
         (*l_fScktGetResExt)( "ERROR: Timeout\r\n", TRUE ) ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
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
/*                                                                            */
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
/*                                                                            */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackReset( char C* i_pszProcessData, BOOL i_bPendingData )
{
   cwifi_ResetVar() ;

   return OK ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackDataMode( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      tim_StartMsTmp( &l_dwTmpDataMode ) ;  // red�marrage tempo
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackCmdMode( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      l_DataBuf.bAskFlush = FALSE ;
      l_DataBuf.bPendingFlush = FALSE ;
      l_DataBuf.dwNbChar = 0 ;
      memset( &l_DataBuf.sDataBuf, 0, sizeof(l_DataBuf.sDataBuf) ) ;
      l_dwTmpDataMode = 0 ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
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
/*                                                                            */
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
         (*l_fHtmlSsi)(dwValParam1, dwValParam2, szOutput, ( sizeof(szOutput) - 2 ) ) ;

         wSize = strlen( szOutput ) ;
         szOutput[wSize] = '\r' ;
         wSize++ ;
         szOutput[wSize] = '\n' ;
         wSize++ ;

         tim_StartMsTmp( &dwTmpSend ) ;
         while( ! uWifi_IsSendDone() )
         {
            if ( tim_IsEndMsTmp( &dwTmpSend, CWIFI_SEND_TIMEOUT ) )
            {
               ERR_FATAL() ;
            }
         }

         uwifi_Send( szOutput, wSize ) ;

         tim_StartMsTmp( &dwTmpSend ) ;
         while( ! uWifi_IsSendDone() )
         {
            if ( tim_IsEndMsTmp( &dwTmpSend, CWIFI_SEND_TIMEOUT ) )
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
/*                                                                            */
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
/*                                                                            */
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

static RESULT cwifi_CmdCallBackExt( char C* i_pszProcData )
{
   BOOL bLastCall ;

   bLastCall = ( ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_OK ) ||
                 ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_ERR ) ) ;

   if ( l_fScktGetResExt != NULL )
   {
      (*l_fScktGetResExt)(i_pszProcData, bLastCall) ;
   }

   return OK ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
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

static void cwifi_HrdModuleReset( void )
{
   DWORD dwTmp ;
                                       /* set reset pin to 0 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO, WIFI_RESET_PIN, GPIO_PIN_RESET ) ;
   tim_StartMsTmp( &dwTmp ) ;
   while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_RESET_DURATION ) ) ;

   uwifi_Init() ;
   uwifi_SetErrorDetection( FALSE ) ;

   HAL_GPIO_WritePin( WIFI_RESET_GPIO, WIFI_RESET_PIN, GPIO_PIN_SET ) ;
   tim_StartMsTmp( &dwTmp ) ;          /* set power-up tempo */
   while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_PWRUP_DURATION ) ) ;

   uwifi_SetErrorDetection( TRUE ) ;
}


