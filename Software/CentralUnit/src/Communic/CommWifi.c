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

#define CWIFI_RESET_DURATION     200         /* wifi module reset duration (ms) */
#define CWIFI_PWRUP_DURATION       1         /* duration to wait after reset release (ms) */

#define CWIFI_CMD_TIMEOUT       2000         /* ms */

#define CWIFI_WIND_PREFIX       "+WIND:"
#define CWIFI_RESP_OK           "OK\r\n"
#define CWIFI_RESP_ERR          "ERROR"

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
   char * pszStrContent ;      // adresse chaine pour réception résultat
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
   CWIFI_STATE_RDY,
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

#define LIST_WIND( Op, Opr, Opf, Oprf ) \
   Op(  CONSOLE_RDY, ConsoleRdy,  "0:",  &l_bConsoleRdy,      TRUE )  \
   Opf( POWER_ON,    PowerOn,     "1:",  &l_bPowerOn,         TRUE )  \
   Op(  HRD_STARTED, HrdStarted,  "32:", &l_bHrdStarted,      TRUE )  \
   Opr( WIFI_UP_IP,  WifiUpIp,    "24:", &l_bWifiUp,          TRUE )  \
   Opf( INPUT,       Input,       "56:", NULL,                0 )     \
   Opr( SOCKETCONIP, SocketConIp, "61:", &l_bSocketConnected, TRUE )  \
   Op(  SOCKETDIS,   SocketDis,   "62:", &l_bSocketConnected, FALSE ) \
   Op(  DATAMODE,    DataMode,    "60:", &l_bDataMode,        TRUE )  \
   Op(  CMDMODE,     CmdMode,     "59:", &l_bDataMode,        FALSE )


typedef enum //vérifier décallage avec 0
{
   CWIFI_WIND_NONE = 0,
   LIST_WIND( CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM )
   CWIFI_WIND_LAST
} e_WindId ;

LIST_WIND( CWIFI_W_NULL, CWIFI_W_NULL, CWIFI_W_CALLBACK, CWIFI_W_CALLBACK )

static s_WindDesc const k_aWindDesc [] =
{
   LIST_WIND( CWIFI_W_OPER, CWIFI_W_OPER_R, CWIFI_W_OPER_F, CWIFI_W_OPER_RF )
} ;


/*----------------------------------------------------------------------------*/
/* Commands/responses table definition                                        */
/*----------------------------------------------------------------------------*/

static char l_szRespPing [40] ;
static char l_szRespFsl [256] ;
static char l_szRespGCfg [40] ;

#define LIST_CMD( Op, Opr, Oprf )                   \
   Op(  AT,        At,        "AT\r" )              \
   Op(  SCFG,      SCfg,      "AT+S.SCFG=%s,%s\r" ) \
   Opr( GCFG,      GCfg,      "AT+S.GCFG=%s\r" )    \
   Op(  SETSSID,   SetSsid,   "AT+S.SSIDTXT=%s\r" ) \
   Op(  CFUN,      CFun,      "AT+CFUN=%s\r" )      \
   Op(  SAVE,      Save,      "AT&W\r" )            \
   Op(  FACTRESET, FactReset, "AT&F\r" )            \
   Opr( PING,      Ping,      "AT+S.PING=%s\r" )    \
   Op(  SOCKD,     Sockd,     "AT+S.SOCKD=%s\r" )   \
   Op(  TODATA,    ToData,    "AT+S.\r" )           \
   Opr( FSL,       Fsl,       "AT+S.FSL\r")         \
   Op(  EXT,       Ext,       "")

typedef enum
{
   CWIFI_CMD_NONE = 0,
   LIST_CMD( CWIFI_C_ENUM, CWIFI_C_ENUM, CWIFI_C_ENUM )
   CWIFI_CMD_LAST,
} e_CmdId ;

LIST_CMD( CWIFI_C_NULL, CWIFI_C_NULL, CWIFI_C_CALLBACK )

