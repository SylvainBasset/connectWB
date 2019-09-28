/******************************************************************************/
/*                                                                            */
/*                                ScktFrame.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:   03 Jul. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Control.h"
#include "Communic.h"
#include "System.h"
#include "Main.h"


/*----------------------------------------------------------------------------*/

#define SFRM_DATA_ITEM_SIZE    100 //SBA voir si augementation

typedef enum
{
   SFRM_ID_NULL = 0,
   SFRM_ID_FIRST = 1,
   SFRM_ID_WIFI_BRIGE = SFRM_ID_FIRST,
   SFRM_ID_WIFI_SETSSID,
   SFRM_ID_WIFI_SETPWD,
   SFRM_ID_WIFI_EXITMAINT,
   SFRM_ID_GETDEVICE,
   SFRM_ID_RAPI_BRIGE,
   SFRM_ID_RAPI_CHARGEINFO,
   SFRM_ID_CHARGE_HISTSTATE,
   SFRM_ID_CHARGE_ADCVAL,
   SFRM_ID_RESET,
   SFRM_ID_LAST
} e_sfrmFrameId ;

typedef struct
{
   e_sfrmFrameId eFrmId ;
   char szCmd[4] ;
   char szRes[5] ;
   BOOL bWifimodule ;
   BOOL bDelayRes ;
} s_FrameDesc ;

#define _D( Name, StrCmd, StrRes, Wifimodule, DelayRes ) \
   { .eFrmId = SFRM_ID_##Name, .szCmd = StrCmd, .szRes = StrRes, \
     .bWifimodule = Wifimodule, .bDelayRes = DelayRes }

static s_FrameDesc const k_aFrameDesc [] =
{
   _D( WIFI_BRIGE,       "$01:", "$81:", TRUE,  TRUE ),
   _D( WIFI_SETSSID,     "$02:", "$82:", FALSE, FALSE ),
   _D( WIFI_SETPWD,      "$03:", "$83:", FALSE, FALSE ),
   _D( WIFI_EXITMAINT,   "$04:", "$84:", FALSE, FALSE ),
   _D( GETDEVICE,        "$05:", "$85:", FALSE, FALSE ),
   _D( RAPI_BRIGE,       "$10:", "$90:", FALSE, TRUE ),
   _D( RAPI_CHARGEINFO,  "$11:", "$91:", FALSE, FALSE ),
   _D( CHARGE_HISTSTATE, "$12:", "$92:", FALSE, FALSE ),
   _D( CHARGE_ADCVAL,    "$13:", "$93:", FALSE, FALSE ),
   _D( RESET,            "$7F:", "$FF:", FALSE, FALSE ),
} ;


/*----------------------------------------------------------------------------*/

static void sfrm_ProcessFrame( char * i_szStrFrm ) ;
static void sfrm_ExecCmd( char C* i_pszArg ) ;
static void sfrm_ProcessResExt( char C* i_szStrFrm, BOOL i_bLastCall ) ;
static void sfrm_SendRes( char C* i_szRes ) ;


/*----------------------------------------------------------------------------*/

static e_sfrmFrameId l_eFrmId ;


/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFunc( &sfrm_ProcessFrame, &sfrm_ProcessResExt ) ;
   coevse_RegisterRetScktFunc( &sfrm_ProcessResExt ) ;

   l_eFrmId = SFRM_ID_NULL ;
}


/*============================================================================*/

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

   for ( byIdx = 0 ; byIdx < ARRAY_SIZE(k_aFrameDesc) ; byIdx++ )
   {
      if ( strncmp( i_szStrFrm, pFrmDesc->szCmd, sizeof(pFrmDesc->szCmd) ) == 0 )
      {
         eFrmId = pFrmDesc->eFrmId ;
         pszArg = i_szStrFrm + sizeof(pFrmDesc->szCmd) ;

         if ( ! pFrmDesc->bWifimodule )
         {
            pszChar = pszArg + strlen(pszArg) - 2 ;
            if ( ( ( *pszChar == '\n' ) || ( *pszChar == '\r' ) ) &&
                 ( ( *( pszChar + 1 ) == '\n' ) || ( *( pszChar + 1 ) == '\r' ) ) )
            {
               *pszChar = 0 ;
            }
         }
         break ;
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

         if ( ( ! pFrmDesc->bDelayRes ) || pFrmDesc->bWifimodule )     //SBA : pas besoin sortie direct data mode pour com openEVSE
         {
            cwifi_AddExtData( "at+s." ) ;
         }
         cwifi_AskFlushData() ;

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
   char szStrInfo [48] ;
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
         pszName = id_GetName() ; //SBA vérifier pourquoi ca marche sans '\r\n' ??
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

      case SFRM_ID_CHARGE_ADCVAL :
         cstate_GetAdcVal( szStrInfo, sizeof(szStrInfo) );
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
   BYTE byResSize ;

   if ( l_eFrmId != SFRM_ID_NULL )
   {
      pFrmDesc = &k_aFrameDesc[ ( l_eFrmId - SFRM_ID_FIRST ) ] ;

      pszRes = szRes ;
      byResSize = sizeof(szRes) ;

      strncpy( pszRes, pFrmDesc->szRes, byResSize ) ;
      pszRes += strlen( pFrmDesc->szRes ) ;
      byResSize -= strlen( pFrmDesc->szRes ) ;

      strncpy( pszRes, i_szParam, byResSize ) ;

         /* Note : force the end of string */

      szRes[ sizeof(szRes)-1 ] = '\0' ;

      cwifi_AddExtData( szRes ) ;
   }
}
