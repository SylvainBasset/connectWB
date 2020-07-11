/******************************************************************************/
/*                                 CommOEvse.c                                */
/******************************************************************************/
/*
   OpenEVSE communication module

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
   @history 1.0, 04 oct. 2018, creation
   @brief
   Implement communication with openEVSE module.

   Entries points are provided to the application for basic settings
   (e.g. coevse_SetCurrentCap() ). These entries points involes a
   reduced set of RAPI commmands desbribed by e_CmdId.

   It is possible to send data directly to the module with coevse_AddExtCmd()
   (bridge mode, COEVSE_CMD_EXTCMD command id)

   Asked RAPI commands are stored in FIFO (l_CmdFifo). Once this FIFO is not
   empty, and the communication is free (l_eCmd == COEVSE_CMD_NONE), the
   command out of the FIFO is sent.

   At RAPI command sending, a timeout of COEVSE_TIMEOUT ms is started. If the
   response is wrong (bad CRC) or the timeout is over, the command is sent
   again, with a maximum of COEVSE_MAXRETRY reties.

   The cyclic task perform verification of charging metrics by sending
   COEVSE_CMD_GETEVSESTATE,
   COEVSE_CMD_GETCURRENTCAP,
   COEVSE_CMD_GETFAULT,
   COEVSE_CMD_GETCHARGPARAM,
   COEVSE_CMD_GETENERGYCNT,
   every COEVSE_GETSTATE_PER seconds

   Responses from these commands is stored by coevse_Cmdresult...() callbacks
*/


#include "Define.h"
#include "Lib.h"
#include "Communic.h"
#include "Communic/l_Communic.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define COEVSE_BAUDRATE     115200llu  /* baudrate (bits per second) value  */

#define COEVSE_TIMEOUT           200   /* communication timeout, ms */

   /* Note : communication timeout would be higher than main.c duration */
   /* to set save mode (MAIN_SAVEMODE_PIN_DUR). Otherwise, in case of   */
   /* power-down, a communication error may be throwed (depending on    */
   /* retries numbers) before the power save mode is effective.         */

#define COEVSE_GETSTATE_PER     1000   /* OpenEVSE status read period */
#define COEVSE_HRD_START_DUR    8000   /* OpenEVSE hardware startup duration */
#define COEVSE_MAXRETRY            4   /* maximum number of retry before error */

#define COEVSE_MAX_CMD_LEN        29   /* maximum size for RAPI command */

                                       /* disable/suspend transmit channel DMA */
#define COEVSE_DISABLE_DMA_TX()     ( UOEVSE_DMA_TX->CCR &= ~DMA_CCR_EN )
                                       /* enable transmit channel DMA */
#define COEVSE_ENABLE_DMA_TX()      ( UOEVSE_DMA_TX->CCR |= DMA_CCR_EN )


static char const k_szStrReset [] = "$FR^30\r" ;


typedef void (*f_ResultCallback)( char C* i_pszDataRes ) ;


   /* Note : LIST_CMD() defines the CRC value (<szChecksum> field of         */
   /* s_CmdDesc struct) when the command does not contain variables elements.*/
   /* At each cmd sending, <szChecksum> field is analysed: if the adresse    */
   /* is not NULL, the pointed CRC value is taken insted of computing the    */
   /* whole CRC.                                                             */
   /* In brief :                                                             */
   /* if the command is const (no param) -> fill the CRC value (optimistion) */
   /* if the command is variable -> keep the filed at NULL to compute CRC    */

   /* Note :                                                            */
   /* Macro Op = macro for command without callback                     */
   /* Macro Opg = macro for command with callback (coevse_Cmdresult...) */


#define LIST_CMD( Op, Opg ) \
   Op(   ENABLE,        Enable,        "$FE",    "^27\r" ) \
   Op(   DISABLE,       Disable,       "$FD",    "^26\r" ) \
   Op(   SETLOCK,       SetLock,       "$S4 %d", NULL    ) \
   Op(   SETCURRENTCAP, SetCurrentCap, "$SC %d", NULL    ) \
   Opg(  GETEVSESTATE,  GetEVSEState,  "$GS",    "^30\r" ) \
   Opg(  GETCURRENTCAP, GetCurrentCap, "$GE",    "^26\r" ) \
   Opg(  GETFAULT,      GetFault,      "$GF",    "^25\r" ) \
   Opg(  GETCHARGPARAM, GetChargParam, "$GG",    "^24\r" ) \
   Opg(  GETENERGYCNT,  GetEneryCnt,   "$GU",    "^36\r" ) \
   Opg(  GETVERSION,    GetVersion,    "$GV",    "^35\r" ) \
   Opg(  EXTCMD,        ExtCmd,        NULL,     NULL ) \