static s_CmdDesc const k_aCmdDesc [] =
{
   LIST_CMD( CWIFI_C_OPER, CWIFI_C_OPER_R, CWIFI_C_OPER_RF )
} ;

typedef struct
{
   e_CmdId eCmdId ;
   e_CmdStatus eStatus ;
   WORD wStrContentIdx ;
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
   s_CmdItem CmdItems [8] ;
} s_CmdFifo ;



/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_Connect( void ) ;
static RESULT cwifi_FmtAddCmdFifo( e_CmdId i_eCmdId, char C* i_szArg1,
                                                     char C* i_szArg2 ) ;
static RESULT cwifi_AddCmdFifo( e_CmdId i_eCmdId, char C* i_szStrCmd ) ;
static void cwifi_ExecSendCmd( void ) ;
static void cwifi_ProcessRec( void ) ;
static void cwifi_ProcessRecWind( char * i_pszProcessData, BOOL i_bPendingData  ) ;
static char C* cwifi_RSplit( char C* i_pszStr, char C* i_pszDelim ) ;
static void cwifi_ProcessRecResp( char * io_pszProcessData ) ;
static void cwifi_HrdInit( void ) ;
static void cwifi_SetReset( BOOL i_bReset ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_WifiState l_eWifiState ;
static s_CmdCurStatus l_CmdCurStatus ;
static BYTE l_byCmdConnectIdx ;
static DWORD l_dwTmpCmdTimeout ;
static BOOL l_bSocketOpen ;
static f_ScktFrame l_fScktFrame ;

static s_CmdFifo l_CmdFifo ;



/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void )
{
   cwifi_HrdInit() ;

   cwifi_SetReset( TRUE ) ;
   uwifi_Init() ;
   uwifi_SetErrorDetection( FALSE ) ;

   l_eWifiState = CWIFI_STATE_OFF ;
   l_CmdCurStatus.eCmdId = CWIFI_CMD_NONE ;
   l_CmdCurStatus.eStatus = CWIFI_CMDST_NONE ;
   l_byCmdConnectIdx = 0 ;

   l_bPowerOn = FALSE ;
   l_bConsoleRdy = FALSE ;
   l_bHrdStarted = FALSE ;
   l_bSocketOpen = FALSE ;

   cwifi_SetReset( FALSE ) ;

   uwifi_SetErrorDetection( TRUE ) ;
}


/*----------------------------------------------------------------------------*/

