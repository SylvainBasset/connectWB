/******************************************************************************/
/*                                                                            */
/*                                 Communic.h                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:    8 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __COMMUNIC_H                   /* to prevent recursive inclusion */
#define __COMMUNIC_H


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void ) ;
void cwifi_Reset( void ) ;
void cwifi_UnReset( void ) ;

/*----------------------------------------------------------------------------*/
/* UartWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

void uwifi_Init( void ) ;
void uwifi_Transmit( void const* i_pvData, DWORD i_dwSize ) ;
void WIFI_UART_IRQHandler(void) ;
#endif /* __COMMUNIC_H */
