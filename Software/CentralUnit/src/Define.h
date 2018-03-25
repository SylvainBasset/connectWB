/******************************************************************************/
/*                                                                            */
/*                                  Define.h                                  */
/*                                                                            */
/******************************************************************************/
/* Created on:    8 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#ifndef __DEFINE_H                     /* to prevent recursive inclusion */
#define __DEFINE_H


#include <string.h>


/*----------------------------------------------------------------------------*/
/* Standard types                                                             */
/*----------------------------------------------------------------------------*/

typedef unsigned long int DWORD ;      /* dw  unsigned 32 bits */
typedef signed long int SDWORD ;       /* sdw signed 32 bits */

typedef unsigned short int WORD ;      /* w   unsigned 16 bits */
typedef signed short int SWORD ;       /* sw  signed 16 bits */

typedef unsigned char BYTE ;           /* by   unsigned 8 bits */
typedef signed char SBYTE ;            /* sby  signed 8 bits */

typedef unsigned char BOOL ;           /* b   boolean */

typedef char CHAR ;                    /* c   character */

typedef float FLOAT ;                  /* f   32 bits float */
typedef double DOUBLE ;                /* fd  64 bits float */

typedef BYTE RESULT ;               /* r   unsigned 8 bits for execution
                                              return statut, OK, ERR, xx_R_yyy */


/*-------------------------------------------------------------------------*/
/* Standard types min/max                                                  */
/*-------------------------------------------------------------------------*/

#define QWORD_MAX   ((QWORD)0xFFFFFFFFFFFFFFFFuLL)  /* 64 bits */
#define SQWORD_MIN  ((SQWORD)0x8000000000000000LL)
#define SQWORD_MAX  ((SQWORD)0x7FFFFFFFFFFFFFFFLL)

#define DWORD_MAX   ((DWORD)0xFFFFFFFFu)            /* 32 bits */
#define SDWORD_MIN  ((SDWORD)0x80000000)
#define SDWORD_MAX  ((SDWORD)0x7FFFFFFF)

#define WORD_MAX    ((WORD)0xFFFFu)                 /* 16 bits */
#define SWORD_MIN   ((SWORD)0x8000)
#define SWORD_MAX   ((SWORD)0x7FFF)

#define BYTE_MAX    ((BYTE)0xFFu)                   /*  8 bits */
#define SBYTE_MIN   ((SBYTE)0x80)
#define SBYTE_MAX   ((SBYTE)0x7F


/*-------------------------------------------------------------------------*/
/* Standard types constants                                                */
/*-------------------------------------------------------------------------*/

#ifndef FALSE
#define FALSE  0                 /* boolean false (BOOL) */
#endif /* FALSE */

#ifndef TRUE
#define TRUE   1                 /* boolean true (BOOL) */
#endif /* TRUE */

#ifndef OK
#define OK     0                 /* correct statut (RESULT) */
#endif /* OK */

#ifndef ERR
#define ERR    1                 /* error statut (RESULT) */
#endif /* ERR */


/*-------------------------------------------------------------------------*/
/* Standard macro                                                          */
/*-------------------------------------------------------------------------*/

#ifndef NULL
#define NULL   0                 /* null pointer */
#endif

//#define C   const                /* constant keyword */

                                 /* elements number of <Array> */
#define ARRAY_SIZE( Array )  ( sizeof(Array) / sizeof((Array)[0]) )

                                 /* next <Idx> value in <Array> table with
                                    round buffer */
#define NEXTIDX( Idx, Array ) \
   ( ( (Idx) >= ARRAY_SIZE(Array) - 1 ) ? 0 : (Idx) + 1 )

                                 /* test if bit <Msk> is set in <Value>, or test
                                    if bit one of <Msk> bits os set in <Value> */
#define ISSET( Value, Msk )  ( ( (Value) & (Msk) ) != 0 )


                                 /* 8 bits LSB of 16 bits value <wValue> */
#define LOBYTE( wValue )    ( (BYTE)( (WORD)(wValue) & 0xFFu ) )
                                 /* 8 bits MSB of 16 bits value <wValue> */
#define HIBYTE( wValue )    ( (BYTE)( (WORD)(wValue) >> 8u ) )
                                 /* makes 16 bits word with <byLow> and <byHigh>
                                    8 bits values */
#define MAKEWORD( byLow, byHigh ) \
   ( ( ( (WORD)(byHigh) & 0xFFu ) << 8u ) | ( (WORD)(byLow) & 0xFFu ) )


                                 /* 16 bits LSB of 32 bits value <dwValue> */
#define LOWORD( dwValue )   ( (WORD)( (DWORD)(dwValue) & 0xFFFFu ) )
                                 /* 16 bits MSB of 32 bits value <dwValue> */
#define HIWORD( dwValue )   ( (WORD)( (DWORD)(dwValue) >> 16u ) )
                                 /* makes 32 bits word with <wLow> and <wHigh>
                                    16 bits values */
#define MAKEDWORD( wLow, wHigh ) \
   ( ( (DWORD)( (WORD)(wHigh) ) << 16u ) | (DWORD)( (WORD)(wLow) ) )


                                 /* get minimum between <Value1> and <Value2> */
#define MIN( Value1, Value2 ) \
   ( ( (Value1) < (Value2) ) ? (Value1) : (Value2) )
                                 /* get maximum between <Value1> and <Value2> */
#define MAX( Value1, Value2 ) \
   ( ( (Value1) > (Value2) ) ? (Value1) : (Value2) )

#define REFPARM( Value ) \
   if ( Value ) {} ;

#endif /* __DEFINE_H */
