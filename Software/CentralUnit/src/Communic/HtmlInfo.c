/******************************************************************************/
/*                                                                            */
/*                                  HtmlInfo.c                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   24 dec. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Control.h"
#include "Communic.h"
#include "System.h"



static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize ) ;
static void html_ProcessSsiCharge( DWORD i_dwParam2,
                                   char * o_pszOutput, WORD i_wStrSize ) ;
static void html_ProcessSsiCalendar( DWORD i_dwParam2, char * o_pszOut, WORD i_wStrSize ) ;
static void html_ProcessSsiWifi( DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize ) ;

static void html_ProcessCgi( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiCharge( DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiCalendar( DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiWifi( DWORD i_dwParam2, char C* i_pszValue ) ;

static CHAR * html_AddToStr( CHAR * o_pszString, WORD * io_pwStrSize, CHAR C* i_szStrToAdd ) ;

/*----------------------------------------------------------------------------*/

void html_Init( void )
{
   cwifi_RegisterHtmlFunc( &html_ProcessSsi, &html_ProcessCgi ) ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize )
{
   switch ( i_dwParam1 )
   {
      case HTML_PAGE_STATUS :
         html_ProcessSsiCharge( i_dwParam2, o_pszOutput, i_wStrSize ) ;
         break ;

      case HTML_PAGE_CALENDAR :
         html_ProcessSsiCalendar( i_dwParam2, o_pszOutput, i_wStrSize ) ;
         break ;

      case HTML_PAGE_WIFI :
         html_ProcessSsiWifi( i_dwParam2, o_pszOutput, i_wStrSize ) ;
         break ;

      default :
         *o_pszOutput = 0 ;
         break ;
   }
}

/*----------------------------------------------------------------------------*/

