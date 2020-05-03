/******************************************************************************/
/*                                  Eeprom.c                                  */
/******************************************************************************/
/*
   Eeprom managment

   Copyright (C) 2018  Sylvain BASSET

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

   ------------
   @version 1.0
   @history 1.0, 04 apr. 2018, creation
*/


#include "Define.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define EEP_BUSY_TIMEOUT   3000        /* eepreom operation timeout, sec */



/*----------------------------------------------------------------------------*/
/* High level wifi SSID/PWD eeprom write                                      */
/*----------------------------------------------------------------------------*/

RESULT eep_WriteWifiId( BOOL i_bIsSsid, char C* i_szParam )
{
   DWORD dwEepAddr ;
   BYTE byEepSize ;
   BYTE byParamSize ;
   DWORD dwEepVal ;
   BYTE byShift ;
   char C* pszChar ;
   RESULT rRet ;

   if ( i_bIsSsid )
   {
      dwEepAddr = (DWORD)&g_sDataEeprom->sWifiConInfo.szWifiSSID ;
      byEepSize = sizeof(g_sDataEeprom->sWifiConInfo.szWifiSSID) ;
   }
   else
   {
      dwEepAddr = (DWORD)&g_sDataEeprom->sWifiConInfo.szWifiPassword ;
      byEepSize = sizeof(g_sDataEeprom->sWifiConInfo.szWifiPassword) ;
   }

   byParamSize = strlen( i_szParam ) + 1 ;

   if ( byParamSize <= byEepSize )
   {
      dwEepVal = 0 ;
      byShift = 0 ;
      pszChar = i_szParam ;
      while( byParamSize != 0 )                 /* concatenate char in 32 bits words */
      {
         dwEepVal |= ( *pszChar << byShift ) ;
         byShift += 8 ;
         pszChar++ ;
         byParamSize-- ;
         if ( byShift == 32 )
         {
            eep_write( dwEepAddr, dwEepVal ) ;
            byShift = 0 ;
            dwEepVal = 0 ;
            dwEepAddr += 4 ;
         }
      }
      if ( byShift != 0 )
      {
         eep_write( dwEepAddr, dwEepVal ) ;
      }
      rRet = OK ;
   }
   else
   {
      rRet = ERR ;
   }
   return rRet ;
}


/*----------------------------------------------------------------------------*/
/* Eeprom writing operation                                                   */
/*    - <i_dwAddress> address in eeprom                                       */
/*    - <i_dwValue> value to write                                            */
/*----------------------------------------------------------------------------*/

void eep_write( DWORD i_dwAddress, DWORD i_dwValue )
{
   DWORD dwTmpTimeout ;
                                       /* adress must be in eeprom and DWORD aligned */
   if ( ( i_dwAddress >= DATA_EEPROM_BASE ) && ( i_dwAddress <= DATA_EEPROM_END ) &&
        ( ( i_dwAddress % 4 ) == 0 ) )
   {                                   /* if eeprom is locked */
      if( ISSET( FLASH->PECR, FLASH_PECR_PELOCK ) )
      {
         FLASH->PEKEYR = FLASH_PEKEY1; /* sequence to unlocking eeprom */
         FLASH->PEKEYR = FLASH_PEKEY2;
      }

      tim_StartMsTmp( &dwTmpTimeout ) ; /* start timeout tempo */
                                       /* wait pervious eeprom operation has finished */
      while ( ISSET( FLASH->SR, FLASH_FLAG_BSY ) )
      {                                /* if timeout is finished */
         if ( tim_IsEndMsTmp( &dwTmpTimeout, EEP_BUSY_TIMEOUT ) )
         {
            break ;
         }
      }
                                       /* write the value */
      *(volatile DWORD *)i_dwAddress = i_dwValue ;
                                       /* Set the PELOCK Bit to lock eeprom access */
      SET_BIT( FLASH->PECR, FLASH_PECR_PELOCK ) ;
   }
}