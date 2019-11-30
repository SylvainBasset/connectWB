/******************************************************************************/
/*                                 Identity.c                                 */
/******************************************************************************/
/*
   Wallybox Identity

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
   @history 1.0, 18 sept 2018, creation
*/


#include "Define.h"
#include "Main.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define ID_NAME            "WallyBox"
#define ID_VERSION         "1.0"


/*----------------------------------------------------------------------------*/
/* Get decive name                                                            */
/*----------------------------------------------------------------------------*/

char C* id_GetName( void )
{
   return ID_NAME ;
}


/*----------------------------------------------------------------------------*/
/* Get decive version                                                         */
/*----------------------------------------------------------------------------*/

char C* id_GetVersion( void )
{
   return ID_VERSION ;
}