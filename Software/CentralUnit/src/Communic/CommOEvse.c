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

#define COEVSE_BAUDRATE     115200llu   /* baudrate (bits per second) value  */

                                       /* disable/suspend transmit channel DMA */
#define COEVSE_DISABLE_DMA_TX()     ( UOEVSE_DMA_TX->CCR &= ~DMA_CCR_EN )
                                       /* enable transmit channel DMA */
#define COEVSE_ENABLE_DMA_TX()      ( UOEVSE_DMA_TX->CCR |= DMA_CCR_EN )


typedef RESULT (*f_ResultCallback)( char C* i_pszDataRes ) ;


#define LIST_CMD( Op, Opg ) \
   Op(   ENABLE,        Enable,        "$FE\r"    ) \
   Op(   DISABLE,       Disable,       "$FD\r"    ) \
   Op(   SETCURRENTCAP, SetCurrentCap, "$SC %d\r" ) \
   Opg(  ISEVCONNECT,   IsEvConnect,   "$G0\r"    ) \
   Opg(  GETCURRENTCAP, GetCurrentCap, "$GC\r"    ) \
   Opg(  GETCHARGPARAM, GetChargParam, "$GG\r"    ) \
   Opg(  GETFAULT,      GetFault,      "$GF\r"    )

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
   char szFmtCmd [32] ;
   f_ResultCallback fResultCallback ;
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
   BOOL bResDone ;
} s_Result ;

// ajout FIFO pour insertion CMD
// toutes les sec, ajout x commandes dans FIFO
// si demande enable/disable ajout commande dans FIFO

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void coevse_AddCmdFifo( e_CmdId i_eCmdId, WORD * i_awParams, BYTE i_byNbParam ) ;
static void coevse_SendCmdFifo( void ) ;
static void coevse_AnalyseRes( void ) ;


static void coevse_HrdInit( void ) ;
static void coevse_HrdSendCmd( char * psStrCmd, BYTE i_bySize ) ;


/*----------------------------------------------------------------------------*/
/* variable                                                                 */
/*----------------------------------------------------------------------------*/