void cwifi_RegisterScktFrameFunc( f_ScktFrame i_fScktFrame )
{
   l_fScktFrame = i_fScktFrame ;
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

RESULT cwifi_AddExtCmd( char C* i_szStrCmd )
{
   RESULT rRet ;

   rRet = cwifi_AddCmdFifo( CWIFI_CMD_EXT, i_szStrCmd ) ;

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 10 msec )                                           */
/*----------------------------------------------------------------------------*/

void cwifi_TaskCyc( void )
{
   RESULT rRet ;
   static DWORD dwTmpPing ;

   cwifi_ProcessRec() ;

   if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_ERR )
   {
      //flush buffer ;
      if ( l_eWifiState == CWIFI_STATE_CONNECTING && ( ! l_bWifiUp ) )
      {
         l_eWifiState = CWIFI_STATE_RDY ;
      }
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


   if ( l_eWifiState == CWIFI_STATE_OFF )
   {
      if ( l_bPowerOn && l_bConsoleRdy && l_bHrdStarted )
      {
         rRet = cwifi_Connect() ;
         if ( rRet == OK )
         {
            l_eWifiState = CWIFI_STATE_CONNECTING ;
            l_byCmdConnectIdx = 0 ;
         }
      }
   }

   if ( l_eWifiState == CWIFI_STATE_CONNECTING )
   {
      if ( l_bWifiUp )
      {
         rRet = cwifi_FmtAddCmdFifo( CWIFI_CMD_SOCKD, "15555", "" ) ;
         if ( rRet == OK )
         {
            l_eWifiState = CWIFI_STATE_CONNECTED ;
            tim_StartSecTmp( &dwTmpPing ) ;
         }
      }
   }

   if ( l_eWifiState == CWIFI_STATE_CONNECTED )
   {
      if ( tim_IsEndSecTmp( &dwTmpPing, 10 ) )
      {
         tim_StartSecTmp( &dwTmpPing ) ;

         if ( ( l_CmdCurStatus.eStatus == CWIFI_CMDST_NONE ) && ( ! l_bDataMode ) )
         {
            if ( ! l_bDataMode )
            {
               cwifi_FmtAddCmdFifo( CWIFI_CMD_PING, "192.168.1.254", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_AT, "", "" ) ;
               cwifi_FmtAddCmdFifo( CWIFI_CMD_PING, "192.168.1.100", "" ) ;
               //cwifi_FmtAddCmdFifo( CWIFI_CMD_FSL, "", "" ) ;
            }
         }
      }
   }

   cwifi_ExecSendCmd() ;
}

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_Connect( void )
{
   RESULT rRet ;
   char * pszWifiSSID ;
   char * pszWifiPassword ;

   rRet = OK ;

   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_priv_mode", "2" ) ;
   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_mode", "1" ) ;

   pszWifiPassword = g_sDataEeprom->sWifiConInfo.szWifiPassword ;
   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_wpa_psk_text", pszWifiPassword ) ;
   pszWifiSSID = g_sDataEeprom->sWifiConInfo.szWifiSSID ;
   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SETSSID, pszWifiSSID, "" ) ;

   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SAVE, "", "" ) ;
   rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_CFUN, "0", "" ) ;

   return rRet ;
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

   byCurIdxIn = l_CmdFifo.byIdxIn ;

   byNextIdxIn = ( ( byCurIdxIn + 1 ) % ARRAY_SIZE(l_CmdFifo.CmdItems) )  ;

   if ( byNextIdxIn != l_CmdFifo.byIdxOut )
   {
      pCmdItem = &l_CmdFifo.CmdItems[byCurIdxIn] ;
      pCmdItem->eCmdId = i_eCmdId ;
      strncpy( pCmdItem->szStrCmd, i_szStrCmd, sizeof(pCmdItem->szStrCmd) ) ;
      l_CmdFifo.byIdxIn = byNextIdxIn ;
      rRet = OK ;
   }
   else
   {
      rRet = ERR ;
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
   BOOL bSendDone ;
   BOOL bCmdAccept ;

   byIdxOut = l_CmdFifo.byIdxOut ;

   if ( ( l_eWifiState != CWIFI_STATE_OFF ) &&
        ( l_CmdCurStatus.eStatus == CWIFI_CMDST_NONE ) &&
        ( byIdxOut != l_CmdFifo.byIdxIn ) )
   {
      bSendDone = uWifi_IsSendDone() ;
      ERR_FATAL_IF( ! bSendDone ) ;

      pCmdItem = &l_CmdFifo.CmdItems[byIdxOut] ;

      bCmdAccept = uwifi_Send( pCmdItem->szStrCmd, strlen(pCmdItem->szStrCmd) ) ;
      ERR_FATAL_IF( ! bCmdAccept ) ;

      l_CmdCurStatus.eCmdId = pCmdItem->eCmdId ;
      if ( pCmdItem->eCmdId == CWIFI_CMD_CFUN )
      {
         l_CmdCurStatus.eStatus = CWIFI_CMDST_END_OK ;
      }
      else
      {
         l_CmdCurStatus.eStatus = CWIFI_CMDST_PROCESSING ;
      }
      l_CmdCurStatus.wStrContentIdx = 0 ;

      byIdxOut = ( ( byIdxOut + 1 ) % ARRAY_SIZE(l_CmdFifo.CmdItems) )  ;
      l_CmdFifo.byIdxOut = byIdxOut ;

      tim_StartMsTmp( &l_dwTmpCmdTimeout ) ;
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
   BYTE bPendingData ;

   bPendingData = FALSE ;

   wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), FALSE ) ;

   if ( wNbReadVal == 0 )
   {                                   /* lecture temporaire données avant CR/LF final */
      wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), TRUE ) ;
      bPendingData = TRUE ;
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
            if ( l_bDataMode && ( l_eWifiState == CWIFI_STATE_CONNECTED ) &&
                  l_bSocketConnected )
            {
               if ( l_fScktFrame != NULL )
               {
                  (*l_fScktFrame)((char*)pbyProcessData) ;
               }
            }
            else
            {
               pbyProcessData = abyReadData ;
               cwifi_ProcessRecResp( (char*)pbyProcessData ) ;
            }
         }
      }
      wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), FALSE ) ;
   }

   if ( ( l_CmdCurStatus.eStatus == CWIFI_CMDST_PROCESSING ) &&
        ( tim_IsEndMsTmp( &l_dwTmpCmdTimeout, CWIFI_CMD_TIMEOUT ) ) )
   {                                // force retour erreur
      l_CmdCurStatus.eStatus = CWIFI_CMDST_END_ERR ;
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

   REFPARM(i_bPendingData)

   if ( strstr( i_pszProcessData, "SPWF01S" ) != NULL )
   {
      rRet = OK ;
   }
   else
   {
      rRet = ERR ;
   }
   return rRet ;
}



