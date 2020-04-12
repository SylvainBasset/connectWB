/******************************************************************************/
/*                                 ConvAcsii.c                                */
/******************************************************************************/
/*
   Ascii integer conversion librairy module

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
   @history 1.0, 11 avr. 2020, creation
   @brief
   Implement read of next decimal or hexadecimal number, ignoring non digit
   caracter at the begining of the string
*/


#include "Define.h"

#include "Lib.h"


/*----------------------------------------------------------------------------*/
/* convert next digit to integer                                              */
/*----------------------------------------------------------------------------*/

char C* cascii_GetNextDec( char C* i_pszStr, SDWORD *o_psdwValue,
                           BOOL i_bIsSigned, DWORD i_dwMax )
{
   BOOL bEndOfString ;
   BOOL bNeg ;
   DWORD dwValue ;
   char C* pszStr ;
   char C* pszEnd ;
   BYTE byVal ;

   bEndOfString = FALSE ;
   bNeg = FALSE ;
   dwValue = 0 ;
   pszStr = i_pszStr ;
   pszEnd = i_pszStr ;
                                       /* loop while there are no digit */
   while( ( *pszStr < '0' ) || ( *pszStr > '9' ) )
   {									//SBA spécifique à openEVSE
      if ( *pszStr == '\0' ) //|| ( *pszStr == '^' ) )
      {
         bEndOfString = TRUE ;
         break ;
      }								   /* check negative sign */
      if ( i_bIsSigned && ( *pszStr == '-' ) )
      {
         bNeg ^= TRUE ;
      }
      else
      {
         bNeg = FALSE ;
      }
      pszStr++ ;
   }

   if ( ! bEndOfString )
   {                                   /* loop until last digit */
      while( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) )
      {
         byVal = ( *pszStr - '0' ) ;
                                       /* if maximum is reached */
         if ( ( dwValue > ( i_dwMax - byVal ) / 10 ) )
         {
            dwValue = i_dwMax ;
                                       /* set pszStr directely to the end of the number */
            while( ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) && ( *pszStr != 0 ) ) )
            {
               pszStr++ ;
            }
            break ;
         }
         dwValue *= 10 ;
         dwValue += byVal ;
         pszStr++ ;
      }
      pszEnd = pszStr ;
   }

   if ( o_psdwValue != NULL )
   {
      if ( i_bIsSigned && bNeg )        /* adjust sign */
      {
         *o_psdwValue = -(SDWORD)dwValue ;
      }
      else
      {
         *o_psdwValue = (SDWORD)dwValue ;
      }
   }

   return pszEnd ;
}


/*----------------------------------------------------------------------------*/
/* convert next hexa to integer                                               */
/*----------------------------------------------------------------------------*/

char C* cascii_GetNextHex( char C* i_pszStr, DWORD *o_pdwValue )
{
   BOOL bEndOfString ;
   DWORD dwValue ;
   char C* pszStr ;
   char C* pszEnd ;

   bEndOfString = FALSE ;
   dwValue = 0 ;
   pszStr = i_pszStr ;
   pszEnd = i_pszStr ;
                                       /* loop while there are no digit */
   while( ! ( ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) ) ||
              ( ( *pszStr >= 'A' ) && ( *pszStr <= 'F' ) ) ) )
   {
      if ( ( *pszStr == '\0' ) || ( *pszStr == '^' ) )
      {
         bEndOfString = TRUE ;
         break ;
      }
      pszStr++ ;
   }

   if ( ! bEndOfString )
   {                                   /* loop until last digit */
      while( ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) ) ||
             ( ( *pszStr >= 'A' ) && ( *pszStr <= 'F' ) ) )
         {
            dwValue *= 16 ;
            if ( ( *pszStr >= '0' ) && ( *pszStr <= '9' ) )
            {
               dwValue += ( *pszStr - '0' ) ;
            }
            else
            {
               dwValue += ( *pszStr - 'A' ) + 10 ;
            }
            pszStr++ ;
         }
         pszEnd = pszStr ;
   }

   if ( o_pdwValue != NULL )
   {
      *o_pdwValue = dwValue ;
   }

   return pszEnd ;
}
