/******************************************************************************/
/*                                 l_Communic.h                               */
/******************************************************************************/
/*
   local communic header

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
   @history 1.0, 17 jun. 2018, creation
*/


/*----------------------------------------------------------------------------*/
/* CommWifi.c                                                                 */
/*----------------------------------------------------------------------------*/

#define CWIFI_W_NULL( NameUp, NameLo, Numb, Var, Value )

#define CWIFI_W_ENUM( NameUp, NameLo, Numb, Var, Value ) CWIFI_WIND_##NameUp,

#define CWIFI_W_CALLBACK( NameUp, NameLo, Numb, Var, Value ) \
   static RESULT cwifi_WindCallBack##NameLo( char C* i_pszProcData, BOOL i_bPendingData ) ;

#define CWIFI_W_OPER( NameUp, NameLo, Numb, Var, Value ) \
   { .szWindNum = Numb, .pbVar = Var, .bValue = Value,   \
     .pszStrContent = NULL, .wContentSize=0,             \
     .fCallback = NULL },

#define CWIFI_W_OPER_R( NameUp, NameLo, Numb, Var, Value )                        \
   { .szWindNum = Numb, .pbVar = Var, .bValue = Value,                            \
     .pszStrContent = l_szWind##NameLo, .wContentSize = sizeof(l_szWind##NameLo), \
     .fCallback = NULL },

#define CWIFI_W_OPER_F( NameUp, NameLo, Numb, Var, Value ) \
   { .szWindNum = Numb, .pbVar = Var, .bValue = Value,     \
     .pszStrContent = NULL, .wContentSize=0,               \
     .fCallback = &cwifi_WindCallBack##NameLo },

//
/*
#define CWIFI_W_OPER_RF( NameUp, NameLo, Numb, Var, Value )                       \
   { .szWindNum = Numb, .pbVar = Var, .bValue = Value,                            \
     .pszStrContent = l_szWind##NameLo, .wContentSize = sizeof(l_szWind##NameLo), \
     .fCallback = &cwifi_WindCallBack##NameLo },
*/

#define CWIFI_C_NULL( NameUp, NameLo, CmdFmt, IsResult )

#define CWIFI_C_ENUM( NameUp, NameLo, CmdFmt, IsResult ) CWIFI_CMD_##NameUp,

#define CWIFI_C_CALLBACK( NameUp, NameLo, CmdFmt, IsResult ) \
   static RESULT cwifi_CmdCallBack##NameLo( char C* i_pszProcData ) ;

#define CWIFI_C_OPER( NameUp, NameLo, CmdFmt, IsResult )  \
   { .szCmdFmt = CmdFmt, .bIsResult = IsResult, .pszStrContent = NULL, \
     .wContentSize=0, .fCallback = NULL },

#define CWIFI_C_OPER_R( NameUp, NameLo, CmdFmt, IsResult )            \
   { .szCmdFmt = CmdFmt, .bIsResult = IsResult, .pszStrContent = l_szResp##NameLo, \
     .wContentSize=sizeof(l_szResp##NameLo), .fCallback = NULL },

#define CWIFI_C_OPER_F( NameUp, NameLo, CmdFmt, IsResult ) \
   { .szCmdFmt = CmdFmt, .bIsResult = IsResult, .pszStrContent = NULL,  \
     .wContentSize=0, .fCallback = cwifi_CmdCallBack##NameLo },
//
/*
#define CWIFI_C_OPER_RF( NameUp, NameLo, CmdFmt, IsResult )           \
   { .szCmdFmt = CmdFmt, .bIsResult = IsResult, .pszStrContent = l_szResp##NameLo, \
     .wContentSize=sizeof(l_szResp##NameLo), .fCallback = cwifi_CmdCallBack##NameLo },
*/


/*----------------------------------------------------------------------------*/
/* CommOEvse.c                                                                 */
/*----------------------------------------------------------------------------*/

#define COEVSE_CMD_NULL( NameUp, NameLo, StrCmd, StrCk )

#define COEVSE_CMD_ENUM( NameUp, NameLo, StrCmd, StrCk ) COEVSE_CMD_##NameUp,

#define COEVSE_CMD_CALLBACK( NameUp, NameLo, StrCmd, StrCk ) \
   static void coevse_Cmdresult##NameLo( char C* i_pszDataRes ) ;

#define COEVSE_CMD_DESC( NameUp, NameLo, StrCmd, StrCk ) \
   { .eCmdId = COEVSE_CMD_##NameUp, .szFmtCmd = (StrCmd), \
     .szChecksum = (StrCk), .fResultCallback = NULL },

#define COEVSE_CMD_DESC_G( NameUp, NameLo, StrCmd, StrCk ) \
   { .eCmdId = COEVSE_CMD_##NameUp, .szFmtCmd = (StrCmd), \
     .szChecksum = (StrCk), .fResultCallback = coevse_Cmdresult##NameLo },
