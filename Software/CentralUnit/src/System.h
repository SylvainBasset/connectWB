/******************************************************************************/
/*                                                                            */
/*                                  System.h                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:    11 mars 2018   Sylvain BASSET        Version 0.1            */
/* Modifications:                                                             */
/******************************************************************************/



/*----------------------------------------------------------------------------*/
/* Error.c                                                                    */
/*----------------------------------------------------------------------------*/

#define FATAL_ERR() \
   err_FatalError()

#define FATAL_ERR_IF( cond ) \
   if ( cond )             \
   {                       \
      err_FatalError() ;   \
   }


void err_FatalError( void ) ;

void NMI_Handler( void ) ;
void HardFault_Handler( void ) ;
void SVC_Handler( void ) ;
void PendSV_Handler( void ) ;
void DebugMon_Handler( void ) ;