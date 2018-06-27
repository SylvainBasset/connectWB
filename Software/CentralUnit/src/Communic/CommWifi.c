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
} s_WindOperation ;

typedef struct
{
   char szCmdFmt [32] ;        // formatteur commande
   char * pszStrContent ;      // adresse chaine pour réception résultat
   WORD wContentSize ;
   f_CmdCallback fCallback ;
} s_CmdOperation ;

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
static BOOL l_bConnected ;
static char l_szWindIpWifiUp[32] ;

#define LIST_WIND( Op, Opr, Opf, Oprf ) \
   Op(  CONSOLE_RDY, ConsoleRdy, "0:",  &l_bConsoleRdy, TRUE ) \
   Opf( POWER_ON,    PowerOn,    "1:",  &l_bPowerOn,    TRUE ) \
   Op(  HRD_STARTED, HrdStarted, "32:", &l_bHrdStarted, TRUE ) \
   Opr( IP_WIFI_UP,  IpWifiUp,   "24:", &l_bConnected,  TRUE ) \
   Opf( INPUT,       Input,      "56:", NULL,           0 )

typedef enum //vérifier décallage avec 0
{
   CWIFI_WIND_NONE = 0,
   LIST_WIND( CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM, CWIFI_W_ENUM )
   CWIFI_WIND_LAST
} e_WindId ;

LIST_WIND( CWIFI_W_NULL, CWIFI_W_NULL, CWIFI_W_CALLBACK, CWIFI_W_CALLBACK )

static s_WindOperation const k_WindOperations [] =
{
   LIST_WIND( CWIFI_W_OPER, CWIFI_W_OPER_R, CWIFI_W_OPER_F, CWIFI_W_OPER_RF )
} ;


/*----------------------------------------------------------------------------*/
/* Commands/responses table definition                                        */
/*----------------------------------------------------------------------------*/

static char l_szRespPing [40] ;
static char l_szRespFsl [256] ;

#define LIST_CMD( Op, Opr, Oprf )              \
   Op(  AT,        At,        "AT\r" )              \
   Op(  SCFG,      SCfg,      "AT+S.SCFG=%s,%s\r" ) \
   Op(  SETSSID,   SetSsid,   "AT+S.SSIDTXT=%s\r" ) \
   Op(  CFUN,      CFun,      "AT+CFUN=%s\r" )      \
   Op(  SAVE,      Save,      "AT&W\r" )            \
   Op(  FACTRESET, FactReset, "AT&F\r" )            \
   Opr( PING,      Ping,      "AT+S.PING=%s\r")     \
   Opr( FSL,       Fsl,       "AT+S.FSL\r")

typedef enum
{
   CWIFI_CMD_NONE = 0,
   LIST_CMD( CWIFI_C_ENUM, CWIFI_C_ENUM, CWIFI_C_ENUM )
   CWIFI_CMD_LAST,
} e_CmdId ;

LIST_CMD( CWIFI_C_NULL, CWIFI_C_NULL, CWIFI_C_CALLBACK )

static s_CmdOperation const k_CmdOperations [] =
{
   LIST_CMD( CWIFI_C_OPER, CWIFI_C_OPER_R, CWIFI_C_OPER_RF )
} ;

typedef struct
{
   e_CmdId eCmdId ;
   e_CmdStatus eStatus ;
   WORD wStrContentIdx ;
} s_CmdCurStatus ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static BOOL cwifi_Connect( void ) ;
static void cwifi_SendCommand( e_CmdId i_eCmdId, char C* i_szArg1,
                                                 char C* i_szArg2 ) ;
static void cwifi_ProcessRec( void ) ;
static void cwifi_ProcessRecWind( char * i_pszProcessData, BOOL i_bPendingData  ) ;
static char C* cwifi_RSplit( char C* i_pszStr, char C* i_pszDelim ) ;
static void cwifi_ProcessRecResp( char * io_pszProcessData ) ;
static void cwifi_HrdInit( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_WifiState l_eWifiState ;
static s_CmdCurStatus l_CmdCurStatus ;
static BYTE l_byCmdConnectIdx ;
static DWORD l_dwTmpCmdTimeout ;


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

   cwifi_SetReset( FALSE ) ;

   uwifi_SetErrorDetection( TRUE ) ;
}


/*----------------------------------------------------------------------------*/
/* Wifi module reset                                                          */
/*----------------------------------------------------------------------------*/

void cwifi_SetReset( BOOL i_bReset )
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


/*----------------------------------------------------------------------------*/
/* Cyclic task ( period = 10 msec )                                           */
/*----------------------------------------------------------------------------*/