static void html_ProcessSsiCharge( DWORD i_dwParam2,
                                   char * o_pszOutput, WORD i_wStrSize )
{
   e_cstateForceSt eForceStatus ;
   WORD wStrSize ;
   e_cstateChargeSt eChargeState ;
   e_coevseEVPlugState ePlugState ;
   DWORD dwCurrent ;
   SDWORD sdwCurrent ;

   wStrSize = i_wStrSize ;

   switch ( i_dwParam2 )
   {
      case HTML_CHARGE_SSI_FORCE :
         eForceStatus = cstate_GetForceState() ;
         if ( eForceStatus == CSTATE_FORCE_AMPMIN )
         {
            html_AddToStr( o_pszOutput, &wStrSize,
                           "<FONT COLOR=\"#0000C0\">Oui, Condition courant minimum ignor&eacute;</FONT>" ) ;
         }
         else if ( eForceStatus == CSTATE_FORCE_ALL )
         {
            html_AddToStr( o_pszOutput, &wStrSize,
                           "<FONT COLOR=\"#0000C0\">Oui, Charge syst&eacutematique</FONT>" ) ;
         }
         else
         {
            html_AddToStr( o_pszOutput, &wStrSize, "Non" ) ;
         }
         break ;

      case HTML_CHARGE_SSI_CALENDAR_OK :
         if ( cal_IsChargeEnable() )
         {
            html_AddToStr( o_pszOutput, &wStrSize, "<FONT COLOR=\"#00C000\">Oui</FONT>" ) ;
         }
         else
         {
            html_AddToStr( o_pszOutput, &wStrSize, "Non" ) ;
         }
         break ;

      case HTML_CHARGE_SSI_EVPLUGGED :
         ePlugState = coevse_GetPlugState() ;
         if ( ePlugState == COEVSE_EV_PLUGGED )
         {
            html_AddToStr( o_pszOutput, &wStrSize, "<FONT COLOR=\"#00C000\">Oui</FONT>" ) ;
         }
         else if ( ePlugState == COEVSE_EV_UNKNOWN )
         {
            html_AddToStr( o_pszOutput, &wStrSize, "???" ) ;
         }
         else
         {
            html_AddToStr( o_pszOutput, &wStrSize, "Non" ) ;
         }
         break ;

      case HTML_CHARGE_SSI_STATE :
         eChargeState = cstate_GetChargeState() ;
         switch ( eChargeState )
         {
            case CSTATE_OFF :
               html_AddToStr( o_pszOutput, &wStrSize, "En arr&ecirc;t" ) ;
               break ;

            case CSTATE_FORCE_AMPMIN_WAIT_VE :
            case CSTATE_FORCE_ALL_WAIT_VE :
               html_AddToStr( o_pszOutput, &wStrSize,
                              "<FONT COLOR=\"#0000C0\">Forc&eacute;, en attente VE</FONT>" ) ;
               break ;

            case CSTATE_DATE_TIME_LOST :
               html_AddToStr( o_pszOutput, &wStrSize,
                              "<FONT COLOR=\"#C00000\">Date/heure perdue</FONT>" ) ;
               break ;

            case CSTATE_WAIT_CALENDAR :
               html_AddToStr( o_pszOutput, &wStrSize, "Attente heure de charge" ) ;
               break ;

            case CSTATE_CHARGING :
               html_AddToStr( o_pszOutput, &wStrSize, "<FONT COLOR=\"#00C000\">En charge</FONT>" ) ;
               break ;

            case CSTATE_END_OF_CHARGE :
               html_AddToStr( o_pszOutput, &wStrSize, "Charge terminée" ) ;
               break ;

            default :
               html_AddToStr( o_pszOutput, &wStrSize, "En arr&ecirc;t" ) ;
               break ;
         }
         break ;

      case HTML_CHARGE_SSI_CURRENT_MES :
         sdwCurrent = coevse_GetCurrent() ;
         if ( sdwCurrent > 0 )
         {
            dwCurrent = (DWORD)sdwCurrent ;
         }
         else
         {
            dwCurrent = 0 ;
         }
         snprintf( o_pszOutput, i_wStrSize, "%lu", dwCurrent ) ;
         break ;

      case HTML_CHARGE_SSI_CURRENT_CAP :
         snprintf( o_pszOutput, i_wStrSize, "%lu", coevse_GetCurrentCap() ) ;
         break ;

      case HTML_CHARGE_SSI_CURRENT_MIN :
         snprintf( o_pszOutput, i_wStrSize, "%lu", cstate_GetCurrentMinStop() ) ;
         break ;

      default :
          html_AddToStr( o_pszOutput, &wStrSize, "---" ) ;
          break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessSsiCalendar( DWORD i_dwParam2,
                                     char * o_pszOutput, WORD i_wStrSize )
{
   s_DateTime DateTime ;
   BYTE byWeekday ;
   s_Time StartTime ;
   s_Time EndTime ;
   DWORD dwStartTimeCnt ;
   DWORD dwEndTimeCnt ;
   WORD wStrSize ;
   CHAR * pszOutput ;
   CHAR szResFormat[16] ;

   switch ( i_dwParam2 )
   {
      case HTML_CALENDAR_SSI_DATETIME :
         wStrSize = i_wStrSize ;
         pszOutput = o_pszOutput ;
         *pszOutput = 0 ;

         clk_GetDateTime( &DateTime, &byWeekday ) ;

         switch ( byWeekday )
         {
            case 0 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Lundi" ) ;    break ;
            case 1 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Mardi" ) ;    break ;
            case 2 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Mercredi" ) ; break ;
            case 3 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Jeudi" ) ;    break ;
            case 4 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Vendredi" ) ; break ;
            case 5 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Samedi" ) ;   break ;
            case 6 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Dimanche" ) ; break ;
            default : pszOutput = html_AddToStr( pszOutput, &wStrSize, "---" ) ;      break ;
         }
         pszOutput = html_AddToStr( pszOutput, &wStrSize, " " ) ;

         snprintf( szResFormat, sizeof(szResFormat), "%02u", DateTime.byDays ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, szResFormat ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, " " ) ;

         switch ( DateTime.byMonth )
         {
            case 1 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Janvier" ) ;         break ;
            case 2 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "F&eacute;vrier" ) ;  break ;
            case 3 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Mars" ) ;            break ;
            case 4 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Avril" ) ;           break ;
            case 5 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Mai" ) ;             break ;
            case 6 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Juin" ) ;            break ;
            case 7 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Juillet" ) ;         break ;
            case 8 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Ao&ucirc;t" ) ;      break ;
            case 9 :  pszOutput = html_AddToStr( pszOutput, &wStrSize, "Septembre" ) ;       break ;
            case 10 : pszOutput = html_AddToStr( pszOutput, &wStrSize, "Octobre" ) ;         break ;
            case 11 : pszOutput = html_AddToStr( pszOutput, &wStrSize, "Novembre" ) ;        break ;
            case 12 : pszOutput = html_AddToStr( pszOutput, &wStrSize, "D&eacute;cembre" ) ; break ;
            default : pszOutput = html_AddToStr( pszOutput, &wStrSize, "---" ) ;             break ;
         }
         pszOutput = html_AddToStr( pszOutput, &wStrSize, " " ) ;

         snprintf( szResFormat, sizeof(szResFormat), "%04u", ((WORD)DateTime.byYear + 2000 ) ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, szResFormat ) ;

         pszOutput = html_AddToStr( pszOutput, &wStrSize, "&nbsp;&nbsp;&nbsp;" ) ;

         snprintf( szResFormat, sizeof(szResFormat), "%02u", DateTime.byHours ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, szResFormat ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, ":" ) ;
         snprintf( szResFormat, sizeof(szResFormat), "%02u", DateTime.byMinutes ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, szResFormat ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, ":" ) ;
         snprintf( szResFormat, sizeof(szResFormat), "%02u", DateTime.bySeconds ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, szResFormat ) ;

         break ;


      case HTML_CALENDAR_SSI_CAL_MONDAY :
      case HTML_CALENDAR_SSI_CAL_TUESDAY :
      case HTML_CALENDAR_SSI_CAL_WEDNESDAY :
      case HTML_CALENDAR_SSI_CAL_THURSDAY :
      case HTML_CALENDAR_SSI_CAL_FRIDAY :
      case HTML_CALENDAR_SSI_CAL_SATURDAY :
      case HTML_CALENDAR_SSI_CAL_SUNDAY :
         byWeekday = i_dwParam2 - HTML_CALENDAR_SSI_CAL_MONDAY ;
         cal_GetDayVals( byWeekday, &StartTime, &EndTime, &dwStartTimeCnt, &dwEndTimeCnt ) ;

         if ( dwStartTimeCnt < dwEndTimeCnt )
         {
            snprintf( o_pszOutput, i_wStrSize,
                      "<TD>de <b>%02u:%02u</b></TD><TD>&agrave; <b>%02u:%02u</b></TD>",
                      StartTime.byHours, StartTime.byMinutes,
                      EndTime.byHours, EndTime.byMinutes ) ;
         }
         else if ( dwStartTimeCnt > dwEndTimeCnt )
         {
            if ( dwEndTimeCnt == 0 )
            {
               snprintf( o_pszOutput, i_wStrSize,
                         "<TD>de <b>%02u:%02u</b></TD><TD>&agrave; <b>minuit</b></TD>",
                         StartTime.byHours, StartTime.byMinutes ) ;
            }
            else
            {
               snprintf( o_pszOutput, i_wStrSize,
                         "<TD>de <b>minuit</b></TD><TD>&agrave; <b>%02u:%02u</b></TD>" \
                         "<TD>et de <b>%02u:%02u</b></TD><TD>&agrave; <b>minuit</b></TD>",
                         EndTime.byHours, EndTime.byMinutes,
                         StartTime.byHours, StartTime.byMinutes ) ;
            }
         }
         else
         {
            strncpy( o_pszOutput, "<TD><b>Off</b></TD>", i_wStrSize ) ;
         }
         break ;

      default :
          strncpy( o_pszOutput, "---", i_wStrSize ) ;
          break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessSsiWifi( DWORD i_dwParam2,
                                 char * o_pszOutput, WORD i_wStrSize )
{
   char * pszWifiSSID ;
   WORD wStrSize ;
   CHAR * pszOutput ;

   wStrSize = i_wStrSize ;
   pszOutput = o_pszOutput ;

   switch ( i_dwParam2 )
   {
      case HTML_WIFI_SSI_WIFIHOME :
         pszOutput = html_AddToStr( pszOutput, &wStrSize, "<b>" ) ;
         pszWifiSSID = g_sDataEeprom->sWifiConInfo.szWifiSSID ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, pszWifiSSID ) ;
         pszOutput = html_AddToStr( pszOutput, &wStrSize, "</b>" ) ;
         break ;

      case HTML_WIFI_SSI_MAINTMODE :
         if ( cwifi_IsMaintMode() )
         {
            html_AddToStr( o_pszOutput, &i_wStrSize, "<b>oui</b>" ) ;
         }
         else
         {
            html_AddToStr( o_pszOutput, &i_wStrSize, "<b>non</b>" ) ;
         }
         break ;

      default :
          strncpy( o_pszOutput, "---", i_wStrSize ) ;
          break ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void html_ProcessCgi( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue )
{
   switch ( i_dwParam1 )
   {
      case HTML_PAGE_STATUS :
         html_ProcessCgiCharge( i_dwParam2, i_pszValue ) ;
         break ;

      case HTML_PAGE_CALENDAR :
         html_ProcessCgiCalendar( i_dwParam2, i_pszValue ) ;
         break ;

      case HTML_PAGE_WIFI :
         html_ProcessCgiWifi( i_dwParam2, i_pszValue ) ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessCgiCharge( DWORD i_dwParam2, char C* i_pszValue )
{
   WORD wCurrentCapMax ;
   WORD wCurrentMin ;
   BYTE byNbScan ;

   switch ( i_dwParam2 )
   {
      case HTML_CHARGE_CGI_CURRENT_CAPMAX :
         byNbScan = sscanf( i_pszValue, "%hu", &wCurrentCapMax ) ;

         if ( ( byNbScan == 1 ) &&
              ( wCurrentCapMax >= COEVSE_CURRENT_CAPMAX_MIN ) &&
              ( wCurrentCapMax <= COEVSE_CURRENT_CAPMAX_MAX ) )
         {
            coevse_SetCurrentCap( wCurrentCapMax ) ;
         }
         break ;

      case HTML_CHARGE_CGI_CURRENT_MIN :
         byNbScan = sscanf( i_pszValue, "%hu", &wCurrentMin ) ;

         if ( ( byNbScan == 1 ) &&
              ( wCurrentMin >= CSTATE_CURRENT_MIN_MIN ) &&
              ( wCurrentMin <= CSTATE_CURRENT_MIN_MAX ) )
         {
            cstate_SetCurrentMinStop( wCurrentMin ) ;
         } ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessCgiCalendar( DWORD i_dwParam2, char C* i_pszValue )
{
   s_DateTime DateTime ;
   BYTE byDayStartSet ;
   BYTE byDayEndSet ;
   BYTE byDay ;
   s_Time TimeStart ;
   s_Time TimeEnd ;
   BYTE byNbScan ;

   unsigned int uiDays ;
   unsigned int uiMonth ;
   unsigned int uiYear ;
   unsigned int uiHours ;
   unsigned int uiMinutes ;
   unsigned int uiHoursEnd ;
   unsigned int uiMinutesEnd ;

   switch ( i_dwParam2 )
   {
      case HTML_CALENDAR_CGI_DATE :
         byNbScan = sscanf( i_pszValue, "%u,%u,%u,%u,%u", &uiDays, &uiMonth, &uiYear,
                                                          &uiHours, &uiMinutes ) ;
         DateTime.byYear = uiYear - 2000 ;
         DateTime.byMonth = uiMonth ;
         DateTime.byDays = uiDays ;
         DateTime.byHours = uiHours ;
         DateTime.byMinutes = uiMinutes ;
         DateTime.bySeconds = 0 ;

         if ( ( byNbScan == 5 ) && clk_IsValid( &DateTime ) )
         {
            clk_SetDateTime( &DateTime ) ;
         }
         //TODO: voir si affichage message erreur avec un SSI
         break ;

      case HTML_CALENDAR_CGI_DAYSET :
         sscanf( i_pszValue, "%u,%u,%u,%u,%u", &uiDays, &uiHours, &uiMinutes,
                                               &uiHoursEnd, &uiMinutesEnd ) ;
         switch( uiDays )
         {
            case 7 :
               byDayStartSet = 0 ;
               byDayEndSet = 6 ;
               break ;
            case 8 :
               byDayStartSet = 0 ;
               byDayEndSet = 4 ;
               break ;
            case 9 :
               byDayStartSet = 5 ;
               byDayEndSet = 6 ;
               break ;
            default :
               byDayStartSet = uiDays ;
               byDayEndSet = uiDays ;
               break ;
         }

         TimeStart.byHours = uiHours ;
         TimeStart.byMinutes = uiMinutes ;
         TimeStart.bySeconds = 0 ;
         TimeEnd.byHours = uiHoursEnd ;
         TimeEnd.byMinutes = uiMinutesEnd ;
         TimeEnd.bySeconds = 0 ;

         if ( cal_IsValid( &TimeStart, &TimeEnd ) )
         {
            for ( byDay = byDayStartSet ; byDay <= byDayEndSet ; byDay++ )
            {
               cal_SetDayVals( byDay, &TimeStart, &TimeEnd ) ;
            }
         }
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessCgiWifi( DWORD i_dwParam2, char C* i_pszValue )
{
   char szSsid[128] ;
   char szPwd[128] ;
   BYTE byIdxSsid ;
   BYTE byIdxPwd ;
   char C* pszValue ;
   BOOL bSsid ;

   switch ( i_dwParam2 )
   {
      case HTML_WIFI_CGI_WIFI :
         bSsid = TRUE ;
         byIdxSsid = 0 ;
         byIdxPwd = 0 ;
         pszValue = i_pszValue ;
         while ( ( *pszValue != 0 ) && ( *pszValue != '\r' ) && ( *pszValue != '\n' ) )
         {
            if ( *pszValue == ',' )
            {
               pszValue++ ;
               bSsid = FALSE ;
               continue ;
            }

            if ( bSsid )
            {
               if ( byIdxSsid < sizeof(szSsid) )
               {
                  szSsid[byIdxSsid] = *pszValue ;
                  byIdxSsid++ ;
               }
            }
            else
            {
               if ( byIdxPwd < sizeof(szPwd) )
               {
                  szPwd[byIdxPwd] = *pszValue ;
                  byIdxPwd++ ;
               }
            }
            pszValue++ ;
         }

         szSsid[byIdxSsid] = 0 ;
         szPwd[byIdxPwd] = 0 ;

         sfrm_WriteWifiId( TRUE, szSsid ) ;
         sfrm_WriteWifiId( FALSE, szPwd ) ;
         cwifi_SetMaintMode( FALSE ) ;
         break ;

      default :
         break ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static CHAR * html_AddToStr( CHAR * o_pszString, WORD * io_pwStrSize, CHAR C* i_szStrToAdd )
{
   CHAR * pszEnd ;
   BYTE byLenToAdd ;

   byLenToAdd = strlen(i_szStrToAdd) ;
   pszEnd = o_pszString ;

   if ( *io_pwStrSize > byLenToAdd )
   {
      strncpy( o_pszString, i_szStrToAdd, *io_pwStrSize ) ;

      *io_pwStrSize -= strlen(i_szStrToAdd) ;

      pszEnd += strlen(i_szStrToAdd) ;
   }

   return pszEnd ;
}
