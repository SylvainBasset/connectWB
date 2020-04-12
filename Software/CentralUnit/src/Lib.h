/******************************************************************************/
/*                                   Lib.h                                    */
/******************************************************************************/
/*
   Lib header

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
*/

#ifndef __LIB_H                        /* to prevent recursive inclusion */
#define __LIB_H


/*----------------------------------------------------------------------------*/
/* ConvAscii.c                                                                */
/*----------------------------------------------------------------------------*/

char C* cascii_GetNextDec( char C* i_pszStr, SDWORD *o_psdwValue,
                           BOOL i_bIsSigned, DWORD i_dwMax ) ;
char C* cascii_GetNextHex( char C* i_pszStr, DWORD *o_pdwValue ) ;


#endif /* __LIB_H */