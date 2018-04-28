/******************************************************************************/
/*                                                                            */
/*                                 UartWifi.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:   08 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define UWIFI_BAUDRATE     115200llu
#define UWIFI_RXBUFSIZE    400 //512

#define UWIFI_DISABLE_DMA_RX()     ( UWIFI_DMA_RX->CCR &= ~DMA_CCR_EN )
#define UWIFI_ENABLE_DMA_RX()      ( UWIFI_DMA_RX->CCR |= DMA_CCR_EN )
#define UWIFI_DISABLE_DMA_TX()     ( UWIFI_DMA_TX->CCR &= ~DMA_CCR_EN )
#define UWIFI_ENABLE_DMA_TX()      ( UWIFI_DMA_TX->CCR |= DMA_CCR_EN )


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void uwifi_HrdInit( void ) ;
static void wifi_DmaTxIrqHandle( void ) ;
static void wifi_DmaRxIrqHandle( void ) ;
static BOOL uwifi_UpdateCheckIdx( void ) ;

/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

BYTE l_byRxBuffer [UWIFI_RXBUFSIZE] ;
WORD l_wRxIdxIn ;
WORD l_wRxIdxOut ;
BOOL l_bRxSuspend ;

BOOL l_bTxPending ;
BOOL l_byErrors ;


/*----------------------------------------------------------------------------*/
/* Uart communication initialisation                                          */
/*----------------------------------------------------------------------------*/

void uwifi_Init( void )
{
   memset( l_byRxBuffer, 0, sizeof(l_byRxBuffer) ) ;
   l_wRxIdxIn = 0 ;
   l_wRxIdxOut = 0 ;
   l_bRxSuspend = FALSE ;

   l_bTxPending = 0 ;
   l_byErrors = 0 ;

   uwifi_HrdInit() ;

   uwifi_StartReceive() ;//
}


/*----------------------------------------------------------------------------*/

void uwifi_StartReceive( void )
{
   UWIFI_DMA_RX->CNDTR = sizeof(l_byRxBuffer) ; //SBA
   UWIFI_DMA_RX->CPAR = (DWORD)&(UWIFI->RDR) ;
   UWIFI_DMA_RX->CMAR = (DWORD)l_byRxBuffer ;
   UWIFI_ENABLE_DMA_RX() ;
}


/*----------------------------------------------------------------------------*/

