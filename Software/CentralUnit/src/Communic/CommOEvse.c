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


typedef void (*f_CmdResult)( char C* i_pszResExt, BOOL i_bLastCall ) ;

#define COEVSE_CMD_ENUM( NameUp, NameLo, StrCmd, Func ) COEVSE_CMD_##NameUp,

#define COEVSE_CMD_DESC( NameUp, NameLo, StrCmd, Func ) \
   { .eCmdId = COEVSE_CMD_##NameUp, .szStrCmd = StrCmd, .byStrSize = sizeof(StrCmd), \
     .fCmdResult = Func },


#define LIST_CMD( Op ) \
   Op(  ENABLE,  enable,  "$FE\r", NULL ) \
   Op(  DISABLE, disable, "$FD\r", NULL )

typedef enum
{
   COEVSE_CMD_NONE = 0,
   LIST_CMD( COEVSE_CMD_ENUM )
   COEVSE_CMD_LAST,
} e_CmdId ;

typedef struct
{
   e_CmdId eCmdId ;
   char szStrCmd [32] ;
   BYTE byStrSize ;
   f_CmdResult fCmdResult ;
} s_CmdDesc ;

static s_CmdDesc const k_aCmdDesc [] =
{
   LIST_CMD( COEVSE_CMD_DESC )
} ;

// ajout FIFO pour insertion CMD
// toutes les sec, ajout x commandes dans FIFO
// si demande enable/disable ajout commande dans FIFO

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void coevse_SendCmd( e_CmdId i_eCmdId ) ;
static void coevse_HrdInit( void ) ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static e_CmdId l_eCmd ;

static BYTE l_abyResponse[32] ;
static BYTE l_byResIdx ;
static BOOL l_bResDone ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void coevse_Init( void )
{
   coevse_HrdInit() ;

   coevse_SendCmd( COEVSE_CMD_ENABLE ) ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void coevse_TaskCyc( void )
{

   if ( l_bResDone )
   {
      l_eCmd = COEVSE_CMD_NONE ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void coevse_SendCmd( e_CmdId i_eCmdId )
{
   BYTE byCmdIdx ;
   char C* pszStrcmd ;
   BYTE bySize ;

   l_byResIdx = 0 ;
   memset( l_abyResponse, 0, sizeof(l_abyResponse) ) ;

   byCmdIdx = i_eCmdId - COEVSE_CMD_NONE ;
   pszStrcmd = k_aCmdDesc[byCmdIdx].szStrCmd ;
   bySize = k_aCmdDesc[byCmdIdx].byStrSize ;

   UOEVSE_DMA_TX->CNDTR = bySize ;
                                    /* set memory address (transmit buffer) */
   UOEVSE_DMA_TX->CMAR = (DWORD)pszStrcmd ;

   COEVSE_ENABLE_DMA_TX() ;

   l_eCmd = i_eCmdId ;
}


/*----------------------------------------------------------------------------*/
/* Open EVSE USART interrupt                                                  */
/*----------------------------------------------------------------------------*/

void UOEVSE_IRQHandler( void )
{
   BYTE byData ;

   //UOEVSE->ICR |= USART_ISR_TC ;

   byData = UOEVSE->RDR ;

   UOEVSE->ICR |= USART_ICR_ORECF ;

   l_abyResponse[l_byResIdx] = byData ;
   l_byResIdx++ ;

   if ( byData == '\r' )
   {
      l_bResDone = TRUE ;
   }
}


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
