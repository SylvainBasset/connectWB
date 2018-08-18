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

#define CWIFI_CMD_TIMEOUT      10000         /* ms */
#define CWIFI_DATAMODE_TIMEOUT   800         /* ms */

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
   BOOL bIsResult ;
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
   Op(  HRD_STARTED, HrdStarted,  "32:", &l_bHrdStarted,      TRUE )  \
   Opr( WIFI_UP_IP,  WifiUpIp,    "24:", &l_bWifiUp,          TRUE )  \
   Opf( INPUT,       Input,       "56:", NULL,                0 )     \
   Opf( CMDMODE,     CmdMode,     "59:", &l_bDataMode,        FALSE ) \
   Opf( DATAMODE,    DataMode,    "60:", &l_bDataMode,        TRUE )  \
   Opr( SOCKETCONIP, SocketConIp, "61:", &l_bSocketConnected, TRUE )  \
   Op(  SOCKETDIS,   SocketDis,   "62:", &l_bSocketConnected, FALSE ) \
   Opf( SOCKETDATA,  SocketData,  "64:", NULL,                0 )

typedef enum //vérifier décallage avec 0
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

static char l_szRespPing [40] ;      //SBA temp
static char l_szRespFsl [256] ;      //SBA temp
static char l_szRespGCfg [40] ;      //SBA temp

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
   Opr( FSL,       Fsl,       "AT+S.FSL\r",        TRUE )  \
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
   s_CmdItem aCmdItems [8] ;
} s_CmdFifo ;


typedef struct
{
   char szStrData [128] ;
} s_DataItem ;

typedef struct
{
   BOOL bSuspend ;
   BYTE byIdxIn ;
   BYTE byIdxOut ;
   s_DataItem aDataItems [4] ;
} s_DataFifo ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cwifi_ConnectFSM( void ) ;

static RESULT cwifi_FmtAddCmdFifo( e_CmdId i_eCmdId, char C* i_szArg1,
                                                     char C* i_szArg2 ) ;
static RESULT cwifi_AddCmdFifo( e_CmdId i_eCmdId, char C* i_szStrCmd ) ;
static void cwifi_ExecSendCmd( void ) ;

static RESULT cwifi_AddDataFifo( char C* i_szStrData ) ;
static void cwifi_ExecSendData( void ) ;

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
static DWORD l_dwTmpDataMode ;
static DWORD l_dwTmpIsAlive ;

static f_ScktGetFrame l_fScktGetFrame ;
static f_ScktGetResExt l_fScktGetResExt ;

static s_CmdFifo l_CmdFifo ;
static s_DataFifo l_DataFifo ;



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

   l_bPowerOn = FALSE ;
   l_bConsoleRdy = FALSE ;
   l_bHrdStarted = FALSE ;

   cwifi_SetReset( FALSE ) ;

   uwifi_SetErrorDetection( TRUE ) ;
}


/*----------------------------------------------------------------------------*/