BOOL uwifi_Transmit( void const* i_pvData, DWORD i_dwSize )
{
   BOOL bRet ;

   if ( ! l_bTxPending )
   {
      l_bTxPending = TRUE ;

      UWIFI->ICR |= USART_ICR_TCCF ;

      UWIFI_DMA_TX->CNDTR = i_dwSize ;
      UWIFI_DMA_TX->CPAR = (DWORD)&(UWIFI->TDR) ; //SBA à mettre dans init
      UWIFI_DMA_TX->CMAR = (DWORD)i_pvData ;
      UWIFI_ENABLE_DMA_TX() ;

      bRet = TRUE ;
   }
   else
   {
      bRet = FALSE ;
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/

WORD uwifi_Read( void * o_pvData, WORD i_dwMaxSize )
{
   BOOL bNeedSuspendRx ;
   WORD wSizeRead ;

   UWIFI_DISABLE_DMA_RX() ;
   HAL_NVIC_DisableIRQ( UWIFI_DMA_IRQn ) ;

   uwifi_UpdateCheckIdx() ;

   if ( l_wRxIdxIn < l_wRxIdxOut )
   {
      wSizeRead = sizeof(l_byRxBuffer) - ( l_wRxIdxOut - l_wRxIdxIn ) ;
   }
   else
   {
      wSizeRead = l_wRxIdxIn - l_wRxIdxOut ;
   }

   if ( wSizeRead > i_dwMaxSize )
   {
      wSizeRead = i_dwMaxSize ;
   }

   memcpy( o_pvData, l_byRxBuffer, wSizeRead ) ;
   l_wRxIdxOut = ( l_wRxIdxOut + wSizeRead ) % sizeof(l_byRxBuffer) ;

   bNeedSuspendRx = uwifi_UpdateCheckIdx() ;
   if ( ! bNeedSuspendRx )
   {
      l_bRxSuspend = FALSE ;
      UWIFI_ENABLE_DMA_RX() ;
   }

   HAL_NVIC_EnableIRQ( UWIFI_DMA_IRQn ) ;

   return wSizeRead ;
}


/*----------------------------------------------------------------------------*/

void uwifi_SetRecErrorDetection( BOOL i_bEnable )
{
   if ( i_bEnable )
   {
      UWIFI->CR3 |= USART_CR3_EIE ;
      UWIFI->CR1 |= USART_CR1_PEIE ;
      UWIFI_DMA_TX->CCR |= DMA_CCR_TEIE ;
      UWIFI_DMA_RX->CCR |= DMA_CCR_TEIE ;

      l_byErrors = 0 ;
   }
   else
   {
      UWIFI->CR3 &= ~USART_CR3_EIE ;
      UWIFI->CR1 &= ~USART_CR1_PEIE ;
      UWIFI_DMA_TX->CCR &= ~DMA_CCR_TEIE ;
      UWIFI_DMA_RX->CCR &= ~DMA_CCR_TEIE ;
   }
}


/*----------------------------------------------------------------------------*/

BYTE uwifi_GetError( void )
{
   return l_byErrors ;
}


/*----------------------------------------------------------------------------*/

void USART1_IRQHandler( void )
{
   DWORD dwErrorFlag ;

   dwErrorFlag = USART_ISR_PE | USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE ;

   if ( ( UWIFI->ISR & dwErrorFlag ) != 0 )
   {
         /* Parity Error Clear Flag, Framing Error Clear Flag */
         /* Noise detected Clear Flag, OverRun Error Clear Flag */
      UWIFI->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;

      l_byErrors |= UWIFI_ERROR_RX ;
   }
}


/*----------------------------------------------------------------------------*/

void UWIFI_DMA_IRQHandler( void )
{
   wifi_DmaTxIrqHandle() ;
   wifi_DmaRxIrqHandle() ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void uwifi_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

   WIFI_TX_GPIO_CLK_ENABLE() ;
   sGpioInit.Pin = WIFI_TX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = WIFI_TX_AF ;
   HAL_GPIO_Init( WIFI_TX_GPIO_PORT, &sGpioInit ) ;

   WIFI_RX_GPIO_CLK_ENABLE() ;
   sGpioInit.Pin = WIFI_RX_PIN ;
   sGpioInit.Alternate = WIFI_RX_AF ;
   HAL_GPIO_Init( WIFI_RX_GPIO_PORT, &sGpioInit ) ;

   WIFI_RTS_GPIO_CLK_ENABLE() ;
   sGpioInit.Pin = WIFI_RTS_PIN ;
   sGpioInit.Alternate = WIFI_RTS_AF ;
   HAL_GPIO_Init( WIFI_RTS_GPIO_PORT, &sGpioInit ) ;

   WIFI_CTS_GPIO_CLK_ENABLE() ;
   sGpioInit.Pin = WIFI_CTS_PIN ;
   sGpioInit.Alternate = WIFI_CTS_AF ;
   HAL_GPIO_Init( WIFI_CTS_GPIO_PORT, &sGpioInit ) ;

  /* Enable USART2 clock */
   UWIFI_CLK_ENABLE() ;

   UWIFI_FORCE_RESET() ;
   UWIFI_RELEASE_RESET() ;
                                       /* activate emission and reception */
   UWIFI->CR1 |= ( USART_CR1_TE | USART_CR1_RE ) ;
                                       /* activate DMA and RTS/CTS management */
   UWIFI->CR3 |= ( USART_CR3_DMAR | USART_CR3_DMAT | USART_CR3_RTSE | USART_CR3_CTSE ) ;

   UWIFI->BRR = (uint16_t)( UART_DIV_SAMPLING16( APB1_CLK, UWIFI_BAUDRATE ) ) ;

   UWIFI->CR1 |= USART_CR1_UE ;

   UWIFI->RDR ;

   UWIFI->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;

   HAL_NVIC_SetPriority( UWIFI_IRQn, UWIFI_IRQPri, 1 ) ;
   HAL_NVIC_EnableIRQ( UWIFI_IRQn ) ;

   /* DMA */

   UWIFI_DMA_TX_CLK_ENABLE();

   UWIFI_DMA_TX->CCR = ( DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE ) ;

   UWIFI_DMA_CSELR->CSELR &= ~UWIFI_DMA_TX_CSELR( DMA_CSELR_C1S_Msk ) ;
   UWIFI_DMA_CSELR->CSELR |= UWIFI_DMA_TX_CSELR( UWIFI_DMA_TX_REQ ) ;

   UWIFI_DMA_RX_CLK_ENABLE() ;

   UWIFI_DMA_RX->CCR = ( DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_TCIE | DMA_CCR_HTIE ) ;

   UWIFI_DMA_CSELR->CSELR &= ~UWIFI_DMA_RX_CSELR( DMA_CSELR_C1S_Msk ) ;
   UWIFI_DMA_CSELR->CSELR |= UWIFI_DMA_RX_CSELR( UWIFI_DMA_RX_REQ ) ;

   HAL_NVIC_SetPriority( UWIFI_DMA_IRQn, UWIFI_DMA_IRQPri, 0 ) ;
   HAL_NVIC_EnableIRQ( UWIFI_DMA_IRQn ) ;
}


/*----------------------------------------------------------------------------*/

static void wifi_DmaTxIrqHandle( void )
{
   DWORD dwIsrVal ;

   dwIsrVal = UWIFI_DMA->ISR ;

   if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_GIF1 ) ) )
   {
      UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CGIF1 ) ;

      if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_TCIF1 ) ) )
      {
         UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CTCIF1 ) ;
         UWIFI_DISABLE_DMA_TX() ;
         l_bTxPending = FALSE ;
      }

      if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_TEIF1 ) ) )
      {
         UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;
         l_byErrors |= UWIFI_ERROR_DMA_TX ;
      }
   }
}


