/******************************************************************************/
/*                                                                            */
/*                                 CommOEvse.c                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   04 oct. 2018   Sylvain BASSET        Version 0.1             */
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

#define COEVSE_BAUDRATE     115200llu  /* baudrate (bits per second) value  */

#define COEVSE_TIMEOUT           100   /* communication timeout, ms */
#define COEVSE_GETSTATE_PER     1000   /* OpenEVSE status read period */
#define COEVSE_HRD_START_DUR    8000   /* OpenEVSE hardware startup duration */
#define COEVSE_MAXRETRY            4   /* maximum number of retry before error */

                                       /* disable/suspend transmit channel DMA */
#define COEVSE_DISABLE_DMA_TX()     ( UOEVSE_DMA_TX->CCR &= ~DMA_CCR_EN )
                                       /* enable transmit channel DMA */
#define COEVSE_ENABLE_DMA_TX()      ( UOEVSE_DMA_TX->CCR |= DMA_CCR_EN )


static char const k_szStrReset [] = "$FR^30\r" ;

typedef void (*f_ResultCallback)( char C* i_pszDataRes ) ;

//SBA: mettre les checksum dans le tableau
#define LIST_CMD( Op, Opg ) \
   Op(   ENABLE,        Enable,        "$FE",    "^27\r" ) \
   Op(   DISABLE,       Disable,       "$FD",    "^26\r" ) \
   Op(   SETCURRENTCAP, SetCurrentCap, "$SC %d", NULL )    \
   Opg(  ISEVCONNECT,   IsEvConnect,   "$G0",    "^53\r" ) \
   Opg(  GETCURRENTCAP, GetCurrentCap, "$GC",    "^20\r" ) \
   Opg(  GETFAULT,      GetFault,      "$GF",    "^25\r" ) \
   Opg(  GETCHARGPARAM, GetChargParam, "$GG",    "^24\r" ) \
   Opg(  GETENERGYCNT,  GetEneryCnt,   "$GU",    "^36\r" ) \
   Opg(  GETVERSION,    GetVersion,    "$GV",    "^35\r" ) \

typedef enum
{
   COEVSE_CMD_NONE = 0,
   LIST_CMD( COEVSE_CMD_ENUM, COEVSE_CMD_ENUM )
   COEVSE_CMD_LAST,
} e_CmdId ;

LIST_CMD( COEVSE_CMD_NULL, COEVSE_CMD_CALLBACK )

typedef struct
{
   e_CmdId eCmdId ;
   f_ResultCallback fResultCallback ;
   char C* szFmtCmd ;
   char C* szChecksum ;

} s_CmdDesc ;

static s_CmdDesc const k_aCmdDesc [] =
{
   LIST_CMD( COEVSE_CMD_DESC, COEVSE_CMD_DESC_G )
} ;

typedef struct
{
   e_CmdId eCmdId ;
   WORD wParam [6] ;
   BYTE byNbParam ;
} s_CmdFifoData ;

typedef struct
{
   BYTE byIdxIn ;
   BYTE byIdxOut ;
   s_CmdFifoData aCmdData [8] ;
} s_CmdFifo ;


typedef struct
{
   BYTE abyDataRes [32+1] ;
   BYTE byResIdx ;
   BOOL bError ;
   BOOL bWaitResponse ;
} s_coevseResult ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static BOOL coevse_IsNeedSend( void ) ;
static void coevse_AddCmdFifo( e_CmdId i_eCmdId, WORD * i_awParams, BYTE i_byNbParam ) ;
static void coevse_SendCmdFifo( void ) ;
static void coevse_AnalyseRes( void ) ;

static void coevse_GetChecksum( char * o_sChecksum, BYTE i_byCkSize,
                                char C* i_szData ) ;
static void coevse_CmdSetErr( void ) ;
static void coevse_CmdStart( e_CmdId i_eCmdId ) ;
static void coevse_CmdEnd( void ) ;

static char C* coevse_GetNextDec( char C* i_pszStr, SDWORD *o_psdwValue ) ;
static char C* coevse_GetNextHex( char C* i_pszStr, DWORD *o_pdwValue ) ;

static void coevse_HrdInit( void ) ;
static void coevse_HrdSendCmd( char C* i_pszStrCmd, BYTE i_bySize ) ;


