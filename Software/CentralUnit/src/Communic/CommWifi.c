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
#include "System.h"
#include "System/Hard.h"

//#ifdef WIFI_PASSWORD
//#include "Communic/WifiPassword.h"
//#endif


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CWIFI_RESET_DURATION     200         /* wifi module reset duration (ms) */
#define CWIFI_PWRUP_DURATION       1         /* duration to wait after reset release (ms) */

#define CWIFI_CMD_TIMEOUT       2000

#define CWIFI_WIND_PREFIX       "+WIND"
#define CWIFI_WIND_OK           "OK\r\n"

typedef enum
{
   CWIFI_CMD_NONE = 0,
   CWIFI_CMD_FIRST = 1,
   CWIFI_CMD_AT = CWIFI_CMD_FIRST,
   CWIFI_CMD_CFG,
   CWIFI_CMD_SETSSID,
   CWIFI_CMD_CFUN,
   CWIFI_CMD_SAVE,
   CWIFI_CMD_FACTRESET,
   CWIFI_CMD_PING,
   CWIFI_CMD_LAST,
} e_CmdType ;

const char * k_pszStrCmdFormat [CWIFI_CMD_LAST-CWIFI_CMD_NONE] =
{
   "AT\r\0",
   "AT+S.SCFG=%s,%s\r\0",
   "AT+S.SSIDTXT=%s\r\0",
   "AT+CFUN=%s\r\0",
   "AT&W\r\0",
   "AT&F\r\0",
   "AT+S.PING\r\0",
} ;

typedef enum
{
   CWIFI_STATE_OFF = 0,
   CWIFI_STATE_RDY,
   CWIFI_STATE_SETSECKEY,
   CWIFI_STATE_CONNECTED,
} e_WifiState ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cwifi_Connect( void ) ;
static BOOL cwifi_SendAndWait( e_CmdType i_eType, char const * i_szArg1,
                                                  char const * i_szArg2 ) ;
static void cwifi_SendCommand( e_CmdType eType, char const * i_szArg1,
                                                char const * i_szArg2 ) ;
static void cwifi_ProcessRec( void ) ;
static void cwifi_ProcessRecWind( BYTE const * i_abyData ) ;
static void cwifi_ProcessRecResp( BYTE const * i_abyData ) ;
static void cwifi_HrdInit( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static char l_szStrCmd [128] ;
static e_CmdType l_eCurCmd ;
static e_WifiState l_eWifiState ;

static BOOL l_bPowerOn ;
static BOOL l_bConsoleRdy ;
static BOOL l_bHardwareStarted ;


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
   l_eCurCmd = CWIFI_CMD_NONE ;

   l_bPowerOn = FALSE ;
   l_bConsoleRdy = FALSE ;
   l_bHardwareStarted = FALSE ;

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
   cwifi_ProcessRec() ;

   if ( ( l_eWifiState == CWIFI_STATE_OFF ) &&
        l_bPowerOn && l_bConsoleRdy && l_bHardwareStarted )
   {
      l_eWifiState = CWIFI_STATE_RDY ;
      cwifi_Connect() ;
   }

   if ( l_eWifiState == CWIFI_STATE_RDY )
   {
      while(1) ;
   }

}

