/******************************************************************************/
/*                                                                            */
/*                                  Eeprom.c                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:   04 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define EEP_BUSY_TIMEOUT   3000        /* eepreom operation timeout, sec */


/*----------------------------------------------------------------------------*/
/* Eeprom writing operation                                                   */
/*    - <i_dwAddress> address in eeprom                                       */
/*    - <i_dwValue> value to write                                            */
/*----------------------------------------------------------------------------*/

void eep_write( DWORD i_dwAddress, DWORD i_dwValue )
{
   DWORD dwTmpTimeout ;
                                       /* if adress is not in eeprom */
   ERR_FATAL_IF( ( i_dwAddress < DATA_EEPROM_BASE ) ||
                 ( i_dwAddress > DATA_EEPROM_END ) ) ;
                                       /* if adress is not DWORD aligned */
   ERR_FATAL_IF( ( i_dwAddress % 4 ) != 0 ) ;
                                       /* if eeprom is locked */
   if( ISSET( FLASH->PECR, FLASH_PECR_PELOCK ) )
   {
      FLASH->PEKEYR = FLASH_PEKEY1;    /* sequence to unlocking eeprom */
      FLASH->PEKEYR = FLASH_PEKEY2;
   }

   tim_StartMsTmp( &dwTmpTimeout ) ;   /* start timeout tempo */
                                       /* wait pervious eeprom operation has finished */
   while ( ISSET( FLASH->SR, FLASH_FLAG_BSY ) )
   {                                   /* if timeout is finished */
      if ( tim_IsEndMsTmp( &dwTmpTimeout, EEP_BUSY_TIMEOUT ) )
      {
         ERR_FATAL() ;
      }
   }
                                       /* write the value */
   *(volatile DWORD *)i_dwAddress = i_dwValue ;
                                       /* Set the PELOCK Bit to lock eeprom access */
   SET_BIT( FLASH->PECR, FLASH_PECR_PELOCK ) ;
}