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

typedef enum
{
   SFRM_ID_NULL = 0,
   SFRM_ID_FIRST = 1,
   SFRM_ID_BRIGEWIFI = SFRM_ID_FIRST,
   SFRM_ID_SETSSID,
   SFRM_ID_SETPWD,
   SFRM_ID_BRIGERAPI,
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
   { .eFrameId = SFRM_ID_BRIGEWIFI, .FrmStr = "$01:", .FrmRes = "$81:", .bExtCmd = TRUE },
   { .eFrameId = SFRM_ID_SETSSID,   .FrmStr = "$02:", .FrmRes = "$82:", .bExtCmd = TRUE },
   { .eFrameId = SFRM_ID_SETPWD,    .FrmStr = "$03:", .FrmRes = "$83:", .bExtCmd = TRUE },
   { .eFrameId = SFRM_ID_BRIGERAPI, .FrmStr = "$10:", .FrmRes = "$90:", .bExtCmd = FALSE },
} ;


/*----------------------------------------------------------------------------*/

static void sfrm_TreatFrame( char C* i_szStrFrm ) ;
static void sfrm_TreatResExt( char C* i_szStrFrm, BOOL i_bLastCall ) ;
static void sfrm_SendRes( char C* i_szRes ) ;


/*----------------------------------------------------------------------------*/

static e_sfrmFrameId l_eFrmId ;


/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFunc( &sfrm_TreatFrame, &sfrm_TreatResExt ) ;
   l_eFrmId = SFRM_ID_NULL ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
static void sfrm_TreatFrame( char C* i_szStrFrm )
{
   BYTE byIdx ;
   s_FrameDesc C* pFrmDesc ;
   char C* pszArg ;

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
            break ;
         }
         pFrmDesc++ ;
      }

      switch ( l_eFrmId )
      {
         case SFRM_ID_BRIGEWIFI :
            cwifi_AddExtCmd( pszArg ) ;
            break ;
         case SFRM_ID_SETSSID :
            cwifi_AddExtCmd( "AT\r\n" ) ;
            break ;
         case SFRM_ID_SETPWD :
            cwifi_AddExtCmd( "AT\r\n" ) ;
            break ;
         case SFRM_ID_BRIGERAPI :
            //op
            sfrm_SendRes( "cocolastico\n" ) ;
         default :
            break ;
      }

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
static void sfrm_TreatResExt( char C* i_szStrFrm, BOOL i_bLastCall )
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