typedef enum                           /* reduced set of used RAPI commands */
{
   COEVSE_CMD_NONE = 0,
   LIST_CMD( COEVSE_CMD_ENUM, COEVSE_CMD_ENUM )
   COEVSE_CMD_LAST,
} e_CmdId ;
                                       /* prototypes declaration for commands callbacks */
LIST_CMD( COEVSE_CMD_NULL, COEVSE_CMD_CALLBACK )

typedef struct                         /* RAPI command const data */
{
   e_CmdId eCmdId ;                    /* command Id, COEVSE_CMD_... */
   f_ResultCallback fResultCallback ;  /* callback, NULL if no callback */
   char C* szFmtCmd ;                  /* command string */
   char C* szChecksum ;                /* checksum value, NULL for dynamic computation*/
} s_CmdDesc ;

static s_CmdDesc const k_aCmdDesc [] = /* list of API commands const data */
{
   LIST_CMD( COEVSE_CMD_DESC, COEVSE_CMD_DESC_G )
} ;

typedef struct                         /* one element of command FIFO */
{
   e_CmdId eCmdId ;                    /* command ID */
   WORD wParam [6] ;                   /* command params */
   BYTE byNbParam ;                    /* nb params */
} s_CmdFifoData ;

typedef struct                         /* command FIFO */
{
   BYTE byIdxIn ;                      /* input index */
   BYTE byIdxOut ;                     /* output index */
   s_CmdFifoData aCmdData [8] ;        /* element data */
} s_CmdFifo ;


typedef struct                         /* response of RAPI commmand */
{
   BYTE abyDataRes [32+1] ;            /* response data */
   BYTE byResIdx ;                     /* response data index (reponse length) */
   BOOL bError ;                       /* Uart module error */
   BOOL bWaitResponse ;                /* pending response reception indicator */
} s_coevseResult ;


typedef struct                         /* module data */
{
   e_coevseEvseState eEvseState ;      /* current openEVSE state */
   BOOL bPlugEvent ;                   /* plugging event (transition from state A to B) */
   DWORD dwCurrentCap ;                /* charing current maximum capacity in A */
   SDWORD sdwChargeVoltage ;           /* charing voltage in mV */
   SDWORD sdwChargeCurrent ;           /* charing current in mA */
   DWORD dwCurWh ;                     /* energy compuption of this charging session in Wh */
   DWORD dwAccWh ;                     /* energy compuption of all charging session in Wh */

   DWORD dwErrGfiTripCnt ;             /* Gfi error counter */
   DWORD dwNoGndTripCnt ;              /* Gournd error counter */
   DWORD dwStuckRelayTripCnt ;
} s_coevseData ;

typedef struct
{
   char szHistStr [500] ;
   WORD wIdx ;
} s_HistCmd ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static BOOL coevse_IsNeedSend( void ) ;
static void coevse_AddCmdFifo( e_CmdId i_eCmdId, WORD * i_awParams, BYTE i_byNbParam ) ;
static void coevse_SendCmdFifo( void ) ;
static void coevse_AnalyseRes( void ) ;

static void coevse_GetChecksum( char * o_sChecksum, BYTE i_byCkSize,
                                char C* i_szData ) ;

static void coevse_CmdStart( e_CmdId i_eCmdId ) ;
static void coevse_CmdSetErr( void ) ;
static void coevse_SetError( void ) ;
static void coevse_CmdEnd( void ) ;

static void coevse_HistAddCmd( char C* i_pszStrCmd ) ;

static void coevse_HrdInit( void ) ;
static void coevse_HrdSendCmd( char C* i_pszStrCmd, BYTE i_bySize ) ;


/*----------------------------------------------------------------------------*/
/* variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_CmdId l_eCmd ;                /* current sending command, COEVSE_CMD_NONE if no command */

static f_PostResProc l_fPostResProc ;  /* addresse of callback function for external command result */
                                       /* external command string */
static char l_szExtCmdStr [ COEVSE_MAX_CMD_LEN + 1 ] ;
static s_CmdFifo l_CmdFifo ;           /* command FIFO */
                                       /* Buffer for sending command (must be
                                          declared in static bescause of use of DMA) */
static char l_szStrCmdBuffer [ COEVSE_MAX_CMD_LEN + 1 ] ;

static BYTE l_byNbRetry ;              /* current retry number */
static DWORD l_dwCmdTimeout ;          /* timeout temporisation */
static BOOL l_bOpenEvseRdy ;           /* hardware openEVSE ready state */
static DWORD l_dwTmpStart ;

static s_coevseResult l_Result ;
static s_coevseResult l_Async ;

static DWORD l_dwGetStateTmp ;
static s_coevseData l_Status ;

