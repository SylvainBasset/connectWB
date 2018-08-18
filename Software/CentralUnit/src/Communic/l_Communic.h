/******************************************************************************/
/*                                                                            */
/*                                 l_Communic.h                               */
/*                                                                            */
/******************************************************************************/
/* Created on:   17 jun. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/



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