/*============================================================================*/

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_Connect( void )
{
   BOOL bCmdOk ;
   char szWifiSSID [32] = "default" ;
   char szWifiPassword [32] = "1324" ;

   bCmdOk = TRUE ;

   if ( bCmdOk )
   {
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_CFG, "wifi_priv_mode", "2" ) ;
   }
   if ( bCmdOk )
   {
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_CFG, "wifi_mode", "1" ) ;
   }
   if ( bCmdOk )
   {
      strcpy( szWifiPassword, g_sDataEeprom->sWifiConInfo.szWifiPassword ) ;
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_CFG, "wifi_wpa_psk_text", szWifiPassword ) ;
   }
   if ( bCmdOk )
   {
      strcpy( szWifiSSID, g_sDataEeprom->sWifiConInfo.szWifiSSID ) ;
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_SETSSID, szWifiSSID, "" ) ;
   }
   if ( bCmdOk )
   {
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_SAVE, "", "" ) ;
   }
   if ( bCmdOk )
   {
      bCmdOk = cwifi_SendAndWait( CWIFI_CMD_CFUN, "0", "" ) ;
   }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static BOOL cwifi_SendAndWait( e_CmdType i_eType, char const * i_szArg1,
                                                  char const * i_szArg2 )
{
   DWORD dwTmpTimeout ;
   BOOL bRet ;

   DWORD dwTmp ;
   tim_StartMsTmp( &dwTmp ) ;
   while ( ! tim_IsEndMsTmp( &dwTmp, 100 ) ) ;

   cwifi_SendCommand( i_eType, i_szArg1, i_szArg2 ) ;

   tim_StartMsTmp( &dwTmpTimeout ) ;

   bRet = TRUE ;

   while ( ! uWifi_IsSendDone() )
   {
      if ( tim_IsEndMsTmp( &dwTmpTimeout, CWIFI_CMD_TIMEOUT ) )
      {
         bRet = FALSE ;
         break ;
      }
   }
   if ( bRet )
   {
      while( l_eCurCmd != CWIFI_CMD_NONE )
      {
         cwifi_ProcessRec() ;

         if ( tim_IsEndMsTmp( &dwTmpTimeout, CWIFI_CMD_TIMEOUT ) )
         {
            bRet = FALSE ;
            break ;
         }
      }
   }
   return bRet ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_SendCommand( e_CmdType eType, char C* i_szArg1, char C* i_szArg2 )
{
   BOOL bCmdAccept ;
   char C* szStrCmdFormat ;

   ERR_FATAL_IF( l_eWifiState == CWIFI_STATE_OFF ) ;

   szStrCmdFormat = k_pszStrCmdFormat[ (BYTE)(eType) - 1 ] ;

   sprintf( l_szStrCmd, szStrCmdFormat, i_szArg1, i_szArg2 ) ;

   bCmdAccept = uwifi_Send( l_szStrCmd, strlen(l_szStrCmd) ) ;

   ERR_FATAL_IF( ! bCmdAccept ) ;

   l_eCurCmd = eType ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRec( void )
{
   BYTE abyReadData[512] ;
   WORD wNbReadVal ;
   BYTE * pbyProcessData ;

   wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), TRUE ) ;

   while ( wNbReadVal != 0 )
   {
      if ( wNbReadVal > 2 )
      {
         if ( strncmp( (char*)abyReadData, CWIFI_WIND_PREFIX, strlen(CWIFI_WIND_PREFIX) ) == 0 )
         {
            pbyProcessData = &abyReadData[sizeof(CWIFI_WIND_PREFIX)-1] ;
            cwifi_ProcessRecWind( pbyProcessData ) ;
         }
         else
         {
            cwifi_ProcessRecResp( abyReadData ) ;
         }
      }
      wNbReadVal = uwifi_Read( abyReadData, sizeof(abyReadData), TRUE ) ;
   }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecWind( BYTE const * i_abyData )
{
   if ( i_abyData[1] == '1' && i_abyData[2] == ':' )
   {
     if ( strstr( (char*)i_abyData, "SPWF01S" ) != 0 )
     {
        l_bPowerOn = TRUE ;
     }
   }

   else if ( i_abyData[1] == '0' && i_abyData[2] == ':' )
   {
       l_bConsoleRdy = TRUE ;
   }

   else if ( i_abyData[1] == '3' && i_abyData[2] == '2' && i_abyData[3] == ':' )
   {
      l_bHardwareStarted =  TRUE ;
   }
   else                                /* any other WIND indication is discarded */
   {
   }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

static void cwifi_ProcessRecResp( BYTE const * i_abyData )
{
   if ( l_eCurCmd != CWIFI_CMD_NONE )
   {
      if ( strncmp( (char*)i_abyData, CWIFI_WIND_OK, strlen(CWIFI_WIND_OK) ) == 0 )
      {
         l_eCurCmd = CWIFI_CMD_NONE ;
      }
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