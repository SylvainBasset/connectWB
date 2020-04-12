/******************************************************************************/
/*                                  HtmlInfo.c                                */
/******************************************************************************/
/*
   Html information control module

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
   @history 1.0, 24 dec. 2018, creation
   @brief
   Implement SSI (information display) and CGI (settings) treatment
   for HTML page information controle

   SSI are managed by html_ProcessSsi(), CGI by html_ProcessCgi().

   Information to return is identified by i_dwParam1 and i_dwParam2 parameters,
   and formatted to <o_pszOutput> with respect to <i_wStrSize> size.

   Both functions are registered to CommWifi.c module via cwifi_RegisterHtmlFunc().
   They are automatically called by CommWifi.c at SSI/CGI reception.

   i_dwParam1 identifies the HTML web page, it may contain HTML_PAGE_... values
   i_dwParam2 identifies the data inside web page, it may contain
   HTML_{CHARGE,CALENDAR-WIFI}_{GCI,SSI}_... values

   see CCI/SSI defines section.
*/


#include "Define.h"
#include "Control.h"
#include "Communic.h"
#include "System.h"


/*----------------------------------------------------------------------------*/
/* defines                                                                    */
/*----------------------------------------------------------------------------*/

#define HTML_COL_RED     "<FONT COLOR=\"#C00000\">"
#define HTML_COL_BLUE    "<FONT COLOR=\"#0000C0\">"
#define HTML_COL_GREEN   "<FONT COLOR=\"#00C000\">"

#define HTML_COL_END     "</FONT>"


/*----------------------------------------------------------------------------*/
/* CCI/SSI defines section.                                                   */
/*----------------------------------------------------------------------------*/

#define HTML_PAGE_CHARGE                 0  /* charge HTML page */
#define HTML_PAGE_CALENDAR               1  /* calendar HTML page */
#define HTML_PAGE_WIFI                   2  /* wifi HTML page */

#define HTML_CHARGE_SSI_FORCE            0  /* SSI in charge HTML page : forced mode */
#define HTML_CHARGE_SSI_CALENDAR_OK      1  /* SSI in charge HTML page : charge is allowed in this time period */
#define HTML_CHARGE_SSI_EVPLUGGED        2  /* SSI in charge HTML page : EV is plugged */
#define HTML_CHARGE_SSI_STATE            3  /* SSI in charge HTML page : charge state */
#define HTML_CHARGE_SSI_CURRENT_MES      4  /* SSI in charge HTML page : charge current measurement in mA */
#define HTML_CHARGE_SSI_VOLTAGE_MES      5  /* SSI in charge HTML page : charge voltage measurement in mV */
#define HTML_CHARGE_SSI_ENERGY_MES       6  /* SSI in charge HTML page : energy consumption in Wh */
#define HTML_CHARGE_SSI_CURRENT_CAP      7  /* SSI in charge HTML page : maximum current capacity */
#define HTML_CHARGE_SSI_CURRENT_MIN      8  /* SSI in charge HTML page : minimum current to stop the charge is allowed */

#define HTML_CALENDAR_SSI_DATETIME       0  /* SSI in calendar HTML page : current date time in "weekday DD/MM/YYYY HH/MM/SS" format */
#define HTML_CALENDAR_SSI_CAL_MONDAY     1  /* SSI in calendar HTML page : monday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_TUESDAY    2  /* SSI in calendar HTML page : tuesday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_WEDNESDAY  3  /* SSI in calendar HTML page : wednesday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_THURSDAY   4  /* SSI in calendar HTML page : thursday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_FRIDAY     5  /* SSI in calendar HTML page : friday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_SATURDAY   6  /* SSI in calendar HTML page : saturday allowed charge period */
#define HTML_CALENDAR_SSI_CAL_SUNDAY     7  /* SSI in calendar HTML page : sunday allowed charge period */

#define HTML_WIFI_SSI_WIFIHOME           0  /* SSI in wifi HTML page : home wifi SSID */
#define HTML_WIFI_SSI_SECURITY           1  /* SSI in wifi HTML page : home wifi security */
#define HTML_WIFI_SSI_MAINTMODE          2  /* SSI in wifi HTML page : maintenance mode status */

#define HTML_CHARGE_CGI_CURRENT_CAPMAX   0  /* CGI (settings) in charge HTML page : maximum current capacity */
#define HTML_CHARGE_CGI_CURRENT_MIN      1  /* CGI (settings) in charge HTML page : minimum current to stop the charge is allowed */
#define HTML_CHARGE_CGI_TOGGLE_FORCE     2  /* CGI (settings) in charge HTML page : forced mode */

