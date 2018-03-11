/******************************************************************************/
/*                                                                            */
/*                                   Error.c                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:   10 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx_hal.h>
#include "Define.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Fatal error                  							   				            */
/* Note: This function set the Nucleo LED as output and makes it blink at     */
/* 10 Hz. Never returns                                                       */
/*----------------------------------------------------------------------------*/

void err_FatalError( void )
{
   GPIO_InitTypeDef  GPIO_InitStruct ;

   LED2_GPIO_CLK_ENABLE() ;            /* Enable the GPIO_LED Clock */
                                       /* Configure the LED2_PIN pin as output */
   GPIO_InitStruct.Pin = LED2_PIN ;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP ;
   GPIO_InitStruct.Pull = GPIO_PULLUP ;
   GPIO_InitStruct.Speed = GPIO_SPEED_FAST ;
   HAL_GPIO_Init( LED2_GPIO_PORT, &GPIO_InitStruct ) ;
                                       /* LED2 on */
   HAL_GPIO_WritePin( LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_SET ) ;

   while( TRUE )                       /* infinite loop */
   {
      HAL_GPIO_TogglePin( LED2_GPIO_PORT, LED2_PIN ) ;
      HAL_Delay( 50 ) ;                /* 50 ms delay */
   }
}


/*----------------------------------------------------------------------------*/
/* Non maskable interrupt exception							   				         */
/*----------------------------------------------------------------------------*/

void NMI_Handler( void )
{
   err_FatalError() ;                  /* MNI exceptions not allowed */
}


/*----------------------------------------------------------------------------*/
/* Hard Fault exception                                                       */
/*----------------------------------------------------------------------------*/

void HardFault_Handler( void )
{
   err_FatalError() ;                  /* Hard Fault exceptions not allowed */
}


/*----------------------------------------------------------------------------*/
/* SVC (supervisor call) exception						   				            */
/*----------------------------------------------------------------------------*/

void SVC_Handler( void )
{
   err_FatalError() ;                  /* SVC exceptions not allowed */
}


/*----------------------------------------------------------------------------*/
/* Pending SVC (supervisor call) exception						   				   */
/*----------------------------------------------------------------------------*/

void PendSV_Handler( void )
{
   err_FatalError() ;                  /* pending SVC exceptions not allowed */
}


/*----------------------------------------------------------------------------*/
/* Debug Monitor exception					   				                        */
/*----------------------------------------------------------------------------*/

void DebugMon_Handler( void )
{
   err_FatalError() ;                  /* Debug Monitor exception not allowed */
}