/*----------------------------------------------------------------------------*/
/* variables                                                                  */
/*----------------------------------------------------------------------------*/

static e_CmdId l_eCmd ;
static s_CmdFifo l_CmdFifo ;
static char l_szStrCmdBuffer [64] ;

static BYTE l_byNbRetry ;
static DWORD l_dwCmdTimeout ;
static BOOL l_bHardStarted ;
static DWORD l_dwTmpStart ;

static s_coevseResult l_Result ;
static s_coevseResult l_Async ;

static DWORD l_dwGetStateTmp ;
static s_coevseStatus l_Status ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void coevse_Init( void )
{
   coevse_HrdInit() ;

   coevse_HrdSendCmd( k_szStrReset, sizeof(k_szStrReset) ) ;
   l_bHardStarted = FALSE ;

   tim_StartMsTmp( &l_dwTmpStart ) ;
}


/*----------------------------------------------------------------------------*/
/* Set current capacity                                                       */
/*----------------------------------------------------------------------------*/

void coevse_SetEnable( BOOL i_bEnable )
{
   if ( i_bEnable )
   {
      coevse_AddCmdFifo( COEVSE_CMD_ENABLE, NULL, 0 ) ;
   }
   else
   {
      coevse_AddCmdFifo( COEVSE_CMD_DISABLE, NULL, 0 ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Set current capacity                                                       */
/*----------------------------------------------------------------------------*/

void coevse_SetCurrentCap( WORD i_wCurrent )
{
   WORD awParam [1] ;

   awParam[0] = i_wCurrent ;
   coevse_AddCmdFifo( COEVSE_CMD_SETCURRENTCAP, awParam, 1 ) ;
}


/*----------------------------------------------------------------------------*/
/* Get if EV is plugged                                                       */
/*----------------------------------------------------------------------------*/

BOOL coevse_IsPlugged( void )
{
   return l_Status.bEvConnect ;
}


/*----------------------------------------------------------------------------*/
/* Get if EV charging                                                        */
/*----------------------------------------------------------------------------*/

BOOL coevse_IsCharging( void )
{
   BOOL bRet ;

   if ( l_Status.sdwChargeCurrent > 0 )
   {
      bRet = TRUE ;
   }
   else
   {
      bRet = FALSE ;
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Get OpenEVSE status                                                        */
/*----------------------------------------------------------------------------*/

s_coevseStatus coevse_GetStatus( void )
{
   return l_Status ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void coevse_TaskCyc( void )
{
   if ( l_bHardStarted )
   {
      if ( coevse_IsNeedSend() )
      {
         coevse_SendCmdFifo() ;
         tim_StartMsTmp( &l_dwCmdTimeout ) ;
      }

      if ( l_eCmd != COEVSE_CMD_NONE )
      {
         if ( ! l_Result.bWaitResponse )
         {
            HAL_NVIC_DisableIRQ( UOEVSE_IRQn ) ;
            coevse_AnalyseRes() ;
            HAL_NVIC_EnableIRQ( UOEVSE_IRQn ) ;
         }

         if ( tim_IsEndMsTmp( &l_dwCmdTimeout, COEVSE_TIMEOUT ) )
         {
            coevse_CmdSetErr() ;
         }
      }

      if ( l_bHardStarted && tim_IsEndMsTmp( &l_dwGetStateTmp, COEVSE_GETSTATE_PER ) )
      {
         tim_StartMsTmp( &l_dwGetStateTmp ) ;

         coevse_AddCmdFifo( COEVSE_CMD_ISEVCONNECT, NULL, 0 ) ;
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
         l_bHardStarted = TRUE ;
                                       /* get version once openEVSE is ready */
         coevse_AddCmdFifo( COEVSE_CMD_GETVERSION, NULL, 0 ) ;
         tim_StartMsTmp( &l_dwGetStateTmp ) ;
      }
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static BOOL coevse_IsNeedSend( void )
{
   BOOL bNeedSend ;

   bNeedSend = ( l_CmdFifo.byIdxIn != l_CmdFifo.byIdxOut ) &&
               ( l_eCmd == COEVSE_CMD_NONE ) ;

   return bNeedSend ;
}


/*----------------------------------------------------------------------------*/

static void coevse_AddCmdFifo( e_CmdId i_eCmdId, WORD * i_awParams, BYTE i_byNbParam )
{
   BYTE byCurIdxIn ;
   BYTE byNextIdxIn ;
   s_CmdFifoData * pCmdData ;
   BYTE byIdx ;

   byCurIdxIn = l_CmdFifo.byIdxIn ;

   byNextIdxIn = NEXTIDX( byCurIdxIn, l_CmdFifo.aCmdData ) ;
   l_CmdFifo.byIdxIn = byNextIdxIn ;

   if ( byNextIdxIn == l_CmdFifo.byIdxOut )
   {                                   // last element is lost
      //l_CmdFifo.byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aDataItems ) ;
      ERR_FATAL() ;
   }

   pCmdData = &l_CmdFifo.aCmdData[byCurIdxIn] ;
   pCmdData->eCmdId = i_eCmdId ;
   pCmdData->byNbParam = i_byNbParam ;
   for ( byIdx = 0 ; byIdx < i_byNbParam ; byIdx++ )
   {
      pCmdData->wParam[byIdx] = i_awParams[byIdx] ;
   }
}


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


   byIdxOut = l_CmdFifo.byIdxOut ;

   pFifoData = &l_CmdFifo.aCmdData[byIdxOut] ;
   byIdxOut = NEXTIDX( byIdxOut, l_CmdFifo.aCmdData ) ;

   eCmd = pFifoData->eCmdId ;

   awPar = &pFifoData->wParam[0] ;

   byCmdIdx = eCmd - ( COEVSE_CMD_NONE + 1 ) ;

   pCmdDesc = &k_aCmdDesc[byCmdIdx] ;
   pszFmtCmd = pCmdDesc->szFmtCmd ;

   byStrRemSize = sizeof(l_szStrCmdBuffer) ;
   byFmtSize = snprintf( l_szStrCmdBuffer, byStrRemSize, pszFmtCmd,
                        awPar[0], awPar[1], awPar[2], awPar[3], awPar[4], awPar[5] ) ;
   pszStrCmd = &l_szStrCmdBuffer[byFmtSize] ;
   byStrRemSize -= byFmtSize ;

   if ( byStrRemSize < 4 )
   {
      ERR_FATAL() ;
   }

   if ( pCmdDesc->szChecksum == NULL )
   {
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
   {
      strncpy( pszStrCmd, pCmdDesc->szChecksum, byStrRemSize ) ;
   }

   coevse_CmdStart( eCmd ) ;
   coevse_HrdSendCmd( l_szStrCmdBuffer, strlen( l_szStrCmdBuffer )  ) ;
}


/*----------------------------------------------------------------------------*/

static void coevse_AnalyseRes( void )
{
   RESULT rRes ;
   char * szResult ;
   BYTE byResSize ;
   BYTE byCmdIdx ;
   f_ResultCallback pFunc ;
   char szChecksum [3] ;

   rRes = OK ;

   if ( rRes == OK )
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

   if ( rRes == OK )
   {
      szResult[byResSize-4] = '\0' ;

      if ( ( szResult[0] != '$' ) || ( szResult[1] != 'O' ) ||
           ( szResult[2] != 'K' ) )
      {
         rRes = ERR ;
      }
   }

   if ( rRes == OK )
   {
      byCmdIdx = l_eCmd - ( COEVSE_CMD_NONE + 1 ) ;
      pFunc = k_aCmdDesc[byCmdIdx].fResultCallback ;

      if ( pFunc != NULL )
      {
         (*pFunc)( &szResult[3] ) ;
      }

      l_CmdFifo.byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aCmdData ) ;
      l_byNbRetry = 0 ;
      coevse_CmdEnd() ;
   }
   else
   {
      coevse_CmdSetErr() ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_GetChecksum( char * o_sChecksum, BYTE i_bySizeMax,
                                char C* i_szData )
{
   BYTE byXor ;
   char C* pszData ;

   if ( ( o_sChecksum == NULL ) || ( i_bySizeMax < 3 ) )
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

   snprintf( o_sChecksum, i_bySizeMax, "%02X", byXor ) ;
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdSetErr( void )
{
   if ( l_byNbRetry >= COEVSE_MAXRETRY )
   {
      ERR_FATAL() ;
   }
   l_byNbRetry++ ;

   coevse_CmdEnd() ;
}


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


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void coevse_CmdresultIsEvConnect( char C* i_pszDataRes )
{
   if ( i_pszDataRes[1] == '1' )
   {
      l_Status.bEvConnect = TRUE ;
   }
   else
   {
      l_Status.bEvConnect = FALSE ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetCurrentCap( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwCurrentCapMin = sdwValue ;
   }

   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwCurrentCapMax = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetChargParam( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwChargeVoltage = sdwValue ;
   }

   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwChargeCurrent = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetFault( char C* i_pszDataRes )
{
   DWORD dwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
   pszNext = coevse_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwErrGfiTripCnt = dwValue ;
   }

   pszNext = coevse_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwNoGndTripCnt = dwValue ;
   }

   pszNext = coevse_GetNextHex( pszStr, &dwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.dwStuckRelayTripCnt = dwValue ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetEneryCnt( char C* i_pszDataRes )
{
   SDWORD sdwValue ;
   CHAR C* pszStr ;
   CHAR C* pszNext ;

   pszStr = i_pszDataRes ;
   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwCurWh = sdwValue ;
   }

   pszNext = coevse_GetNextDec( pszStr, &sdwValue ) ;
   if ( pszStr != pszNext )
   {
      pszStr = pszNext ;
      l_Status.sdwAccWh = sdwValue ;
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_CmdresultGetVersion( char C* i_pszDataRes )
{
   REFPARM( i_pszDataRes )
}


/*----------------------------------------------------------------------------*/

static char C* coevse_GetNextDec( char C* i_pszStr, SDWORD *o_psdwValue )
{
   BOOL bEndOfString ;
   BOOL bNeg ;
   DWORD dwValue ;
   char C* pszStr ;
   char C* pszEnd ;

   bEndOfString = FALSE ;
   bNeg = FALSE ;
   dwValue = 0 ;
   pszStr = i_pszStr ;
   pszEnd = i_pszStr ;

   while( ! ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) ) )
   {
      if ( ( *pszStr == '\0' ) || ( *pszStr == '^' ) )
      {
         bEndOfString = TRUE ;
         break ;
      }
      if ( *pszStr == '-' )
      {
         bNeg ^= TRUE ;
      }
      pszStr++ ;
   }

   if ( ! bEndOfString )
   {
      while( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) )
      {
         dwValue *= 10 ;
         dwValue += ( *pszStr - '0' ) ;
         pszStr++ ;
      }
      pszEnd = pszStr ;
   }

   if ( o_psdwValue != NULL )
   {
      if ( bNeg )
      {
         *o_psdwValue = -(SDWORD)dwValue ;
      }
      else
      {
         *o_psdwValue = (SDWORD)dwValue ;
      }
   }

   return pszEnd ;
}


/*----------------------------------------------------------------------------*/

static char C* coevse_GetNextHex( char C* i_pszStr, DWORD *o_pdwValue )
{
   BOOL bEndOfString ;
   DWORD dwValue ;
   char C* pszStr ;
   char C* pszEnd ;

   bEndOfString = FALSE ;
   dwValue = 0 ;
   pszStr = i_pszStr ;
   pszEnd = i_pszStr ;

   while( ! ( ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) ) ||
              ( ( *pszStr >= 'A' ) && ( *pszStr <= 'F' ) ) ) )
   {
      if ( ( *pszStr == '\0' ) || ( *pszStr == '^' ) )
      {
         bEndOfString = TRUE ;
         break ;
      }
      pszStr++ ;
   }

   if ( ! bEndOfString )
   {
   while( ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) ) ||
          ( ( *pszStr >= 'A' ) && ( *pszStr <= 'F' ) ) )
      {
         dwValue *= 16 ;
         if ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) )
         {
            dwValue += ( *pszStr - '0' ) ;
         }
         else
         {
            dwValue += ( *pszStr - 'A' ) + 10 ;
         }
         pszStr++ ;
      }
      pszEnd = pszStr ;
   }

   if ( o_pdwValue != NULL )
   {
      *o_pdwValue = dwValue ;
   }

   return pszEnd ;
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

         if ( byData == '\r' )
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
