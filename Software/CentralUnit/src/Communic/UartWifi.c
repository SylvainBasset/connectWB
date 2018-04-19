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


//UART_InitTypeDef const k_sUartInit =
//{
//   .BaudRate        = 115200,
//   .WordLength      = UART_WORDLENGTH_8B,
//   .StopBits        = UART_STOPBITS_1,
//   .Parity          = UART_PARITY_NONE,
//   .HwFlowCtl       = UART_HWCONTROL_NONE, //RTS
//   .Mode            = UART_MODE_TX_RX,
//   .OverSampling    = UART_OVERSAMPLING_16,
//} ;

/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define UWIFI_BAUDRATE  115200llu

#define UWIFI_TX_TIMEOUT   100         /* transmission timeout (ms) */



/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void uwifi_HrdInit( void ) ;
static void uwifi_StartReceive( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

BYTE l_byRxBuffer[800] ;
WORD l_wRxIdx ;


/*----------------------------------------------------------------------------*/
/* Uart communication initialisation                                          */
/*----------------------------------------------------------------------------*/

void uwifi_Init( void )
{
   memset( l_byRxBuffer, 0, sizeof(l_byRxBuffer) ) ;
   l_wRxIdx = 0 ;

   uwifi_HrdInit() ;

   uwifi_StartReceive() ;
}


/*----------------------------------------------------------------------------*/

void uwifi_Transmit( void const* i_pvData, DWORD i_dwSize )
{
   BYTE const* pbyData ;

   pbyData = (BYTE *)i_pvData ;

   WIFI_UART_DMA_TX->CNDTR = i_dwSize ;
   WIFI_UART_DMA_TX->CPAR = (DWORD)&(WIFI_UART->TDR) ;
   WIFI_UART_DMA_TX->CMAR = (DWORD)&pbyData[0] ;
   WIFI_UART_DMA_TX->CCR |= DMA_CCR_EN ;

   WIFI_UART->ICR |= USART_ICR_TCCF ;
}


/*----------------------------------------------------------------------------*/

void USART1_IRQHandler( void )
{
   BYTE byReadData ;
   DWORD dwErrorFlag ;

   //dwErrorFlag = USART_ISR_PE | USART_ISR_FE | USART_ISR_ORE /* | USART_ISR_NE */ ;

   //if ( ( WIFI_UART->ISR & dwErrorFlag ) != 0 )
   //{
   //   while(1) ; //SBA
   //}

   byReadData = (BYTE)( WIFI_UART->RDR & BYTE_MAX ) ;
   l_byRxBuffer[l_wRxIdx] = byReadData ;

   l_wRxIdx = NEXTIDX( l_wRxIdx, l_byRxBuffer ) ;

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
   WIFI_UART_CLK_ENABLE() ;

   WIFI_UART_FORCE_RESET() ;
   WIFI_UART_RELEASE_RESET() ;
                                       /* activate emission and reception */
   WIFI_UART->CR1 |= ( USART_CR1_TE |USART_CR1_RE ) ;
                                       /* activate DMA and RTS/CTS management */
   WIFI_UART->CR3 |= ( USART_CR3_DMAT | USART_CR3_RTSE | USART_CR3_CTSE ) ;

   WIFI_UART->BRR = (uint16_t)( UART_DIV_SAMPLING16( APB1_CLK, UWIFI_BAUDRATE ) ) ;

   WIFI_UART->CR1 |= USART_CR1_UE ;

   WIFI_UART->RDR ;

   WIFI_UART->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;

   HAL_NVIC_SetPriority(WIFI_UART_IRQn, 0, 1) ;
   HAL_NVIC_EnableIRQ(WIFI_UART_IRQn) ;

   /* DMA */

   WIFI_UART_DMA_TX_CLK_ENABLE();

   WIFI_UART_DMA_TX->CCR = ( DMA_CCR_DIR | DMA_CCR_MINC ) ;

   WIFI_UART_DMA_TX_CSELR->CSELR &= ~DMA_MAKE_CSELR( DMA_CSELR_CxS_Msk, WIFI_UART_DMA_TX_CHANNEL ) ;
   WIFI_UART_DMA_TX_CSELR->CSELR |= DMA_MAKE_CSELR( WIFI_UART_DMA_TX_REQ, WIFI_UART_DMA_TX_CHANNEL ) ;


   //WIFI_UART_DMA_RX_CLK_ENABLE() ;
   //
   //WIFI_UART_DMA_RX->CCR = ( DMA_CCR_MINC ) ; //DMA_CIRCULAR
   //
   //WIFI_UART_DMA_RX_CSELR->CSELR &= ~DMA_MAKE_CSELR( DMA_CSELR_CxS_Msk, WIFI_UART_DMA_RX_CHANNEL ) ;
   //WIFI_UART_DMA_RX_CSELR->CSELR |= DMA_MAKE_CSELR( WIFI_UART_DMA_RX_REQ, WIFI_UART_DMA_RX_CHANNEL ) ;
}


/*----------------------------------------------------------------------------*/

static void uwifi_StartReceive( void )
{
   //WIFI_UART->CR3 |= USART_CR3_EIE ;
   WIFI_UART->CR1 |= ( /* USART_CR1_PEIE |*/ USART_CR1_RXNEIE ) ;

   //WIFI_UART_DMA_RX->CNDTR = sizeof(l_byRxBuffer) ;
   //WIFI_UART_DMA_RX->CPAR = (DWORD)&(WIFI_UART->RDR) ;
   //WIFI_UART_DMA_RX->CMAR = (DWORD)&l_byRxBuffer[0] ;
   //WIFI_UART_DMA_RX->CCR |= DMA_CCR_EN ;
}