void cwifi_RegisterScktFunc( f_ScktGetFrame fScktGetFrame,
                             f_ScktGetResExt fScktGetResExt )
{
   l_fScktGetFrame = fScktGetFrame ;
   l_fScktGetResExt = fScktGetResExt ;
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

void cwifi_AddExtCmd( char C* i_szStrCmd )
{
   cwifi_AddCmdFifo( CWIFI_CMD_EXT, i_szStrCmd ) ;
}


/*----------------------------------------------------------------------------*/
void cwifi_AddExtData( char C* i_szStrCmd )
{
   cwifi_AddDataFifo( i_szStrCmd ) ;
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
         cwifi_AddDataFifo( "at+s." ) ;
      }
   }

   if ( l_bDataMode )
   {
      cwifi_ExecSendData() ;
   }
   else
   {
      if ( ( l_CmdFifo.byIdxIn == l_CmdFifo.byIdxOut ) &&
           ( l_DataFifo.byIdxIn != l_DataFifo.byIdxOut ) )
      {
         cwifi_FmtAddCmdFifo( CWIFI_CMD_CMDTODATA, "", "" ) ;
      }
      cwifi_ExecSendCmd() ;
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
         rRet = OK ;
         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_priv_mode", "2" ) ;
         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_mode", "1" ) ;

         pszWifiPassword = g_sDataEeprom->sWifiConInfo.szWifiPassword ;
         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SCFG, "wifi_wpa_psk_text", pszWifiPassword ) ;
         pszWifiSSID = g_sDataEeprom->sWifiConInfo.szWifiSSID ;
         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SETSSID, pszWifiSSID, "" ) ;

         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_SAVE, "", "" ) ;
         rRet |= cwifi_FmtAddCmdFifo( CWIFI_CMD_CFUN, "0", "" ) ;

         if ( rRet == OK )
         {
            l_eWifiState = CWIFI_STATE_CONNECTING ;
         }
         break ;

      case CWIFI_STATE_CONNECTING :
         if ( l_bWifiUp )
         {
            rRet = cwifi_FmtAddCmdFifo( CWIFI_CMD_SOCKD, "15555", "" ) ; //TODO: de temps en temps, la commande ne semble pas s'envoyer
            if ( rRet == OK )
            {
               l_eWifiState = CWIFI_STATE_CONNECTED ;
               tim_StartSecTmp( &l_dwTmpIsAlive ) ;
            }
         }
         break ;

      case CWIFI_STATE_CONNECTED :
         if ( l_bDataMode )
         {
            if ( tim_IsEndSecTmp( &l_dwTmpIsAlive, 10 ) )
            {
               tim_StartSecTmp( &l_dwTmpIsAlive ) ;

               if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_NONE  )
               {
                  //cwifi_FmtAddCmdFifo( CWIFI_CMD_PING, "192.168.1.254", "" ) ;
               }
            }
         }
         break ;
   }
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
   {                                   // last element is lost
      l_CmdFifo.byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aCmdItems ) ;
      rRet = ERR ;
   }

   pCmdItem = &l_CmdFifo.aCmdItems[byCurIdxIn] ;
   pCmdItem->eCmdId = i_eCmdId ;
   strncpy( pCmdItem->szStrCmd, i_szStrCmd, sizeof(pCmdItem->szStrCmd) ) ;
   l_CmdFifo.byIdxIn = byNextIdxIn ;

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

static RESULT cwifi_AddDataFifo( char C* i_szStrData )
{
   BYTE byCurIdxIn ;
   BYTE byNextIdxIn ;
   s_DataItem * pDataItem ;
   RESULT rRet ;

   rRet = OK ;

   byCurIdxIn = l_DataFifo.byIdxIn ;

   byNextIdxIn = NEXTIDX( byCurIdxIn, l_DataFifo.aDataItems ) ;

   if ( byNextIdxIn == l_DataFifo.byIdxOut )
   {                                   // last element is lost
      l_DataFifo.byIdxOut = NEXTIDX( l_DataFifo.byIdxOut, l_DataFifo.aDataItems ) ;
      rRet = ERR ;
   }

   pDataItem = &l_DataFifo.aDataItems[byCurIdxIn] ;
   strncpy( pDataItem->szStrData, i_szStrData, sizeof(pDataItem->szStrData) ) ;
   l_DataFifo.byIdxIn = byNextIdxIn ;

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ExecSendData( void )
{
   BYTE byIdxOut ;
   s_DataItem * pDataItem ;
   BOOL bUartAccept ;
   char * szStrData ;

   byIdxOut = l_DataFifo.byIdxOut ;

   if ( ( l_eWifiState != CWIFI_STATE_OFF ) && ( byIdxOut != l_DataFifo.byIdxIn ) &&
         ( ! l_DataFifo.bSuspend ) && uWifi_IsSendDone() )
   {
      pDataItem = &l_DataFifo.aDataItems[byIdxOut] ;

      szStrData = pDataItem->szStrData ;

      if ( ( szStrData[0] == 'a' ) && ( szStrData[1] == 't' ) &&
           ( szStrData[2] == '+' ) && ( szStrData[3] == 's' ) && ( szStrData[4] == '.' ) )
      {
         l_DataFifo.bSuspend = TRUE ;
      }

      bUartAccept = uwifi_Send( pDataItem->szStrData, strlen(pDataItem->szStrData) ) ;
      ERR_FATAL_IF( ! bUartAccept ) ;

      byIdxOut = NEXTIDX( byIdxOut, l_DataFifo.aDataItems )  ;
      l_DataFifo.byIdxOut = byIdxOut ;

      tim_StartMsTmp( &l_dwTmpDataMode ) ; // redémarrage tempo
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

static RESULT cwifi_WindCallBackDataMode( char C* i_pszProcessData, BOOL i_bPendingData )
{
   if ( ! i_bPendingData )
   {
      tim_StartMsTmp( &l_dwTmpDataMode ) ;  // redémarrage tempo
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
      l_DataFifo.bSuspend = FALSE ;
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
