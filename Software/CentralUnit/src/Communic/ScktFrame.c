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

//#define SFRM_CMD_LISTFILE     "Cmd-01"
//#define SFRM_CMD_OPENFILE     "Cmd-02:%s"
//#define SFRM_CMD_READFILE     "Cmd-03"
//#define SFRM_CMD_WRITEFILE    "Cmd-04"
//#define SFRM_CMD_ERASEFILE    "Cmd-05"

//BYTE l_byIdx ;
//char l_aszLine [10][32] ;

static void sfrm_TreatLine( char * i_pszScktFrame ) ;

/*----------------------------------------------------------------------------*/

void sfrm_Init( void )
{
   cwifi_RegisterScktFrameFunc( &sfrm_TreatLine ) ;
}


/*----------------------------------------------------------------------------*/

static void sfrm_TreatLine( char * i_pszScktFrame )
{
}
