/******************************************************************************/
/*                                  Define.h                                  */
/******************************************************************************/
/*
   general header

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


#ifndef __DEFINE_H                     /* to prevent recursive inclusion */
#define __DEFINE_H


#include <string.h>

#include <stm32l0xx_hal.h>


/*----------------------------------------------------------------------------*/
/* Standard types                                                             */
/*----------------------------------------------------------------------------*/

typedef unsigned long int DWORD ;   /* unsigned 32 bits (dw) */
typedef signed long int SDWORD ;    /* signed 32 bits (sdw) */

typedef unsigned short int WORD ;   /* unsigned 16 bits (w) */
typedef signed short int SWORD ;    /* signed 16 bits (sw) */

typedef unsigned char BYTE ;        /* unsigned 8 bits (by) */
typedef signed char SBYTE ;         /* signed 8 bits (sby) */

typedef unsigned char BOOL ;        /* boolean (b) */

typedef char CHAR ;                 /* character (c) */

typedef float FLOAT ;               /* 32 bits float (f) */
typedef double DOUBLE ;             /* 64 bits float (fd) */

typedef BYTE RESULT ;               /* unsigned 8 bits return statut (r) */


/*-------------------------------------------------------------------------*/
/* Standard types min/max                                                  */
/*-------------------------------------------------------------------------*/

                                    /* min/max for 64 bits types */
#define QWORD_MAX   ((QWORD)0xFFFFFFFFFFFFFFFF)
#define SQWORD_MIN  ((SQWORD)0x8000000000000000)
#define SQWORD_MAX  ((SQWORD)0x7FFFFFFFFFFFFFFF)
                                    /* min/max for 32 bits types */
#define DWORD_MAX   ((DWORD)0xFFFFFFFF)
#define SDWORD_MIN  ((SDWORD)0x80000000)
#define SDWORD_MAX  ((SDWORD)0x7FFFFFFF)

#define WORD_MAX    ((WORD)0xFFFFu) /* min/max for 16 bits types */
#define SWORD_MIN   ((SWORD)0x8000)
#define SWORD_MAX   ((SWORD)0x7FFF)

#define BYTE_MAX    ((BYTE)0xFF)    /* min/max for 8 bits types */
#define SBYTE_MIN   ((SBYTE)0x80)
#define SBYTE_MAX   ((SBYTE)0x7F


/*-------------------------------------------------------------------------*/
/* Standard types constants                                                */
/*-------------------------------------------------------------------------*/

#define FALSE  0                    /* boolean false (BOOL) */
#define TRUE   1                    /* boolean true (BOOL) */

#define OK     0                    /* correct statut (RESULT) */
#define ERR    1                    /* error statut (RESULT) */



/*-------------------------------------------------------------------------*/
/* Standard macro                                                          */
/*-------------------------------------------------------------------------*/

#define C   const                   /* constant keyword */

                                    /* test if one of bit in set <Bits> is set in <Value> */
#define ISSET( Val, Bits )             ( ( (Val) & (Bits) ) != 0 )

                                    /* return elements number for an array */
#define ARRAY_SIZE( Array )            ( sizeof( Array ) / sizeof( Array[0] ) )

                                    /* return next index value in a round buffer array */
#define NEXTIDX( Idx, Array )          ( ( (Idx) < ARRAY_SIZE(Array) - 1 ) ? Idx + 1 : 0 )


                                    /* return the 4 lowest bits on a BYTE */
#define LO4B( byValue )                ( (BYTE)( byValue & 0xF ) )
                                    /* return the 4 highest bits on a BYTE */
#define HI4B( byValue )                ( (BYTE)( byValue >> 4 ) )
                                    /* make BYTE from 2 4-bits parts */
#define MAKEBYTE( byLo, byHi )         ( ( (BYTE)( byHi & 0xFu ) << 4 ) | (BYTE)( byLo & 0xFu ) )

                                    /* return the 8 lowest bits on a WORD */
#define LOBYTE( wValue )               ( (BYTE)( wValue & 0xFF ) )
                                    /* return the 8 highest bits on a WORD */
#define HIBYTE( wValue )               ( (BYTE)( wValue >> 8 ) )
                                    /* make WORD from 2 BYTE parts */
#define MAKEWORD( byLo, byHi )         ( ( (WORD)( byHi & 0xFFu ) << 8 ) | (WORD)( byLo & 0xFFu ) )

                                    /* return the 16 lowest bits on a DWORD */
#define LOWORD( wValue )               ( (WORD)( wValue & 0xFFFF ) )
                                    /* return the 16 highest bits on a DWORD */
#define HIWORD( wValue )               ( (WORD)( wValue >> 16 ) )
                                    /* make DWORD from 2 WORD parts */
#define MAKEDWORD( wLo, wHi )          ( ( (DWORD)( wHi & 0xFFFFu ) << 16 ) | (DWORD)( wLo & 0xFFFFu ) )

                                 /* get minimum between <Val1> and <Val1> */
#define GETMIN( Val1, Val2 )           ( ( (Val1) < (Val2) ) ? (Val1) : (Val2) )
                                 /* get maximum between <Val1> and <Val2> */
#define GETMAX( Val1, Val2 )           ( ( (Val1) < (Val2) ) ? (Val2) : (Val1) )

                                 /* compute absolute difference */
#define ABS_DIFF(Val1, Val2 )          ( ( Val1 > Val2 ) ? ( Val1 - Val2 ) : ( Val2 - Val1 ) )

                                 /* use paramètre in a test, to avoid non-used
                                    parameter warning */
#define USEPARAM( Val )                if ( Val ) {} ;

#define DEFENS_LIM_MAX( Val, Limit )   Val = Val > Limit ? Limit : Val


/*-------------------------------------------------------------------------*/
/* Common defines                                                          */
/*-------------------------------------------------------------------------*/

typedef struct
{
   BYTE byYear ;
   BYTE byMonth ;
   BYTE byDays ;
} s_Date ;                             /* date (day/month/year) structure */

typedef struct
{
   BYTE byHours ;
   BYTE byMinutes ;
   BYTE bySeconds ;
} s_Time ;                             /* time (second/minutes/hours) structure */

typedef struct
{
   union
   {
      struct
      {
         BYTE byYear ;
         BYTE byMonth ;
         BYTE byDays ;
         BYTE byHours ;
         BYTE byMinutes ;
         BYTE bySeconds ;
      } ;
      struct
      {
         s_Date sDate ;
         s_Time sTime ;
      } ;
   } ;
} s_DateTime ;                         /* datetime (date+time) structure */

#endif /* __DEFINE_H */
