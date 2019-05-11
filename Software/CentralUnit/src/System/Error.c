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
   GPIO_InitTypeDef sGpioInit ;

                                       /* Configure SYSLED_PIN pin as output push-pull */
   sGpioInit.Pin = SYSLED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_PULLUP ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   HAL_GPIO_Init( SYSLED_GPIO, &sGpioInit ) ;
                                       /* system Led on */
   HAL_GPIO_WritePin( SYSLED_GPIO, SYSLED_PIN, GPIO_PIN_SET ) ;


   sGpioInit.Pin = CSTATE_LEDCH_RED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDCH_RED_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_RED_GPIO, &sGpioInit ) ;

   sGpioInit.Pin = CSTATE_LEDCH_BLUE_PIN ;
   sGpioInit.Alternate = CSTATE_LEDCH_BLUE_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_BLUE_GPIO, &sGpioInit ) ;

   sGpioInit.Pin = CSTATE_LEDCH_GREEN_PIN ;
   sGpioInit.Alternate = CSTATE_LEDCH_GREEN_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_GREEN_GPIO, &sGpioInit ) ;

   sGpioInit.Pin = CSTATE_LEDCH_COMMON_PIN ;
   sGpioInit.Alternate = CSTATE_LEDCH_COMMON_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_COMMON_GPIO, &sGpioInit ) ;

   HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_SET ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_RESET ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_RESET ) ;


   while( TRUE )                       /* infinite loop */
   {
      HAL_Delay( 50 ) ;                /* 50 ms delay */
      HAL_GPIO_TogglePin( SYSLED_GPIO, SYSLED_PIN ) ;
      HAL_GPIO_TogglePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN ) ;
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