#define HTML_CALENDAR_CGI_DATE           0  /* CGI (settings) in calendar HTML page : date/time */
#define HTML_CALENDAR_CGI_DAYSET         1  /* CGI (settings) in calendar HTML page : allowed charge period for the weekday */

#define HTML_WIFI_CGI_WIFI               0  /* CGI (settings) in Wifi HTML page : home wifi SSID / pwd and securirty type */


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize ) ;
static void html_ProcessSsiCharge( DWORD i_dwParam2,
                                   char * o_pszOutput, WORD i_wStrSize ) ;
static void html_ProcessSsiCalendar( DWORD i_dwParam2, char * o_pszOut, WORD i_wStrSize ) ;
static void html_ProcessSsiWifi( DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize ) ;

static void html_ProcessCgi( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiCharge( DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiCalendar( DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiWifi( DWORD i_dwParam2, char C* i_pszValue ) ;

static void html_DecodUrlChar( CHAR * io_pszString, WORD i_wStrSize ) ;
static CHAR * html_AddToStr( CHAR * o_pszString, WORD * io_pwStrSize, CHAR C* i_szStrToAdd ) ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void html_Init( void )
{
   cwifi_RegisterHtmlFunc( &html_ProcessSsi, &html_ProcessCgi ) ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* SSI dispatch                                                               */
/*----------------------------------------------------------------------------*/

static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_wStrSize )
{
   switch ( i_dwParam1 )
   {
      case HTML_PAGE_CHARGE :
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
/* Charge page SSIs treatment                                                 */
/*----------------------------------------------------------------------------*/

static void html_ProcessSsiCharge( DWORD i_dwParam2,
                                   char * o_pszOutput, WORD i_wStrSize )
{
   e_cstateForceSt eForceStatus ;
   WORD wStrSize ;
   e_cstateChargeSt eChargeState ;
   e_coevseEvseState ePlugState ;
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
         ePlugState = coevse_GetEvseState() ;
         if ( ( ePlugState == COEVSE_STATE_CONNECTED ) || ( ePlugState == COEVSE_STATE_CHARGING ) )
         {
            html_AddToStr( o_pszOutput, &wStrSize, "<FONT COLOR=\"#00C000\">Oui</FONT>" ) ;
         }
         else if ( ePlugState == COEVSE_STATE_UNKNOWN )
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
               if ( clk_IsDateTimeLost() )
               {
                  html_AddToStr( o_pszOutput, &wStrSize,
                                 HTML_COL_RED "Date/heure perdue" HTML_COL_END ) ;
               }
               else
               {
                  html_AddToStr( o_pszOutput, &wStrSize, "En arr&ecirc;t" ) ;
               }
               break ;

            case CSTATE_FORCE_WAIT :
               html_AddToStr( o_pszOutput, &wStrSize,
                              HTML_COL_BLUE "Forc&eacute;, en attente VE" HTML_COL_END ) ;
               break ;

            case CSTATE_ON_WAIT :
               html_AddToStr( o_pszOutput, &wStrSize, "Attente VE" ) ;
               break ;

            case CSTATE_CHARGING :
               html_AddToStr( o_pszOutput, &wStrSize,
                              HTML_COL_GREEN "En charge" HTML_COL_END ) ;
               break ;

            case CSTATE_EOC_LOWCUR :
               html_AddToStr( o_pszOutput, &wStrSize, "Fin de charge par courant min" ) ;
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

      case HTML_CHARGE_SSI_VOLTAGE_MES :
         snprintf( o_pszOutput, i_wStrSize, "%li", coevse_GetVoltage() ) ;
         break ;

      case HTML_CHARGE_SSI_ENERGY_MES :
         snprintf( o_pszOutput, i_wStrSize, "%lu", coevse_GetEnergy() ) ;
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
/* Calendar page SSIs treatment                                               */
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
/* Wifi page SSIs treatment                                                   */
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

      case HTML_WIFI_SSI_SECURITY :
         pszOutput = html_AddToStr( pszOutput, &wStrSize, "<b>" ) ;
         if ( g_sDataEeprom->sWifiConInfo.dwWifiSecurity == 0 )
         {
            pszOutput = html_AddToStr( pszOutput, &wStrSize, "None" ) ;
         }
         else if ( g_sDataEeprom->sWifiConInfo.dwWifiSecurity == 1 )
         {
            pszOutput = html_AddToStr( pszOutput, &wStrSize, "WEP" ) ;
         }
         else
         {
            pszOutput = html_AddToStr( pszOutput, &wStrSize, "WPA" ) ;
         }
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
/* CGI dispatch treatment                                                     */
/*----------------------------------------------------------------------------*/

static void html_ProcessCgi( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue )
{
   switch ( i_dwParam1 )
   {
      case HTML_PAGE_CHARGE :
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
/* Charge page CGIs treatment                                                 */
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
              ( wCurrentMin >= CSTATE_CURRENT_MINSTOP_MIN ) &&
              ( wCurrentMin <= CSTATE_CURRENT_MINSTOP_MAX ) )
         {
            cstate_SetCurrentMinStop( wCurrentMin ) ;
         } ;
         break ;

      case HTML_CHARGE_CGI_TOGGLE_FORCE :
         cstate_ToggleForce() ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/
/* Calendar page CGIs treatment                                               */
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
            clk_SetDateTime( &DateTime, 0 ) ;
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
/* Wifi page CGIs treatment                                                   */
/*----------------------------------------------------------------------------*/

static void html_ProcessCgiWifi( DWORD i_dwParam2, char C* i_pszValue )
{
   char szSsid[128] ;
   char szPwd[128] ;
   BYTE bySecurity ;
   BYTE byIdxSsid ;
   BYTE byIdxPwd ;
   char C* pszValue ;
   BYTE byParam ;

   szSsid[0] = '\0' ;
   szPwd[0] = '\0' ;
   bySecurity = 2 ;

   switch ( i_dwParam2 )
   {
      case HTML_WIFI_CGI_WIFI :
    	 byParam = 0 ;
         byIdxSsid = 0 ;
         byIdxPwd = 0 ;
         pszValue = i_pszValue ;

         while ( ( *pszValue != 0 ) && ( *pszValue != '\r' ) && ( *pszValue != '\n' ) )
         {
            if ( *pszValue == ',' )
            {
               pszValue++ ;
               byParam++ ;
               continue ;
            }

            if ( byParam == 0 )
            {
               if ( byIdxSsid < sizeof(szSsid) )
               {
                  szSsid[byIdxSsid] = *pszValue ;
                  byIdxSsid++ ;
               }
            }
            else if ( byParam == 1 )
            {
               if ( byIdxPwd < sizeof(szPwd) )
               {
                  szPwd[byIdxPwd] = *pszValue ;
                  byIdxPwd++ ;
               }
            }
            else
            {
               if ( ( *pszValue >= '0' ) && ( *pszValue <= '2' ) )
               {
                  bySecurity = *pszValue - '0' ;
               }

            }
            pszValue++ ;
         }

         szSsid[byIdxSsid] = 0 ;
         szPwd[byIdxPwd] = 0 ;

         html_DecodUrlChar( szSsid, sizeof(szSsid) ) ;
         eep_WriteWifiId( TRUE, szSsid ) ;

         html_DecodUrlChar( szPwd, sizeof(szPwd) ) ;
         eep_WriteWifiId( FALSE, szPwd ) ;

         eep_write( (DWORD)&g_sDataEeprom->sWifiConInfo.dwWifiSecurity, (DWORD)bySecurity ) ;
         cwifi_SetMaintMode( FALSE ) ;
         break ;

      default :
         break ;
   }
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Decod URL specific characters (%..)                                        */
/*----------------------------------------------------------------------------*/

static void html_DecodUrlChar( CHAR * io_pszString, WORD i_wStrSize )
{
   WORD i ;
   CHAR * pszStrRaw ;
   CHAR * pszStrDecod ;
   CHAR cDecodedChar ;
   BYTE byDecodIdx ;

   pszStrRaw = io_pszString ;
   pszStrDecod = io_pszString ;

   for ( i = 0 ; i < i_wStrSize ; ++i )
   {
      if ( *pszStrRaw == 0 )
      {
         break ;
      }

      if ( *pszStrRaw == '%' )
      {
         cDecodedChar = 0 ;

         for ( byDecodIdx = 0 ; byDecodIdx < 2 ; byDecodIdx++ )
         {
            pszStrRaw++ ;
            cDecodedChar *= 16 ;
            if ( *pszStrRaw == 0 )
            {
               break ;
            }
            if ( *pszStrRaw <= '9' )
            {
               cDecodedChar += ( *pszStrRaw - '0' ) ;
            }
            else
            {
               cDecodedChar += ( *pszStrRaw - 'A' + 10 ) ;
            }
         }
         *pszStrDecod = cDecodedChar ;

         if ( *pszStrRaw == 0 )
         {
            break ;
         }
      }
      else
      {
         *pszStrDecod = *pszStrRaw ;
      }

      pszStrRaw++ ;
      pszStrDecod++ ;
   }
   *pszStrDecod = 0 ;
}


/*----------------------------------------------------------------------------*/
/* Concat string at the end of an other                                       */
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