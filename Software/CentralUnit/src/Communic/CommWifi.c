/******************************************************************************/
/*                                                                            */
/*                                  CommWifi.c                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   07 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CWIFI_RESET_DURATION     200         /* wifi module reset duration (ms) */
#define CWIFI_PWRUP_DURATION       1         /* duration to wait after reset release (ms) */


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cwifi_HrdInit( void ) ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cwifi_Init( void )
{
   BYTE abyData[512] ;
   DWORD dwInitTmp ;
   WORD wNbReadVal ;

   cwifi_HrdInit() ;

   cwifi_Reset() ;
   uwifi_Init() ;
   uwifi_SetRecErrorDetection( FALSE ) ;

   cwifi_UnReset() ;

   tim_StartMsTmp( &dwInitTmp ) ;      // wait time before enabling error detection
   while ( ! tim_IsEndMsTmp( &dwInitTmp, CWIFI_PWRUP_DURATION ) ) ;

   uwifi_SetRecErrorDetection( TRUE ) ;

   while(1)
   {
      tim_StartMsTmp( &dwInitTmp ) ;      // wait time before enabling error detection
      while ( ! tim_IsEndMsTmp( &dwInitTmp, 1000 ) ) ;

      WORD wReadVar = 0 ;
      wNbReadVal = uwifi_Read( abyData, wReadVar ) ;

      //abyData[0] = 'A' ;
      //abyData[1] = 'T' ;
      //abyData[2] = 0x0D ;
      //abyData[3] = 0x0A ;
      //uwifi_Transmit( abyData, 3 ) ;
   }
}

/*----------------------------------------------------------------------------*/
/* Wifi module reset                                                          */
/*----------------------------------------------------------------------------*/

void cwifi_Reset( void )
{
   DWORD dwRstTmp ;

                                          /* set reset pin to 0 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_RESET ) ;

   tim_StartMsTmp( &dwRstTmp ) ;
   while ( ! tim_IsEndMsTmp( &dwRstTmp, CWIFI_RESET_DURATION ) ) ;
}


/*----------------------------------------------------------------------------*/

void cwifi_UnReset( void )
{                                        /* set reset pin to 1 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_SET ) ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Low level init                                                             */
/*----------------------------------------------------------------------------*/

static void cwifi_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

   WIFI_RESET_GPIO_CLK_ENABLE() ;          /* enable the GPIO_LED Clock */

                                          /* set reset pin to 1 */
   HAL_GPIO_WritePin( WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_SET ) ;

   sGpioInit.Pin = WIFI_RESET_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_PULLUP ;
   sGpioInit.Speed = GPIO_SPEED_FAST ;
   sGpioInit.Alternate = WIFI_RESET_AF ;
   HAL_GPIO_Init( WIFI_RESET_GPIO_PORT, &sGpioInit ) ;
}
