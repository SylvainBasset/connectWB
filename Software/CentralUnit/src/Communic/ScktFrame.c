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


#define SFRM_CMD_BRIGE        "$01:"
#define SFRM_CMD_SETSSID      "$02:"
#define SFRM_CMD_SETPWD       "$03:"

//BYTE l_byIdx ;
//char l_aszLine [10][32] ;

static void sfrm_TreatFrame( char C* i_pszScktFrame ) ;
static void sfrm_TreatResExt( char C* i_pszScktFrame ) ;

/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFunc( &sfrm_TreatFrame, &sfrm_TreatResExt ) ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void sfrm_TreatFrame( char C* i_pszScktFrame )
{
   char C* pszData ;

   //cwifi_ToCmdMode() ;

   if ( strncmp( i_pszScktFrame, SFRM_CMD_BRIGE, strlen( SFRM_CMD_BRIGE ) ) == 0 )
   {
      pszData = i_pszScktFrame + strlen( SFRM_CMD_BRIGE ) ;
      cwifi_AddExtCmd( pszData ) ;
   }
   else if ( strncmp( i_pszScktFrame, SFRM_CMD_SETSSID, strlen( SFRM_CMD_SETSSID ) ) == 0 )
   {
      //TODO
   }
   else if ( strncmp( i_pszScktFrame, SFRM_CMD_SETPWD, strlen( SFRM_CMD_SETPWD ) ) == 0 )
   {
      //TODO
   }
}


/*----------------------------------------------------------------------------*/

static void sfrm_TreatResExt( char C* i_pszScktFrame )
{
   cwifi_AddExtData( i_pszScktFrame ) ;
}