/*----------------------------------------------------------------------------*/

static void wifi_DmaRxIrqHandle( void )
{
   DWORD dwIsrVal ;
   BOOL bNeedCheck ;
   BOOL bNeedSuspendRx ;

   dwIsrVal = UWIFI_DMA->ISR ;

   if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_GIF1 ) ) )
   {
      bNeedCheck = FALSE ;

      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_TCIF1 ) ) )
      {
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CTCIF1 ) ;
         bNeedCheck = TRUE ;
      }

      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_HTIF1 ) ) )
      {
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CHTIF1 ) ;
         bNeedCheck = TRUE ;
      }

      if ( bNeedCheck )
      {
         bNeedSuspendRx = uwifi_UpdateCheckIdx() ;
         if ( bNeedSuspendRx )
         {
            UWIFI_DISABLE_DMA_RX() ;
            l_bRxSuspend = TRUE ;
         }
      }

      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_TEIF1 ) ) )
      {
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;
         l_byErrors |= UWIFI_ERROR_DMA_RX ;
      }
   }
}


/*----------------------------------------------------------------------------*/

static BOOL uwifi_UpdateCheckIdx( void )
{
   BOOL bNeedSuspendRx ;

   l_wRxIdxIn = sizeof(l_byRxBuffer) - UWIFI_DMA_RX->CNDTR ;

   bNeedSuspendRx = FALSE ;

   if ( l_wRxIdxIn < l_wRxIdxOut )
   {
      //if ( ( l_wRxIdxIn + ( sizeof(l_byRxBuffer) / 2 ) >= l_wRxIdxOut )
      //if ( ( ( sizeof(l_byRxBuffer) / 2 ) >= l_wRxIdxOut - l_wRxIdxIn )
      if ( ( l_wRxIdxOut - l_wRxIdxIn ) <= ( sizeof(l_byRxBuffer) / 2 ) )
      {
         bNeedSuspendRx = TRUE ;
      }
   }
   else
   {
      if ( ( l_wRxIdxIn - l_wRxIdxOut ) >= ( sizeof(l_byRxBuffer) / 2 ) )
      {
         bNeedSuspendRx = TRUE ;
      }
   }

   return bNeedSuspendRx ;
}
