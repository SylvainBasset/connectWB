/******************************************************************************/
/*                                ScktFrame.c                                 */
/******************************************************************************/
/*
   Socket frames managment module

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
   @history 1.0, 03 Jul. 2018, creation
   @brief
   Implement a simple command/response protocol to communicate with rest of software

   Data recevied by Wifi module through socket channel are treated as "ScktFrame"
   commands/responses.
   At initialisation, this module register reception function (sfrm_ProcessFrame)
   to CommWifi module. This is  the data input of this module.

   A "posting resutls" function (sfrm_ProcessResExt) is also registered to each
   module on which operations are not immediately executed. "Delayed" response is
   so posted by calling this function.

   Socket frame commands are treated as ASCII values. They must begin by '$' followed
   a 2 digit hex code (from 00h to 7Fh) reffering to the command number, follwed by a
   ':' charater. The command may contain a parameter placed after the comma.

   Each command also have a corresponding response. Responses codes use ranges
   from 80h to FFh with same offset as command code (response code = command code + 80h)

   Available commands are :

   $01:<arg> : wifi bridge (reponse code 0x81) : Received argument is sent back to
               Wifi module as an external AT command. The response is delayed. Execution
               result is sent with the response.
   $02:<arg> : SSID set (reponse code 0x82) : Received argument is used as new home
               wifi SSID (stored in eeprom).
   $03:<arg> : Password set (reponse code 0x83) : Received argument is used as new
               home wifi password (stored in eeprom).
   $04:      : Exit maintenance mode (reponse code 0x84) : Force the Wifi module to
               leave maintenance mode.
   $05:      : Get device name (reponse code 0x84) : device name is sent with the
               response
   $10:<arg> : OpenEvse RAPI bridge (reponse code 0x90) : Received argument is sent
               to OpenEVSE module as an external command. The response is delayed.
               Execution result is sent with the response.
   $11:      : Get charge information (response code 0x91) : Charge information are
               sent with the response
   $12:      : Get charge history (response code 0x92) : The 10 last charge states
               are sent with the response (see ChargeState.c)
   $13:      : RAPI (openEvse) Sx commands history
   $14:      : Get OpenEVSE asynchronous state
   $7F:      : "ScktFrame" reset (response code 0xFF) : reset the "ScktFrame" state
               <l_eFrmId>, in case of pending delayed response.

   When a command invloves a delayed response, the response is not sent imediately.
   Instead, <l_eFrmId> is set with the command code. When the response is ready, it
   is sent by sfrm_ProcessResExt (reponse code is then computed given <l_eFrmId>value).
   The command/response is finished (l_eFrmId at SFRM_ID_NULL) only when the
   <i_bLastCall> argument is set.
   It is not possible to send an other command while a delayed repsonse is pending
   (l_eFrmId != SFRM_ID_NULL) execpt for the "ScktFrame" reset command ("$7F:").
*/


#include "Define.h"
#include "Control.h"
#include "Communic.h"
#include "System.h"
#include "Main.h"



/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define SFRM_DATA_PAYLOAD_SIZE         512   /* Frame payload size */
                                             /* Frame response size (plus response code size) */
#define SFRM_DATA_ITEM_SIZE \
            ( SFRM_DATA_PAYLOAD_SIZE + 5 )

typedef enum                                 /* Frames command Ids */
{
   SFRM_ID_NULL = 0,
   SFRM_ID_FIRST = 1,
   SFRM_ID_WIFI_BRIGE = SFRM_ID_FIRST,       /* $01: wifi bridge */
   SFRM_ID_WIFI_SETSSID,                     /* $02: SSID set */
   SFRM_ID_WIFI_SETPWD,                      /* $03: Password set */
   SFRM_ID_WIFI_EXITMAINT,                   /* $04: Exit maintenance mode */
   SFRM_ID_GETDEVICE,                        /* $05: Get device name */

   SFRM_ID_RAPI_BRIGE,                       /* $10: OpenEvse RAPI bridge */
   SFRM_ID_RAPI_CHARGEINFO,                  /* $11: Get charge information */
   SFRM_ID_CHARGE_HISTSTATE,                 /* $12: Get charge history */
   SFRM_ID_COEVSE_HIST,                      /* $13: Get RAPI Sx History */
   SFRM_ID_COEVSE_ASYNCH,                    /* $14: Get OpenEVSE asynchronous state */

   SFRM_ID_ERRORS_LIST,                      /* $20: Get error list */

   SFRM_ID_RESET,                            /* $7F: "ScktFrame" reset */

   SFRM_ID_LAST
} e_sfrmFrameId ;

