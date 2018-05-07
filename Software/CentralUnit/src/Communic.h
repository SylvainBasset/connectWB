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

#define UWIFI_ERROR_RX        1u
#define UWIFI_ERROR_DMA_TX    2u
#define UWIFI_ERROR_DMA_RX    4u

void uwifi_Init( void ) ;
void uwifi_SetRecErrorDetection( BOOL i_bEnable ) ;
WORD uwifi_Read( void * o_pvData, WORD i_dwMaxSize ) ;
BOOL uwifi_Send( void const* i_pvData, DWORD i_dwSize ) ;
BYTE uwifi_GetError( BOOL i_bReset ) ;
#endif /* __COMMUNIC_H */
