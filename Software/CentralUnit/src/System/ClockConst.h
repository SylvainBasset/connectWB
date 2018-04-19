/******************************************************************************/
/*                                                                            */
/*                                ClockConst.h                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   28 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/

                                       /* table of winter to summer day change */
BYTE const k_abyDaySumSpr[] = { 26,25,31,30,28,27,26,25,30,29,28,27,25,31,30,29,27,
                                26,25,31,29,28,27,26,31,30,29,28,26,25,31,30,28,27,
                                26,25,30,29,28,27,25,31,30,29,27,26,25,31,29,28,27,
                                26,31,30,29,28,26,25,31,30,28,27,26,25,30,29,28,27,
                                25,31,30,29,27,26,25,31,29,28,27,26,31,30,29,28,26,
                                25,31,30,28,27,26,25,30,29,28,27,25,31,30,29 } ;

                                       /* table of summer to winter day change */
BYTE const k_abyDaySumAut[] = { 29,28,27,26,31,30,29,28,26,25,31,30,28,27,26,25,30,
                                29,28,27,25,31,30,29,27,26,25,31,29,28,27,26,31,30,
                                29,28,26,25,31,30,28,27,26,25,30,29,28,27,25,31,30,
                                29,27,26,25,31,29,28,27,26,31,30,29,28,26,25,31,30,
                                28,27,26,25,30,29,28,27,25,31,30,29,27,26,25,31,29,
                                28,27,26,31,30,29,28,26,25,31,30,28,27,26,25 } ;

#define CB  MAKEBYTE
                                       /* table of weekday of all month first days */
BYTE const k_abyFirstDayOfMonth[] = {
   CB(5,1), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4),    /* 0 1/2000 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6),    /* 9 7/2001 */
   CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(0,3), CB(5,1),    /* 18 1/2003 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 27 7/2004 */
   CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4),    /* 36 1/2006 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 45 7/2007 */
   CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1),    /* 54 1/2009 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 63 7/2010 */
   CB(6,2), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5),    /* 72 1/2012 */
   CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 81 7/2013 */
   CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(1,4), CB(6,2),    /* 90 1/2015 */
   CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4),    /* 99 7/2016 */
   CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5),    /* 108 1/2018 */
   CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1),    /* 117 7/2019 */
   CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2),    /* 126 1/2021 */
   CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4),    /* 135 7/2022 */
   CB(0,3), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6),    /* 144 1/2024 */
   CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1),    /* 153 7/2025 */
   CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(2,5), CB(0,3),    /* 162 1/2027 */
   CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5),    /* 171 7/2028 */
   CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6),    /* 180 1/2030 */
   CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2),    /* 189 7/2031 */
   CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3),    /* 198 1/2033 */
   CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5),    /* 207 7/2034 */
   CB(1,4), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0),    /* 216 1/2036 */
   CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2),    /* 225 7/2037 */
   CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(3,6), CB(1,4),    /* 234 1/2039 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6),    /* 243 7/2040 */
   CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0),    /* 252 1/2042 */
   CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 261 7/2043 */
   CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4),    /* 270 1/2045 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6),    /* 279 7/2046 */
   CB(2,5), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1),    /* 288 1/2048 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 297 7/2049 */
   CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(4,0), CB(2,5),    /* 306 1/2051 */
   CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 315 7/2052 */
   CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1),    /* 324 1/2054 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4),    /* 333 7/2055 */
   CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5),    /* 342 1/2057 */
   CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 351 7/2058 */
   CB(3,6), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2),    /* 360 1/2060 */
   CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4),    /* 369 7/2061 */
   CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(5,1), CB(3,6),    /* 378 1/2063 */
   CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1),    /* 387 7/2064 */
   CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2),    /* 396 1/2066 */
   CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5),    /* 405 7/2067 */
   CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6),    /* 414 1/2069 */
   CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1),    /* 423 7/2070 */
   CB(4,0), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3),    /* 432 1/2072 */
   CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5),    /* 441 7/2073 */
   CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(6,2), CB(4,0),    /* 450 1/2075 */
   CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2),    /* 459 7/2076 */
   CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3), CB(6,2), CB(2,5), CB(0,3),    /* 468 1/2078 */
   CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6),    /* 477 7/2079 */
   CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(6,2), CB(4,0),    /* 486 1/2081 */
   CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1), CB(3,6), CB(2,4), CB(0,2),    /* 495 7/2082 */
   CB(5,1), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4),    /* 504 1/2084 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5), CB(0,3), CB(6,1), CB(4,6),    /* 513 7/2085 */
   CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0), CB(3,6), CB(0,3), CB(5,1),    /* 522 1/2087 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 531 7/2088 */
   CB(6,2), CB(2,5), CB(0,3), CB(5,1), CB(4,6), CB(2,4), CB(0,3), CB(3,6), CB(1,4),    /* 540 1/2090 */
   CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 549 7/2091 */
   CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1), CB(4,0), CB(0,3), CB(5,1),    /* 558 1/2093 */
   CB(3,6), CB(2,4), CB(0,2), CB(5,1), CB(1,4), CB(6,2), CB(4,0), CB(3,5), CB(1,3),    /* 567 7/2094 */
   CB(6,2), CB(3,6), CB(1,4), CB(6,2), CB(5,0), CB(3,5), CB(1,4), CB(4,0), CB(2,5),    /* 576 1/2096 */
   CB(0,3), CB(6,1), CB(4,6), CB(2,5), CB(5,1), CB(3,6), CB(1,4), CB(0,2), CB(5,0),    /* 585 7/2097 */
   CB(3,6), CB(6,2), CB(4,0), CB(2,5), CB(1,3), CB(6,1)                                /* 594 1/2099 */
} ;