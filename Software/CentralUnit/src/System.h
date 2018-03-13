/******************************************************************************/
/*                                                                            */
/*                                  System.h                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:    11 mars 2018   Sylvain BASSET        Version 0.1            */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __SYSTEM_H                     /* to prevent recursive inclusion */
#define __SYSTEM_H


/*----------------------------------------------------------------------------*/
/* Error.c                                                                    */
/*----------------------------------------------------------------------------*/

#define ERR_FATAL() \
   err_FatalError()

#define ERR_FATAL_IF( cond ) \
   if ( cond )               \
   {                         \
      err_FatalError() ;     \
   }


void err_FatalError( void ) ;

//void NMI_Handler( void ) ;
//void HardFault_Handler( void ) ;
//void SVC_Handler( void ) ;
//void PendSV_Handler( void ) ;
//void DebugMon_Handler( void ) ;


/*----------------------------------------------------------------------------*/
/* Timer.c                                                                    */
/*----------------------------------------------------------------------------*/

void tim_StartMsTmp( DWORD* io_pdwTempo ) ;
void tim_StartSecTmp( DWORD* io_pdwTempo ) ;

BOOL tim_IsEndMsTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;
BOOL tim_IsEndSecTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;

DWORD tim_GetRemainMsTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;
DWORD tim_GetRemainSecTmp( DWORD* io_pdwTempo, DWORD i_dwDelay ) ;

#endif /* __SYSTEM_H */