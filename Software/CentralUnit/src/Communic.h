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
/* ScktFrame.c                                                                */
/*----------------------------------------------------------------------------*/

void sfrm_Init( void ) ;


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

typedef void (*f_ScktGetFrame)( char C* i_pszFrame ) ;
typedef void (*f_ScktGetResExt)( char C* i_pszResExt, BOOL i_bLastCall ) ;

void cwifi_Init( void ) ;
void cwifi_RegisterScktFunc( f_ScktGetFrame fScktGetFrame,
                             f_ScktGetResExt fScktGetResExt ) ;
BOOL cwifi_IsConnected( void ) ;
BOOL cwifi_IsSocketConnected( void ) ;
void cwifi_AddExtCmd( char C* i_szStrCmd ) ;
void cwifi_AddExtData( char C* i_szStrCmd ) ;
void cwifi_TaskCyc( void ) ;


/*----------------------------------------------------------------------------*/
/* UartWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

#define UWIFI_ERROR_RX        1u
#define UWIFI_ERROR_DMA_TX    2u
#define UWIFI_ERROR_DMA_RX    4u

void uwifi_Init( void ) ;
WORD uwifi_Read( BYTE * o_pbyData, WORD i_dwMaxSize, BOOL i_bGetPending ) ;
BOOL uwifi_Send( void const* i_pvData, DWORD i_dwSize ) ;
BOOL uWifi_IsSendDone( void ) ;
void uwifi_SetErrorDetection( BOOL i_bEnable ) ;
BYTE uwifi_GetError( BOOL i_bReset ) ;
#endif /* __COMMUNIC_H */
