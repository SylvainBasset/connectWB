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

UART_InitTypeDef const k_sUartInit =
{
   .BaudRate        = 115200,
   .WordLength      = UART_WORDLENGTH_8B,
   .StopBits        = UART_STOPBITS_1,
   .Parity          = UART_PARITY_NONE,
   .HwFlowCtl       = UART_HWCONTROL_NONE, //RTS
   .Mode            = UART_MODE_TX_RX,
   .OverSampling    = UART_OVERSAMPLING_16,
} ;

/*----------------------------------------------------------------------------*/

static UART_HandleTypeDef l_UartWiFiHandle ;

BYTE l_byRxBuffer[800] ;
WORD l_wRxIdx ;


/*----------------------------------------------------------------------------*/
/* Uart communication initialisation                                          */
/*----------------------------------------------------------------------------*/

void uwifi_Init( void )
{
   GPIO_InitTypeDef sGpioInit ;

  /* Enable GPIO TX/RX clock */
   WIFI_TX_GPIO_CLK_ENABLE() ;
   WIFI_RX_GPIO_CLK_ENABLE() ;

  /* Enable USART2 clock */
   WIFI_UART_CLK_ENABLE() ;

   WIFI_UART_FORCE_RESET() ;
   WIFI_UART_RELEASE_RESET() ;

   sGpioInit.Pin = WIFI_TX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = WIFI_TX_AF ;
   HAL_GPIO_Init( WIFI_TX_GPIO_PORT, &sGpioInit ) ;

   sGpioInit.Pin = WIFI_RX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = WIFI_RX_AF ;
   HAL_GPIO_Init( WIFI_RX_GPIO_PORT, &sGpioInit ) ;

   sGpioInit.Pin = WIFI_RTS_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = WIFI_RTS_AF ;
   HAL_GPIO_Init( WIFI_RTS_GPIO_PORT, &sGpioInit ) ;

   memset( &l_UartWiFiHandle, 0, sizeof(l_UartWiFiHandle) ) ;
   l_UartWiFiHandle.Instance        = WIFI_UART;
   l_UartWiFiHandle.Init.BaudRate   = 115200 ;
   l_UartWiFiHandle.Init.WordLength = UART_WORDLENGTH_8B;
   l_UartWiFiHandle.Init.StopBits   = UART_STOPBITS_1;
   l_UartWiFiHandle.Init.Parity     = UART_PARITY_NONE;
   l_UartWiFiHandle.Init.HwFlowCtl  = UART_HWCONTROL_RTS;
   l_UartWiFiHandle.Init.Mode       = UART_MODE_TX_RX;

   HAL_NVIC_SetPriority(WIFI_UART_IRQn, 0, 1);
   HAL_NVIC_EnableIRQ(WIFI_UART_IRQn);

   l_wRxIdx = 0 ;
   memset( l_byRxBuffer, 0, sizeof(l_byRxBuffer) ) ;

   if(HAL_UART_Init(&l_UartWiFiHandle) != HAL_OK)
   {
   }


}


/*----------------------------------------------------------------------------*/

void uwifi_StartReceive( void )
{
   //GPIO_InitTypeDef sGpioInit ;
   BYTE byData[10] ;


   //WIFI_RTS_GPIO_CLK_ENABLE() ;
   //sGpioInit.Pin = WIFI_RTS_PIN ;
   //sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   //sGpioInit.Pull = GPIO_PULLUP ;
   //sGpioInit.Speed = GPIO_SPEED_HIGH ;
   //sGpioInit.Alternate = 0 ;
   //HAL_GPIO_Init( WIFI_RTS_GPIO_PORT, &sGpioInit ) ;
   //
   //HAL_GPIO_WritePin( WIFI_RTS_GPIO_PORT, WIFI_RTS_PIN, GPIO_PIN_RESET ) ;


   memset( byData, 0, sizeof(byData) ) ;

   HAL_UART_Receive_IT( &l_UartWiFiHandle, byData, sizeof(byData) ) ;

   WIFI_UART->RDR ;

   if ( byData != 0 )
   {
   }
}


/*----------------------------------------------------------------------------*/

void uwifi_Tramsmit( void )
{
   BYTE byData[4] ;

   byData[0] = 0 ;
   byData[1] = 1 ;
   byData[2] = 2 ;
   byData[3] = 3 ;

   HAL_UART_Transmit( &l_UartWiFiHandle, byData, sizeof(byData), 5000) ;

   if ( byData != 0 )
   {
   }
}


/*----------------------------------------------------------------------------*/

void USART1_IRQHandler( void )
{
   BYTE byReadData ;

   byReadData = (BYTE)( WIFI_UART->RDR & BYTE_MAX ) ;
   l_byRxBuffer[l_wRxIdx] = byReadData ;

   l_wRxIdx = NEXTIDX( l_wRxIdx, l_byRxBuffer ) ;
}