static s_HistCmd l_HistCmd ;           /* RAPI Sx command history */


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void coevse_Init( void )
{
   coevse_HrdInit() ;

   coevse_HrdSendCmd( k_szStrReset, sizeof(k_szStrReset) ) ;
   l_bOpenEvseRdy = FALSE ;

   tim_StartMsTmp( &l_dwTmpStart ) ;

   coevse_AddCmdFifo( COEVSE_CMD_ENABLE, NULL, 0 ) ;
}


/*----------------------------------------------------------------------------*/
/* Registering callback function for external function result                 */
/*----------------------------------------------------------------------------*/

void coevse_RegisterRetScktFunc( f_PostResProc i_fPostResProc )
{
   l_fPostResProc = i_fPostResProc ;
}


/*----------------------------------------------------------------------------*/
/* Set current capacity                                                       */
/*----------------------------------------------------------------------------*/

void coevse_SetChargeEnable( BOOL i_bEnable )
{
   WORD awParam [1] ;

   if ( i_bEnable )                    /* if charge is enabled */
   {
      awParam[0] = 0 ;                 /* disable lockstate */
   }
   else
   {
      awParam[0] = 1 ;                 /* enable lockstate */
   }

   coevse_AddCmdFifo( COEVSE_CMD_SETLOCK, awParam, 1 ) ;
}


/*----------------------------------------------------------------------------*/
/* Set current capacity (A)                                                    */
/*----------------------------------------------------------------------------*/

void coevse_SetCurrentCap( BYTE i_byCurrent )
{
   WORD awParam [1] ;

   awParam[0] = i_byCurrent ;
   coevse_AddCmdFifo( COEVSE_CMD_SETCURRENTCAP, awParam, 1 ) ;
}


/*----------------------------------------------------------------------------*/
/* Get current capacity (A)                                                   */
/*----------------------------------------------------------------------------*/

DWORD coevse_GetCurrentCap( void )
{
   return l_Status.dwCurrentCap ;
}


/*----------------------------------------------------------------------------*/
/* Get EV state                                                               */
/*----------------------------------------------------------------------------*/

e_coevseEvseState coevse_GetEvseState( void )
{
   return l_Status.eEvseState ;
}


/*----------------------------------------------------------------------------*/
/* Ask if plugging event                                                      */
/*----------------------------------------------------------------------------*/

BOOL coevse_IsPlugEvent( BOOL i_bReset )
{
   BOOL bPlugEvent ;

   bPlugEvent = l_Status.bPlugEvent ;

   if ( i_bReset )
   {
      l_Status.bPlugEvent = 0 ;
   }

   return bPlugEvent ;
}


/*----------------------------------------------------------------------------*/
/* Get charge current (measured), mA                                          */
/*----------------------------------------------------------------------------*/

SDWORD coevse_GetCurrent( void )
{
   return l_Status.sdwChargeCurrent ;
}


/*----------------------------------------------------------------------------*/
/* Get charge voltage (measured), mV                                          */
/*----------------------------------------------------------------------------*/

SDWORD coevse_GetVoltage( void )
{
   return l_Status.sdwChargeVoltage ;
}


/*----------------------------------------------------------------------------*/
/* Get charge energy consumption, Wh                                          */
/*----------------------------------------------------------------------------*/

DWORD coevse_GetEnergy( void )
{
   return l_Status.dwCurWh ;
}


/*----------------------------------------------------------------------------*/
/* Get RAPI commands history                                                  */
/*----------------------------------------------------------------------------*/

void coevse_GetHist( CHAR * o_pszHistCmd, WORD i_wSize )
{
   strncpy( o_pszHistCmd, l_HistCmd.szHistStr, i_wSize ) ;
                                       /* clear command history */
   memset( &l_HistCmd, 0, sizeof( l_HistCmd ) ) ;
}



/*----------------------------------------------------------------------------*/
/* Format charge informations                                                 */
/*----------------------------------------------------------------------------*/

void coevse_FmtInfo( CHAR * o_pszInfo, WORD i_wSize )
{
   snprintf( o_pszInfo, i_wSize, "%li, %li, %lu", l_Status.sdwChargeCurrent,
                                                  l_Status.sdwChargeVoltage,
                                                  l_Status.dwCurWh ) ;
}


/*----------------------------------------------------------------------------*/
/* Add external RAPI command (bridge)                                         */
/*----------------------------------------------------------------------------*/

