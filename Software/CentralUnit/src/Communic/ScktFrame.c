/******************************************************************************/
/*                                                                            */
/*                                ScktFrame.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:   03 Jul. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "System.h"
#include "Main.h"

typedef enum
{
   SFRM_ID_NULL = 0,
   SFRM_ID_FIRST = 1,
   SFRM_ID_WIFI_BRIGE = SFRM_ID_FIRST,
   SFRM_ID_WIFI_SETSSID,
   SFRM_ID_WIFI_SETPWD,
   SFRM_ID_WIFI_EXITMAINT,
   SFRM_ID_WIFI_GETDEVICE,
   SFRM_ID_RAPI_BRIGE,
   SFRM_ID_LAST
} e_sfrmFrameId ;

typedef struct
{
   e_sfrmFrameId eFrameId ;
   char FrmStr[4] ;
   char FrmRes[5] ;
   BOOL bExtCmd ;
} s_FrameDesc ;

static s_FrameDesc const k_aFrameDesc [] =
{
   { .eFrameId = SFRM_ID_WIFI_BRIGE,     .FrmStr = "$01:", .FrmRes = "$81:", .bExtCmd = TRUE },
   { .eFrameId = SFRM_ID_WIFI_SETSSID,   .FrmStr = "$02:", .FrmRes = "$82:", .bExtCmd = FALSE },
   { .eFrameId = SFRM_ID_WIFI_SETPWD,    .FrmStr = "$03:", .FrmRes = "$83:", .bExtCmd = FALSE },
   { .eFrameId = SFRM_ID_WIFI_EXITMAINT, .FrmStr = "$04:", .FrmRes = "$84:", .bExtCmd = FALSE },
   { .eFrameId = SFRM_ID_WIFI_GETDEVICE, .FrmStr = "$05:", .FrmRes = "$85:", .bExtCmd = FALSE },
   { .eFrameId = SFRM_ID_RAPI_BRIGE,     .FrmStr = "$10:", .FrmRes = "$90:", .bExtCmd = FALSE },
} ;


/*----------------------------------------------------------------------------*/

static void sfrm_ProcessFrame( char * i_szStrFrm ) ;
static void sfrm_ExecCmd( e_sfrmFrameId i_eFrmId, char C* i_pszArg ) ;
static RESULT sfrm_WriteWifiId( BOOL i_bIsSsid, char C* i_szParam ) ;
static void sfrm_ProcessResExt( char C* i_szStrFrm, BOOL i_bLastCall ) ;
static void sfrm_SendRes( char C* i_szRes ) ;


/*----------------------------------------------------------------------------*/

static e_sfrmFrameId l_eFrmId ;


/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFunc( &sfrm_ProcessFrame, &sfrm_ProcessResExt ) ;
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

   pFrmDesc = &k_aFrameDesc[0] ;
   pszArg = NULL ;

   if ( l_eFrmId == SFRM_ID_NULL )
   {
      for ( byIdx = 0 ; byIdx < ARRAY_SIZE(k_aFrameDesc) ; byIdx++ )
      {
         if ( strncmp( i_szStrFrm, pFrmDesc->FrmStr, sizeof(pFrmDesc->FrmStr) ) == 0 )
         {
            l_eFrmId = pFrmDesc->eFrameId ;
            pszArg = i_szStrFrm + sizeof(pFrmDesc->FrmStr) ;

            if ( ! pFrmDesc->bExtCmd )
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
      sfrm_ExecCmd( l_eFrmId, pszArg ) ;

      if ( pFrmDesc->bExtCmd )
      {
         cwifi_AddExtData( "at+s." ) ;
      }
      else
      {
         l_eFrmId = SFRM_ID_NULL ;
      }
   }
}


/*----------------------------------------------------------------------------*/
static void sfrm_ExecCmd( e_sfrmFrameId i_eFrmId, char C* i_pszArg )
{
   char C* pszName ;

   switch ( i_eFrmId )
   {
      case SFRM_ID_WIFI_BRIGE :
         cwifi_AddExtCmd( i_pszArg ) ;
         break ;

      case SFRM_ID_WIFI_SETSSID :
         if ( sfrm_WriteWifiId( TRUE, i_pszArg ) == OK )
         {
            sfrm_SendRes( "OK\r\n" ) ;
         }
         else
         {
            sfrm_SendRes( "ERROR : Too large\r\n" ) ;
         }
         break ;

      case SFRM_ID_WIFI_SETPWD :
         if ( sfrm_WriteWifiId( FALSE, i_pszArg ) == OK )
         {
            sfrm_SendRes( "OK\r\n" ) ;
         }
         else
         {
            sfrm_SendRes( "ERROR : Too large\r\n" ) ;
         }
         break ;

      case SFRM_ID_WIFI_EXITMAINT :
         cwifi_SetMaintMode( FALSE ) ;
         sfrm_SendRes( "OK\r\n") ;
         cwifi_AskForRestart() ;
         break ;

      case SFRM_ID_WIFI_GETDEVICE :
         pszName = id_GetName() ;
         sfrm_SendRes( pszName ) ;
         break ;

      case SFRM_ID_RAPI_BRIGE :
         //op
         sfrm_SendRes( "cocolastico\r\n" ) ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/
static RESULT sfrm_WriteWifiId( BOOL i_bIsSsid, char C* i_szParam )
{
   DWORD dwEepAddr ;
   BYTE byEepSize ;
   BYTE byParamSize ;
   DWORD dwEepVal ;
   BYTE byShift ;
   char C* pszChar ;
   RESULT rRet ;

   if ( i_bIsSsid )
   {
      dwEepAddr = (DWORD)&g_sDataEeprom->sWifiConInfo.szWifiSSID ;
      byEepSize = sizeof(g_sDataEeprom->sWifiConInfo.szWifiSSID) ;
   }
   else
   {
      dwEepAddr = (DWORD)&g_sDataEeprom->sWifiConInfo.szWifiPassword ;
      byEepSize = sizeof(g_sDataEeprom->sWifiConInfo.szWifiPassword) ;
   }

   byParamSize = strlen( i_szParam ) + 1 ;

   if ( byParamSize <= byEepSize )
   {
      dwEepVal = 0 ;
      byShift = 0 ;
      pszChar = i_szParam ;
      while( byParamSize != 0 )
      {
         dwEepVal |= ( *pszChar << byShift ) ;
         byShift += 8 ;
         pszChar++ ;
         byParamSize-- ;
         if ( byShift == 32 )
         {
            eep_write( dwEepAddr, dwEepVal ) ;
            byShift = 0 ;
            dwEepVal = 0 ;
            dwEepAddr += 4 ;
         }
      }
      if ( byShift != 0 )
      {
         eep_write( dwEepAddr, dwEepVal ) ;
      }
      rRet = OK ;
   }
   else
   {
      rRet = ERR ;
   }
   return rRet ;
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
         l_eFrmId = SFRM_ID_NULL ;
      }
   }
}


/*----------------------------------------------------------------------------*/
static void sfrm_SendRes( char C* i_szParam )
{
   s_FrameDesc C* pFrmDesc ;
   char szRes [128] ;

   pFrmDesc = &k_aFrameDesc[ ( l_eFrmId - SFRM_ID_FIRST ) ] ;

   strncpy( szRes, pFrmDesc->FrmRes, sizeof(szRes) ) ;
   strncat( szRes, i_szParam, sizeof(szRes) ) ;

   cwifi_AddExtData( szRes ) ;
}