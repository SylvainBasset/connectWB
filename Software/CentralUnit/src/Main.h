/******************************************************************************/
/*                                   Main.h                                   */
/******************************************************************************/
/*
   main header

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
   @history 1.0, 08 mar. 2018, creation
*/


#ifndef __MAIN_H                       /* to prevent recursive inclusion */
#define __MAIN_H


/*----------------------------------------------------------------------------*/
/* Main.c                                                                     */
/*----------------------------------------------------------------------------*/

#define TASKCALL_PER_MS  10            /* tasks call period, ms */


/*----------------------------------------------------------------------------*/
/* Identity.c                                                                 */
/*----------------------------------------------------------------------------*/

char C* id_GetName( void ) ;
char C* id_GetVersion( void ) ;


#endif /* __MAIN_H */