typedef struct                               /* Frame descriptor */
{
   e_sfrmFrameId eFrmId ;                    /* Frame Identificator */
   char szCmd[4] ;                           /* Associated command string */
   char szRes[5] ;                           /* Associated reponse string */
   BOOL bWifimodule ;                        /* This command/reponse frame invlove exchanges with Wifi module */
   BOOL bDelayRes ;                          /* Delayed response */
} s_FrameDesc ;

                                             /* descriptor define macro */
#define _D( Name, StrCmd, StrRes, Wifimodule, DelayRes ) \
   { .eFrmId = SFRM_ID_##Name, .szCmd = StrCmd, .szRes = StrRes, \
     .bWifimodule = Wifimodule, .bDelayRes = DelayRes }

                                             /* table of frame descriptor */
static s_FrameDesc const k_aFrameDesc [] =
{
   _D( WIFI_BRIGE,       "$01:", "$81:", TRUE,  TRUE  ),
   _D( WIFI_SETSSID,     "$02:", "$82:", FALSE, FALSE ),
   _D( WIFI_SETPWD,      "$03:", "$83:", FALSE, FALSE ),
   _D( WIFI_EXITMAINT,   "$04:", "$84:", FALSE, FALSE ),
   _D( GETDEVICE,        "$05:", "$85:", FALSE, FALSE ),
   _D( RAPI_BRIGE,       "$10:", "$90:", FALSE, TRUE  ),
   _D( RAPI_CHARGEINFO,  "$11:", "$91:", FALSE, FALSE ),
   _D( CHARGE_HISTSTATE, "$12:", "$92:", FALSE, FALSE ),
   _D( COEVSE_HIST,      "$13:", "$93:", FALSE, FALSE ),
   _D( COEVSE_ASYNCH,    "$14:", "$94:", FALSE, FALSE ),
   _D( ERRORS_LIST,      "$20:", "$A0:", FALSE, FALSE ),
   _D( RESET,            "$7F:", "$FF:", FALSE, FALSE ),
} ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void sfrm_ProcessFrame( char * i_szStrFrm ) ;
static void sfrm_ExecCmd( char C* i_pszArg ) ;
static void sfrm_ProcessResExt( char C* i_szStrFrm, BOOL i_bLastCall ) ;
static void sfrm_SendRes( char C* i_szRes ) ;


/*----------------------------------------------------------------------------*/
/* variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_sfrmFrameId l_eFrmId ;



/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFunc( &sfrm_ProcessFrame, &sfrm_ProcessResExt ) ;
   coevse_RegisterRetScktFunc( &sfrm_ProcessResExt ) ;

   l_eFrmId = SFRM_ID_NULL ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Process frame command                                                      */
/*----------------------------------------------------------------------------*/

static void sfrm_ProcessFrame( char * i_szStrFrm )
{
   BYTE byIdx ;
   s_FrameDesc C* pFrmDesc ;
   char * pszArg ;
   char * pszChar ;
   e_sfrmFrameId eFrmId ;

   pFrmDesc = &k_aFrameDesc[0] ;
   pszArg = NULL ;

   eFrmId = SFRM_ID_NULL ;
                                       /* find the command/response frame Id */
   for ( byIdx = 0 ; byIdx < ARRAY_SIZE(k_aFrameDesc) ; byIdx++ )
   {
      if ( strncmp( i_szStrFrm, pFrmDesc->szCmd, sizeof(pFrmDesc->szCmd) ) == 0 )
      {
         eFrmId = pFrmDesc->eFrmId ;
         pszArg = i_szStrFrm + sizeof(pFrmDesc->szCmd) ;

         if ( ! pFrmDesc->bWifimodule ) /* if frame content is sent directly to wifi module */
         {
            pszChar = pszArg + strlen(pszArg) - 2 ;
                                       /* remove final CR/LF */
            if ( ( ( *pszChar == '\n' ) || ( *pszChar == '\r' ) ) &&
                 ( ( *( pszChar + 1 ) == '\n' ) || ( *( pszChar + 1 ) == '\r' ) ) )
            {
               *pszChar = 0 ;
            }
         }
         break ;                       /* frame Id found */
      }
      pFrmDesc++ ;
   }

   if ( eFrmId == SFRM_ID_RESET )
   {
      l_eFrmId = SFRM_ID_NULL ;
   }
   else
   {
      if ( ( eFrmId != SFRM_ID_NULL ) && ( l_eFrmId == SFRM_ID_NULL ) )
      {
         l_eFrmId = eFrmId ;
         sfrm_ExecCmd( pszArg ) ;

         //SBA : pas besoin sortie direct data mode pour com openEVSE

         if ( ( ! pFrmDesc->bDelayRes ) || pFrmDesc->bWifimodule )
         {
            cwifi_AddExtData( "at+s." ) ;
         }
         cwifi_AskFlushData() ;              //SBA : pas nÃ©cessaire si pas bridge ?

         if ( ! pFrmDesc->bDelayRes )
         {
            l_eFrmId = SFRM_ID_NULL ;
         }
      }
   }
}