void cwifi_TaskCyc( void )
{
   BOOL bConnectDone ;
   DWORD dwTmpPing ;

   cwifi_ProcessRec() ;

   if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_END_ERR )
   {
      //flush buffer ;
      l_eWifiState = CWIFI_STATE_CONNECTING ;

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
         l_eWifiState = CWIFI_STATE_CONNECTING ;
         l_byCmdConnectIdx = 0 ;
      }
   }

   if ( l_eWifiState == CWIFI_STATE_CONNECTING )
   {
      bConnectDone = cwifi_Connect() ;

      if ( bConnectDone && l_bConnected )
      {
         l_eWifiState = CWIFI_STATE_CONNECTED ;
         tim_StartSecTmp( &dwTmpPing ) ;
      }
   }

   if ( l_eWifiState == CWIFI_STATE_CONNECTED )
   {
      if ( tim_IsEndSecTmp( &dwTmpPing, 5 ) )
      {
         tim_StartSecTmp( &dwTmpPing ) ;
         //cwifi_SendCommand( CWIFI_CMD_PING, "192.168.1.254", "" ) ;
         //cwifi_SendCommand( CWIFI_CMD_FSL, "", "" ) ;

      }
   }
}

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static BOOL cwifi_Connect( void )
{
   BOOL bDone ;
   char * pszWifiSSID ;
   char * pszWifiPassword ;

   bDone = FALSE ;

   if ( l_CmdCurStatus.eStatus == CWIFI_CMDST_NONE )
   {
      if ( l_byCmdConnectIdx >= 6 )
      {
         bDone = TRUE ;
      }
      else
      {
         switch ( l_byCmdConnectIdx )
         {
            case 0 :
               cwifi_SendCommand( CWIFI_CMD_SCFG, "wifi_priv_mode", "2" ) ;
               break ;
            case 1 :
               cwifi_SendCommand( CWIFI_CMD_SCFG, "wifi_mode", "1" ) ;
               break ;
            case 2 :
               pszWifiPassword = g_sDataEeprom->sWifiConInfo.szWifiPassword ;
               cwifi_SendCommand( CWIFI_CMD_SCFG, "wifi_wpa_psk_text", pszWifiPassword ) ;
               break ;
            case 3 :
               pszWifiSSID = g_sDataEeprom->sWifiConInfo.szWifiSSID ;
               cwifi_SendCommand( CWIFI_CMD_SETSSID, pszWifiSSID, "" ) ;
               break ;
            case 4 :
               cwifi_SendCommand( CWIFI_CMD_SAVE, "", "" ) ;
               break ;
            case 5 :
               cwifi_SendCommand( CWIFI_CMD_CFUN, "0", "" ) ;
               break ;
            default :
               ERR_FATAL() ;
         }
         l_byCmdConnectIdx++ ;
      }
   }
   else
   {
   }

   return bDone ;
}



/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_SendCommand( e_CmdId i_eCmdId, char C* i_szArg1, char C* i_szArg2 )
{
   BOOL bSendDone ;
   BOOL bCmdAccept ;
   char C* pszStrCmdFormat ;
   char szStrCmd[128] ;

   ERR_FATAL_IF( l_eWifiState == CWIFI_STATE_OFF ) ;

   bSendDone = uWifi_IsSendDone() ;
   ERR_FATAL_IF( ! bSendDone ) ;

   pszStrCmdFormat = k_CmdOperations[ (BYTE)(i_eCmdId) - 1 ].szCmdFmt ;

   sprintf( szStrCmd, pszStrCmdFormat, i_szArg1, i_szArg2 ) ;

   bCmdAccept = uwifi_Send( szStrCmd, strlen(szStrCmd) ) ;

   ERR_FATAL_IF( ! bCmdAccept ) ;

   l_CmdCurStatus.eCmdId = i_eCmdId ;
   l_CmdCurStatus.eStatus = CWIFI_CMDST_PROCESSING ;
   l_CmdCurStatus.wStrContentIdx = 0 ;

   tim_StartMsTmp( &l_dwTmpCmdTimeout ) ;
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
            pbyProcessData = abyReadData ;
            cwifi_ProcessRecResp( (char*)pbyProcessData ) ;
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
   s_WindOperation C* pWindOp ;
   BYTE byIdx ;
   RESULT rRet ;
   char * pszContent ;

   pWindOp = &k_WindOperations[0] ;
   for ( byIdx = 0 ; byIdx < ARRAY_SIZE( k_WindOperations ) ; byIdx ++ )
   {
      if ( strncmp( io_pszProcessData, pWindOp->szWindNum, strlen(pWindOp->szWindNum) ) == 0 )
      {
         rRet = OK ;

         if ( pWindOp->fCallback != NULL )
         {
             rRet = pWindOp->fCallback( io_pszProcessData, i_bPendingData ) ;
         }
         if ( ( rRet == OK ) && ( ! i_bPendingData ) )
         {
            if ( pWindOp->pbVar != NULL )
            {
               *( pWindOp->pbVar ) = pWindOp->bValue ;
            }
            if ( pWindOp->pszStrContent != NULL )
            {
               pszContent = (char*)cwifi_RSplit( io_pszProcessData, ":" ) ;
               pszContent[ strlen(pszContent) - 3 ] = '\0' ;
               strncpy( pWindOp->pszStrContent, pszContent, pWindOp->wContentSize ) ;
            }
         }
         break ;
      }
      pWindOp++ ;
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
      uwifi_Send( "1234\r\n", strlen("1234\r\n") ) ;
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
   s_CmdOperation C* pCmdOp ;
   RESULT rRet ;
   WORD wContentSize ;
   WORD wStrContentIdx ;
   WORD wNbRemainChar ;
   WORD wNbCharToCopy ;

   eStatus = l_CmdCurStatus.eStatus ;

   if ( eStatus == CWIFI_CMDST_PROCESSING )
   {
      pCmdOp = &k_CmdOperations[l_CmdCurStatus.eCmdId -1] ;

      if ( strncmp( io_pszProcessData, CWIFI_RESP_ERR, strlen(CWIFI_RESP_ERR) ) == 0 )
      {
         eStatus = CWIFI_CMDST_END_ERR ;
      }
      else if ( strncmp( io_pszProcessData, CWIFI_RESP_OK, strlen(CWIFI_RESP_OK) ) == 0 )
      {
         pCmdOp->pszStrContent[l_CmdCurStatus.wStrContentIdx] = '\0' ;

         rRet = OK ;

         if ( pCmdOp->fCallback != NULL )
         {
            rRet = pCmdOp->fCallback( pCmdOp->pszStrContent ) ;
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
         if ( pCmdOp->pszStrContent != NULL )
         {
            wContentSize = pCmdOp->wContentSize - 1 ;
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
            memcpy( &pCmdOp->pszStrContent[wStrContentIdx], io_pszProcessData, wNbCharToCopy ) ;

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