RESULT coevse_AddExtCmd( char C* i_szStrCmd )
{
   RESULT rRet ;
                                       /* allowed only if no other external cmd */
                                       /* in FIFO, and command is valid */
   if ( ( l_szExtCmdStr[0] == 0 ) &&
        ( i_szStrCmd[0] == '$' ) &&
        ( strlen( i_szStrCmd ) < ( sizeof( l_szExtCmdStr ) - 1 ) ) )
   {                                   /* copy into external command string. */
                                       /* The strlen in condition guarantee that */
                                       /* there will be NULL at the end */
      strncpy( l_szExtCmdStr, i_szStrCmd, sizeof( l_szExtCmdStr ) ) ;

      coevse_AddCmdFifo( COEVSE_CMD_EXTCMD, NULL, 0 ) ;
      rRet = OK ;
   }
   else
   {
	   rRet = ERR ;
   }

   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void coevse_TaskCyc( void )
{
   if ( l_bOpenEvseRdy )
   {
      if ( coevse_IsNeedSend() )       /* if sending is ready */
      {
         coevse_SendCmdFifo() ;        /* send next command in FIFO */
         tim_StartMsTmp( &l_dwCmdTimeout ) ;
      }

      if ( l_eCmd != COEVSE_CMD_NONE ) /* if command is still pending */
      {                                /* if response has arrived */
         if ( ! l_Result.bWaitResponse )
         {                             /* treat the response */
            HAL_NVIC_DisableIRQ( UOEVSE_IRQn ) ;
            coevse_AnalyseRes() ;
            HAL_NVIC_EnableIRQ( UOEVSE_IRQn ) ;
         }

         if ( tim_IsEndMsTmp( &l_dwCmdTimeout, COEVSE_TIMEOUT ) )
         {
            coevse_CmdSetErr() ;       /* timeout error */
         }
      }
                                       /* verification of charging metrics temporisation */
      if ( tim_IsEndMsTmp( &l_dwGetStateTmp, COEVSE_GETSTATE_PER ) )
      {
         tim_StartMsTmp( &l_dwGetStateTmp ) ;

         coevse_AddCmdFifo( COEVSE_CMD_GETEVSESTATE, NULL, 0 ) ;
         coevse_AddCmdFifo( COEVSE_CMD_GETCURRENTCAP, NULL, 0 ) ;
         coevse_AddCmdFifo( COEVSE_CMD_GETFAULT, NULL, 0 ) ;
         coevse_AddCmdFifo( COEVSE_CMD_GETCHARGPARAM, NULL, 0 ) ;
         coevse_AddCmdFifo( COEVSE_CMD_GETENERGYCNT, NULL, 0 ) ;
      }
   }
   else
   {
      if ( tim_IsEndMsTmp( &l_dwTmpStart, COEVSE_HRD_START_DUR ) )
      {
         l_bOpenEvseRdy = TRUE ;       /* openEVSE hardware is ready */
                                       /* get version once openEVSE is ready */
         coevse_AddCmdFifo( COEVSE_CMD_GETVERSION, NULL, 0 ) ;
         tim_StartMsTmp( &l_dwGetStateTmp ) ;
      }
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Test if sending is needed                                                  */
/*----------------------------------------------------------------------------*/

static BOOL coevse_IsNeedSend( void )
{
   BOOL bNeedSend ;

   bNeedSend = ( l_CmdFifo.byIdxIn != l_CmdFifo.byIdxOut ) &&
               ( l_eCmd == COEVSE_CMD_NONE ) ;

   return bNeedSend ;
}


/*----------------------------------------------------------------------------*/
/* Add command in FIFO                                                        */
/*----------------------------------------------------------------------------*/

static void coevse_AddCmdFifo( e_CmdId i_eCmdId, WORD * i_awParams, BYTE i_byNbParam )
{
   BYTE byCurIdxIn ;
   BYTE byNextIdxIn ;
   s_CmdFifoData * pCmdData ;
   BYTE byIdx ;

   byCurIdxIn = l_CmdFifo.byIdxIn ;
                                       /* caculate next input index */
   byNextIdxIn = NEXTIDX( byCurIdxIn, l_CmdFifo.aCmdData ) ;
                                       /* if the FIFO overflows */
   if ( byNextIdxIn != l_CmdFifo.byIdxOut )
   {
      l_CmdFifo.byIdxIn = byNextIdxIn ;
                                       /* fill the new element */
      pCmdData = &l_CmdFifo.aCmdData[byCurIdxIn] ;
      pCmdData->eCmdId = i_eCmdId ;
      pCmdData->byNbParam = i_byNbParam ;

      if ( i_awParams != NULL )        /* fill parameters if needed */
      {
         for ( byIdx = 0 ; byIdx < i_byNbParam ; byIdx++ )
         {                             /* if param buffer overflows */
            if ( byIdx >= ARRAY_SIZE( pCmdData->wParam ) )
            {
               break ;
            }
            pCmdData->wParam[byIdx] = i_awParams[byIdx] ;
         }
      }
   }
   else
   {
      err_Set( ERR_OEVSE_COM_BUF_FULL ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Send next command in FIFO                                                  */
/*----------------------------------------------------------------------------*/

static void coevse_SendCmdFifo( void )
{
   BYTE byIdxOut ;
   s_CmdFifoData * pFifoData ;
   e_CmdId eCmd ;
   WORD * awPar ;
   BYTE byCmdIdx ;
   char C* pszFmtCmd ;
   char * pszStrCmd ;
   char szChecksum [3] ;
   s_CmdDesc C* pCmdDesc ;
   BYTE byStrRemSize ;
   BYTE byFmtSize ;
   WORD wExtCmdLen ;

   byIdxOut = l_CmdFifo.byIdxOut ;

   pFifoData = &l_CmdFifo.aCmdData[byIdxOut] ;
                                       /* caculate next output index */
   byIdxOut = NEXTIDX( byIdxOut, l_CmdFifo.aCmdData ) ;

                                       /* remaining size of command string */
   byStrRemSize = sizeof(l_szStrCmdBuffer) ;

   eCmd = pFifoData->eCmdId ;          /* get command descriptor */
   byCmdIdx = eCmd - ( COEVSE_CMD_NONE + 1 ) ;
   pCmdDesc = &k_aCmdDesc[byCmdIdx] ;

   if ( eCmd == COEVSE_CMD_EXTCMD )    /* in cas of external command */
   {                                   /* copy from external command string */
                                       /* NULL terminasion is garanteed since the 2 buffers */
                                       /* share the same size and l_szExtCmdStr is NULL terminated*/
      pszStrCmd = strncpy( l_szStrCmdBuffer, l_szExtCmdStr, sizeof(l_szStrCmdBuffer) ) ;
      wExtCmdLen = strlen( l_szStrCmdBuffer ) ;
                                       /* new end of string pnt */
      pszStrCmd = &l_szStrCmdBuffer[wExtCmdLen] ;
      byStrRemSize -= wExtCmdLen ;     /* new remaining size */

      l_szExtCmdStr[0] = 0 ;           /* external cmd string is free */
   }
   else
   {
      awPar = &pFifoData->wParam[0] ;

      pszFmtCmd = pCmdDesc->szFmtCmd ;
                                       /* format command with parameters */
      byFmtSize = snprintf( l_szStrCmdBuffer, byStrRemSize, pszFmtCmd,
                            awPar[0], awPar[1], awPar[2], awPar[3], awPar[4], awPar[5] ) ;
                                       /* new end of string pnt */
      pszStrCmd = &l_szStrCmdBuffer[byFmtSize] ;
      byStrRemSize -= byFmtSize ;      /* new remaining size */
   }

   if ( byStrRemSize >= 4 )            /* if size left for checksum */
   {
      if ( pCmdDesc->szChecksum == NULL )
      {                                /* compute checksum and add it at the end of the string */
         coevse_GetChecksum( szChecksum, sizeof(szChecksum), l_szStrCmdBuffer ) ;
         *pszStrCmd = '^' ;
         pszStrCmd++ ;
         *pszStrCmd = szChecksum[0] ;
         pszStrCmd++ ;
         *pszStrCmd = szChecksum[1] ;
         pszStrCmd++ ;
         *pszStrCmd = '\r' ;
         pszStrCmd++ ;
         *pszStrCmd = '\0' ;
      }
      else
      {                                /* add static checksum */
         strlcpy( pszStrCmd, pCmdDesc->szChecksum, byStrRemSize ) ;
      }
   }

   coevse_CmdStart( eCmd ) ;           /* start command transmission */
   coevse_HrdSendCmd( l_szStrCmdBuffer, strlen( l_szStrCmdBuffer )  ) ;

   coevse_HistAddCmd( l_szStrCmdBuffer ) ;
}


/*----------------------------------------------------------------------------*/
/* Analyse response                                                           */
/*----------------------------------------------------------------------------*/

static void coevse_AnalyseRes( void )
{
   RESULT rRes ;
   char * szResult ;
   BYTE byResSize ;
   BYTE byCmdIdx ;
   f_ResultCallback pFunc ;
   char szChecksum [3] ;
   char * pszDataRes ;

   rRes = OK ;

   if ( rRes == OK )                   /* verify checksum */
   {
      szResult = (char*) l_Result.abyDataRes ;
      byResSize = l_Result.byResIdx ;

      coevse_GetChecksum( szChecksum, sizeof(szChecksum), szResult ) ;

      if ( ( szResult[ ( byResSize - 3 ) ] != szChecksum[0] ) ||
           ( szResult[ ( byResSize - 2 ) ] != szChecksum[1] ) )
      {
         rRes = ERR ;
      }
   }

   if ( rRes == OK )                   /* verify command OK/KO response */
   {
      if ( l_eCmd != COEVSE_CMD_EXTCMD )
      {
         szResult[byResSize-4] = '\0' ;

         if ( ( szResult[0] != '$' ) || ( szResult[1] != 'O' ) ||
              ( szResult[2] != 'K' ) )
         {
            rRes = ERR ;
         }
      }
   }

   if ( rRes == OK )                   /* call callback if defined */
   {
      byCmdIdx = l_eCmd - ( COEVSE_CMD_NONE + 1 ) ;
      pFunc = k_aCmdDesc[byCmdIdx].fResultCallback ;

      if ( l_eCmd == COEVSE_CMD_EXTCMD )
      {
         pszDataRes = &szResult[0] ;   /* in case of external we need the whole response */
      }
      else
      {
         pszDataRes = &szResult[3] ;   /* in other cases, only the data after "OK " is needed */
      }

      if ( pFunc != NULL )
      {
         (*pFunc)( pszDataRes ) ;
      }
                                       /* save next output FIFO index*/
      l_CmdFifo.byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aCmdData ) ;
      l_byNbRetry = 0 ;
      coevse_CmdEnd() ;
   }
   else
   {
      coevse_CmdSetErr() ;             /* there is an error */
   }
}


/*----------------------------------------------------------------------------*/
/* Compute checksum and store it in string                                    */
/*----------------------------------------------------------------------------*/

static void coevse_GetChecksum( char * o_sChecksum, BYTE i_byStrSize,
                                char C* i_szData )
{
   BYTE byXor ;
   char C* pszData ;
                                       /* if string is null or too small */
   if ( ( o_sChecksum == NULL ) || ( i_byStrSize < 3 ) )
   {
      ERR_FATAL() ;
   }

   pszData = i_szData ;
   byXor = 0 ;

   while( ( *pszData != '^' ) && ( *pszData != 0 ) )
   {
      byXor ^= (BYTE)*pszData ;
      pszData++ ;
   }

   snprintf( o_sChecksum, i_byStrSize, "%02X", byXor ) ;
}


/*----------------------------------------------------------------------------*/
/* Start command sending                                                      */
/*----------------------------------------------------------------------------*/

static void coevse_CmdStart( e_CmdId i_eCmdId )
{
   HAL_NVIC_DisableIRQ( UOEVSE_IRQn ) ;

   memset( l_Result.abyDataRes, 0, sizeof(l_Result.abyDataRes) ) ;
   l_Result.byResIdx = 0 ;
   l_Result.bError = FALSE ;
   l_Result.bWaitResponse = TRUE ;
   l_eCmd = i_eCmdId ;

   HAL_NVIC_EnableIRQ( UOEVSE_IRQn ) ;
}


/*----------------------------------------------------------------------------*/
/* error treatment                                                            */
/*----------------------------------------------------------------------------*/

static void coevse_CmdSetErr( void )
{
   if ( l_byNbRetry >= COEVSE_MAXRETRY )
   {
      coevse_SetError() ;
   }
   else
   {
      l_byNbRetry++ ;
   }

   coevse_CmdEnd() ;                /* stop the sending to retry an other one */

   /* note : the output index of command FIFO is incremented only if the     */
   /* response valid (see coevse_AnalyseRes() ). So at this point the output */
   /* index remain the same, and the next retry send the same command        */
}


/*----------------------------------------------------------------------------*/
/* Hardware error processing                                                  */
/*----------------------------------------------------------------------------*/

static void coevse_SetError( void )
{
                                       /* send reset command */
   coevse_HrdSendCmd( k_szStrReset, sizeof(k_szStrReset) ) ;

   l_eCmd = COEVSE_CMD_NONE ;          /* initialize commands variables */
   memset( l_szExtCmdStr, 0, sizeof(l_szExtCmdStr) ) ;
   memset( l_szStrCmdBuffer, 0, sizeof(l_szStrCmdBuffer) ) ;
   memset( &l_CmdFifo, 0, sizeof(l_CmdFifo) ) ;

   l_byNbRetry = 0 ;
   l_dwCmdTimeout = 0 ;
   l_dwGetStateTmp = 0 ;
   l_bOpenEvseRdy = 0 ;

   l_bOpenEvseRdy = FALSE ;
   tim_StartMsTmp( &l_dwTmpStart ) ;

   err_Set( ERR_OEVSE_COM ) ;
}


/*----------------------------------------------------------------------------*/
/* End command sending                                                        */
/*----------------------------------------------------------------------------*/

static void coevse_CmdEnd( void )
{
   HAL_NVIC_DisableIRQ( UOEVSE_IRQn ) ;

   memset( l_Result.abyDataRes, 0, sizeof(l_Result.abyDataRes) ) ;
   l_Result.byResIdx = 0 ;
   l_Result.bError = FALSE ;
   l_Result.bWaitResponse = FALSE ;
   l_eCmd = COEVSE_CMD_NONE ;

   HAL_NVIC_EnableIRQ( UOEVSE_IRQn ) ;
}


/*----------------------------------------------------------------------------*/
/* Add new command to historic                                                */
/*----------------------------------------------------------------------------*/

static void coevse_HistAddCmd( char C* i_pszStrCmd )
{
   char * pszHistbuffer ;
   SWORD swTotalSizeLeft ;
   BYTE byStrCmdSize ;

   if ( i_pszStrCmd[1] == 'S' )           /* only Sx RAPI commands are stored */
   {
      byStrCmdSize = strlen( i_pszStrCmd ) - 4 ;

      swTotalSizeLeft = ( sizeof( l_HistCmd.szHistStr ) ) - l_HistCmd.wIdx ;
                                          /* if sapce left */
      if ( ( swTotalSizeLeft > 0 ) && ( ( byStrCmdSize + 1 ) <= swTotalSizeLeft ) )
      {                                   /* pointer to free hist character */
         pszHistbuffer = &l_HistCmd.szHistStr[l_HistCmd.wIdx] ;
                                          /* copy commmand string */
         strlcpy( pszHistbuffer, i_pszStrCmd, byStrCmdSize + 1 ) ;

         pszHistbuffer += byStrCmdSize ;
         *pszHistbuffer = '\n' ;          /* add '\n' at the end */

         pszHistbuffer += 1 ;
         *pszHistbuffer = '\0' ;

         l_HistCmd.wIdx += ( byStrCmdSize + 1 ) ;
      }
   }
}



/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETEVSESTATE command callback (update EVSE state)               */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetEVSEState( char C* i_pszDataRes )
{
   if ( i_pszDataRes[1] == '1' )
   {
      l_Status.eEvseState = COEVSE_STATE_NOTCONNECTED ;
   }
   else if ( i_pszDataRes[1] == '2' )
   {
      if ( l_Status.eEvseState == COEVSE_STATE_NOTCONNECTED )
      {
         l_Status.bPlugEvent = TRUE ;
      }

      l_Status.eEvseState = COEVSE_STATE_CONNECTED ;
   }
   else if ( i_pszDataRes[1] == '3' )
   {
      l_Status.eEvseState = COEVSE_STATE_CHARGING ;
   }
   else if ( i_pszDataRes[1] == '4' )
   {
      l_Status.eEvseState = COEVSE_STATE_CHARGING ;
   }
   else
   {
      l_Status.eEvseState = COEVSE_STATE_UNKNOWN;
   }
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETCURRENTCAP command callback (read current cap)               */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetCurrentCap( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
                                       /* convert next digit to integer */
   pszNext = cascii_GetNextDec( pszStr, &sdwValue, FALSE, DWORD_MAX ) ;

   if ( pszStr != pszNext )            /* if conversion succeded */
   {
      pszStr = pszNext ;               /* store current cap */
      l_Status.dwCurrentCap = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETFAULT command callback (read faults counter)                 */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetFault( char C* i_pszDataRes )
{
   DWORD dwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
                                          /* convert next hexa to integer */
   pszNext = cascii_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwErrGfiTripCnt = dwValue ;
   }
                                       /* convert next hexa to integer */
   pszNext = cascii_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwNoGndTripCnt = dwValue ;
   }
                                       /* convert next hexa to integer */
   pszNext = cascii_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwStuckRelayTripCnt = dwValue ;
   }
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETCHARGPARAM command callback (read charge param)              */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetChargParam( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
                                       /* convert next digit to integer */
   pszNext = cascii_GetNextDec( pszStr, &sdwValue, TRUE, SDWORD_MAX ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwChargeCurrent = sdwValue ;
   }
                                       /* convert next digit to integer */
   pszNext = cascii_GetNextDec( pszStr, &sdwValue, TRUE, SDWORD_MAX ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwChargeVoltage = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETENERGYCNT command callback                                   */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetEneryCnt( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
                                       /* convert next digit to integer */
   pszNext = cascii_GetNextDec( pszStr, &sdwValue, FALSE, DWORD_MAX ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwCurWh = ( sdwValue / ( 60 * 60 ) ) ;
   }
                                       /* convert next digit to integer */
   pszNext = cascii_GetNextDec( pszStr, &sdwValue, FALSE, DWORD_MAX ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwAccWh = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_GETVERSION command callback                                     */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetVersion( char C* i_pszDataRes )
{
   USEPARAM( i_pszDataRes ) //TODO: save version
}


/*----------------------------------------------------------------------------*/
/* COEVSE_CMD_EXTCMD command callback                                         */
/*----------------------------------------------------------------------------*/

static void coevse_CmdresultExtCmd( char C* i_pszDataRes )
{
   if ( l_fPostResProc != NULL )
   {                                   /* post to ScktFrame module */
      (*l_fPostResProc)( i_pszDataRes, TRUE ) ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Hardware initialization                                                    */
/*----------------------------------------------------------------------------*/

static void coevse_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

      /* configure TX pin as alternate, no push/pull, high freq */
   sGpioInit.Pin = OESVE_TX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = OESVE_TX_AF ;
   HAL_GPIO_Init( OESVE_TX_GPIO, &sGpioInit ) ;

      /* configure RX pin as alternate, no push/pull, high freq */
   sGpioInit.Pin = OESVE_RX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = OESVE_RX_AF ;
   HAL_GPIO_Init( OESVE_RX_GPIO, &sGpioInit ) ;


   /* -------- USART -------- */

   UOEVSE_CLK_ENABLE() ;                /* enable USART clock */

   UOEVSE_FORCE_RESET() ;               /* reset USART  */
   UOEVSE_RELEASE_RESET() ;
                                         /* set baudrate */
   UOEVSE->BRR = UART_DIV_LPUART( APB1_CLK, COEVSE_BAUDRATE ) ;

   UOEVSE->CR3 = USART_CR3_DMAT ;

   UOEVSE->RDR ;                       /* read input register to avoid unwanted data */

   UOEVSE->ICR |= 0xFFFFFFFF ;         /* reset interrupt */
                                       /* activate emission */
   UOEVSE->CR1 = USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_RE | USART_CR1_TE  ;

                                       /* set USART interrupt priority level */
   HAL_NVIC_SetPriority( UOEVSE_IRQn, UOEVSE_IRQPri, 0 ) ;
   HAL_NVIC_EnableIRQ( UOEVSE_IRQn ) ;  /* enable USART interrupt */

   /* -------- DMA -------- */

   UOEVSE_DMA_CLK_ENABLE() ;           /* enable DMA clock */
                                       /* set direction mem-to-perif, memory increment, */
                                       /* enable transfer complete and error interrupt */
   UOEVSE_DMA_TX->CCR = DMA_CCR_DIR | DMA_CCR_MINC ;
                                       /* set periferal address (USART TDR register) */
   UOEVSE_DMA_TX->CPAR = (DWORD)&(UOEVSE->TDR) ;
                                       /* set TX request source */
   UOEVSE_DMA_CSELR->CSELR &= ~UOEVSE_DMA_TX_CSELR( DMA_CSELR_C1S_Msk ) ;
   UOEVSE_DMA_CSELR->CSELR |= UOEVSE_DMA_TX_CSELR( UOEVSE_DMA_TX_REQ ) ;
}


/*----------------------------------------------------------------------------*/

static void coevse_HrdSendCmd( char C* i_pszStrCmd, BYTE i_bySize )
{
   COEVSE_DISABLE_DMA_TX() ;           /* stop previous DMA transfer */

   UOEVSE_DMA_TX->CNDTR = i_bySize ;
                                       /* set periferal address (USART TDR register) */
   UOEVSE_DMA_TX->CPAR = (DWORD)&(UOEVSE->TDR) ;
                                       /* set memory address (transmit buffer) */
   UOEVSE_DMA_TX->CMAR = (DWORD)i_pszStrCmd ;

   COEVSE_ENABLE_DMA_TX() ;
}


/*----------------------------------------------------------------------------*/
/* Open EVSE USART interrupt                                                  */
/*----------------------------------------------------------------------------*/

void UOEVSE_IRQHandler( void )
{
   BYTE byData ;
   BYTE byResIdx ;

   byData = UOEVSE->RDR ;

   byResIdx = l_Result.byResIdx ;
                                       /* check error presence */
   if ( ( ISSET( UOEVSE->ISR, USART_ISR_ORE ) ) ||
        ( byResIdx > sizeof(l_Result.abyDataRes) - 1 ) )
   {
	   UOEVSE->ICR |= USART_ICR_ORECF ;
      l_Result.bError = TRUE ;
   }

   if ( ! l_Result.bError )
   {
      if ( l_Result.bWaitResponse )
      {
         l_Result.abyDataRes[byResIdx] = byData ;
         l_Result.byResIdx = byResIdx + 1 ;

         if ( byData == '\r' )         /* check the end of response */
         {
            l_Result.bWaitResponse = FALSE ;
         }
      }
      else
      {
         //data outside commmand transfer is not used for now

         l_Async.abyDataRes[l_Async.byResIdx] = byData ;
         l_Async.byResIdx = ( l_Async.byResIdx + 1 ) % ARRAY_SIZE(l_Async.abyDataRes) ;
      }
   }
}