/*----------------------------------------------------------------------------*/
static void sfrm_ExecCmd( char C* i_pszArg )
{
   char szStrInfo [SFRM_DATA_PAYLOAD_SIZE] ;
   char C* pszName ;
   RESULT rRet ;

   switch ( l_eFrmId )
   {
      case SFRM_ID_WIFI_BRIGE :
         rRet = cwifi_AddExtCmd( i_pszArg ) ;
         if ( rRet != OK )
         {
            l_eFrmId = SFRM_ID_NULL ;
         }
         break ;

      case SFRM_ID_WIFI_SETSSID :
         if ( eep_WriteWifiId( TRUE, i_pszArg ) == OK )
         {
            sfrm_SendRes( "OK\r\n" ) ;
         }
         else
         {
            sfrm_SendRes( "ERROR : Too large\r\n" ) ;
         }
         break ;

      case SFRM_ID_WIFI_SETPWD :
         if ( eep_WriteWifiId( FALSE, i_pszArg ) == OK )
         {
            sfrm_SendRes( "OK\r\n" ) ;
         }
         else
         {
            sfrm_SendRes( "ERROR : Too large\r\n" ) ;
         }
         break ;

      case SFRM_ID_WIFI_EXITMAINT :
         sfrm_SendRes( "OK\r\n") ;
         cwifi_SetMaintMode( FALSE ) ;
         break ;

      case SFRM_ID_GETDEVICE :
         pszName = id_GetName() ; //SBA vÃ©rifier pourquoi ca marche sans '\r\n' ??
         sfrm_SendRes( pszName ) ;
         break ;

      case SFRM_ID_RAPI_BRIGE :
         rRet = coevse_AddExtCmd( i_pszArg ) ;
         rRet = OK ;
         if ( rRet != OK )
         {
            l_eFrmId = SFRM_ID_NULL ;
         }
         break ;

      case SFRM_ID_RAPI_CHARGEINFO :
         coevse_FmtInfo( szStrInfo, sizeof(szStrInfo) ) ;
         sfrm_SendRes( szStrInfo ) ;
         break ;

      case SFRM_ID_CHARGE_HISTSTATE :
         cstate_GetHistState( szStrInfo, sizeof(szStrInfo) ) ;
         sfrm_SendRes( szStrInfo ) ;
         break ;

      case SFRM_ID_COEVSE_HIST :
         coevse_GetHist( szStrInfo, sizeof(szStrInfo) ) ;
         sfrm_SendRes( szStrInfo ) ;
         break ;

      case SFRM_ID_COEVSE_ASYNCH :
         coevse_GetAsyncState( szStrInfo, sizeof(szStrInfo) ) ;
         sfrm_SendRes( szStrInfo ) ;
         break ;

      case SFRM_ID_ERRORS_LIST :
         err_GetErrorList( NULL, FALSE, szStrInfo, sizeof(szStrInfo) );
         sfrm_SendRes( szStrInfo ) ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/
static void sfrm_ProcessResExt( char C* i_szStrFrm, BOOL i_bLastCall )
{
   if ( l_eFrmId != SFRM_ID_NULL )
   {
      sfrm_SendRes( i_szStrFrm ) ;

      if ( i_bLastCall )
      {
         cwifi_AddExtData( "at+s." ) ;
         cwifi_AskFlushData() ;
         l_eFrmId = SFRM_ID_NULL ;
      }
   }
}


/*----------------------------------------------------------------------------*/
static void sfrm_SendRes( char C* i_szParam )
{
   s_FrameDesc C* pFrmDesc ;
   char szRes [SFRM_DATA_ITEM_SIZE] ;
   char * pszRes ;
   WORD wResSize ;

   if ( l_eFrmId != SFRM_ID_NULL )
   {
      pFrmDesc = &k_aFrameDesc[ ( l_eFrmId - SFRM_ID_FIRST ) ] ;

      pszRes = szRes ;
      wResSize = sizeof(szRes) ;

      strncpy( pszRes, pFrmDesc->szRes, wResSize ) ;
      pszRes += strlen( pFrmDesc->szRes ) ;
      wResSize -= strlen( pFrmDesc->szRes ) ;

      strncpy( pszRes, i_szParam, wResSize ) ;

         /* Note : force the end of string */

      szRes[ sizeof(szRes)-1 ] = '\0' ;

      cwifi_AddExtData( szRes ) ;
   }
}