static e_CmdId l_eCmd ;
static s_CmdFifo l_CmdFifo ;
static s_Result l_Result ;
static BYTE l_byNbRetry ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void coevse_Init( void )
{
   coevse_HrdInit() ;
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
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void coevse_TaskCyc( void )
{
   coevse_SendCmdFifo( ) ;

   if ( l_Result.bResDone )
   {
      coevse_AnalyseRes() ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
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
   char sStrCmd [64] ;
   BYTE bySize ;

   if ( ( l_CmdFifo.byIdxIn != l_CmdFifo.byIdxOut ) &&
        ( l_eCmd == COEVSE_CMD_NONE ) )
   {
      byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aCmdData ) ;

      pFifoData = &l_CmdFifo.aCmdData[byIdxOut] ;

      eCmd = pFifoData->eCmdId ;
      l_eCmd = eCmd ;

      awPar = &pFifoData->wParam[0] ;

      byCmdIdx = eCmd - ( COEVSE_CMD_NONE + 1 ) ;
      pszFmtCmd = k_aCmdDesc[byCmdIdx].szFmtCmd ;

      snprintf( sStrCmd, sizeof(sStrCmd), pszFmtCmd,
                awPar[0], awPar[1], awPar[2], awPar[3], awPar[4], awPar[5] ) ;

      bySize = strlen( sStrCmd ) ;

      if ( bySize < sizeof(sStrCmd) )
      {
         coevse_HrdSendCmd( sStrCmd, bySize ) ;
      }
      else
      {
         ERR_FATAL() ;
      }
   }
}


/*----------------------------------------------------------------------------*/

static void coevse_AnalyseRes( void )
{
   RESULT rRes ;
   char * szDataRes ;
   BYTE byCmdIdx ;
   f_ResultCallback pFunc ;

   rRes = OK ;

   if ( rRes == OK )
   {
      if ( l_eCmd == COEVSE_CMD_NONE )
      {
         rRes = ERR ;
      }
   }

   if ( rRes == OK )
   {
      //test CRC
      szDataRes = (char*) l_Result.abyDataRes ;
      szDataRes[l_Result.byResIdx-4] = '\0' ;

      if ( ( szDataRes[0] != '$' ) || ( szDataRes[1] != 'O' ) ||
           ( szDataRes[2] != 'K' ) )
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
         (*pFunc)( &szDataRes[3] ) ;
      }

      l_CmdFifo.byIdxOut = NEXTIDX( l_CmdFifo.byIdxOut, l_CmdFifo.aCmdData ) ;
      l_eCmd = COEVSE_CMD_NONE ;
      l_byNbRetry = 0 ;
   }
   else
   {
      if ( l_byNbRetry == 5 )
      {
         ERR_FATAL() ;
      }
      l_byNbRetry++ ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static RESULT coevse_CmdresultIsEvConnect( char C* i_pszDataRes )
{
   REFPARM( i_pszDataRes )
   return OK ;
}


/*----------------------------------------------------------------------------*/

static RESULT coevse_CmdresultGetCurrentCap( char C* i_pszDataRes )
{
   REFPARM( i_pszDataRes )
   return OK ;
}


/*----------------------------------------------------------------------------*/

static RESULT coevse_CmdresultGetChargParam( char C* i_pszDataRes )
{
   REFPARM( i_pszDataRes )
   return OK ;
}


/*----------------------------------------------------------------------------*/

static RESULT coevse_CmdresultGetFault( char C* i_pszDataRes )
{
   REFPARM( i_pszDataRes )
   return OK ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Hardware initialization                                                    */
/*----------------------------------------------------------------------------*/

static void coevse_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

      /* configure TX pin as alternate, no push/pull, high freq */
   OESVE_TX_GPIO_CLK_ENABLE() ;         /* start TX gpio clock */
   sGpioInit.Pin = OESVE_TX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = OESVE_TX_AF ;
   HAL_GPIO_Init( OESVE_TX_GPIO_PORT, &sGpioInit ) ;

      /* configure RX pin as alternate, no push/pull, high freq */
   OESVE_RX_GPIO_CLK_ENABLE() ;         /* start RX gpio clock */
   sGpioInit.Pin = OESVE_RX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = OESVE_RX_AF ;
   HAL_GPIO_Init( OESVE_RX_GPIO_PORT, &sGpioInit ) ;


   /* -------- USART -------- */

   UOEVSE_CLK_ENABLE() ;                /* enable USART clock */

   UOEVSE_FORCE_RESET() ;               /* reset USART  */
   UOEVSE_RELEASE_RESET() ;
                                         /* set baudrate */
   UOEVSE->BRR = UART_DIV_LPUART( APB1_CLK, COEVSE_BAUDRATE ) ;

   UOEVSE->CR3 = USART_CR3_DMAT ;

   UOEVSE->RDR ;                       /* read input register to avoid unwanted data */

                                       /* activate emission and reception */
   UOEVSE->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE ;
   UOEVSE->ICR |= 0xFFFFFFFF ;         /* reset interrupt */

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

static void coevse_HrdSendCmd( char * psStrCmd, BYTE i_bySize )
{
   l_Result.byResIdx = 0 ;
   memset( l_Result.abyDataRes, 0, sizeof(l_Result.abyDataRes) ) ;

   UOEVSE_DMA_TX->CNDTR = i_bySize ;
                                    /* set memory address (transmit buffer) */
   UOEVSE_DMA_TX->CMAR = (DWORD)psStrCmd ;

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

   UOEVSE->ICR |= USART_ICR_ORECF ;

   byResIdx = l_Result.byResIdx ;

   if ( byResIdx > sizeof(l_Result.abyDataRes) - 1 )
   {
      ERR_FATAL() ;
   }

   l_Result.abyDataRes[byResIdx] = byData ;
   l_Result.byResIdx = byResIdx + 1 ;

   if ( byData == '\r' )
   {
      l_Result.bResDone = TRUE ;
   }
}