/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static RESULT cwifi_WindCallBackInput( char C* i_pszProcessData, BOOL i_bPendingData )
{
   BOOL bSendDone ;

   bSendDone = uWifi_IsSendDone() ;
   if ( bSendDone && i_bPendingData )
   {
      uwifi_Send( "1234\r\n", strlen("1234\r\n") ) ; // SBA appel pour fourniture infos
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
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecResp( char * io_pszProcessData )
{
   e_CmdStatus eStatus ;
   s_CmdDesc C* pCmdDesc ;
   RESULT rRet ;
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

         rRet = OK ;

         if ( pCmdDesc->fCallback != NULL )
         {
            rRet = pCmdDesc->fCallback( pCmdDesc->pszStrContent ) ;
         }
         if ( rRet == OK )
         {
            eStatus = CWIFI_CMDST_END_OK ;
         }
         else
         {
            eStatus = CWIFI_CMDST_END_ERR ;
         }
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
   }
}


/*----------------------------------------------------------------------------*/
/* Low level init                                                             */
/*----------------------------------------------------------------------------*/

static void cwifi_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

   WIFI_RESET_GPIO_CLK_ENABLE() ;          /* enable the GPIO_LED Clock */

                                          /* set reset pin to 1 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_SET ) ;

   sGpioInit.Pin = WIFI_RESET_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_PULLUP ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   sGpioInit.Alternate = WIFI_RESET_AF ;
   HAL_GPIO_Init( WIFI_RESET_GPIO_PORT, &sGpioInit ) ;
}


/*----------------------------------------------------------------------------*/
/* Wifi module reset                                                          */
/*----------------------------------------------------------------------------*/

static void cwifi_SetReset( BOOL i_bReset )
{
   DWORD dwTmp ;

   if ( i_bReset )
   {                                      /* set reset pin to 0 */
      HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_RESET ) ;

      tim_StartMsTmp( &dwTmp ) ;
      while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_RESET_DURATION ) ) ;
   }
   else
   {                                      /* set reset pin to 1 */
      HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_SET ) ;

      tim_StartMsTmp( &dwTmp ) ;          /* set power-up tempo */
      while ( ! tim_IsEndMsTmp( &dwTmp, CWIFI_PWRUP_DURATION ) ) ;
   }
}
