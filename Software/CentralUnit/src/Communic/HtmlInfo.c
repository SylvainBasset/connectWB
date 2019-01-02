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



static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_byOutSize ) ;
static void html_ProcessSsiCalendar( DWORD i_dwParam2, char * o_pszOut, WORD i_byOutSize ) ;

static void html_ProcessCgi( DWORD i_dwParam1, DWORD i_dwParam2, char C* i_pszValue ) ;
static void html_ProcessCgiCalendar( DWORD i_dwParam2, char C* i_pszValue ) ;

/*----------------------------------------------------------------------------*/

void html_Init( void )
{
   cwifi_RegisterHtmlFunc( &html_ProcessSsi, &html_ProcessCgi ) ;
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/

static void html_ProcessSsi( DWORD i_dwParam1, DWORD i_dwParam2, char * o_pszOutput, WORD i_byOutSize )
{
   switch ( i_dwParam1 )
   {
      case HTML_PAGE_STATUS :
         *o_pszOutput = 0 ;   //TMP
         break ;

      case HTML_PAGE_CALENDAR :
         html_ProcessSsiCalendar( i_dwParam2, o_pszOutput, i_byOutSize ) ;
         break ;

      case HTML_PAGE_WIFI :
         *o_pszOutput = 0 ;   //TMP
         break ;

      default :
         *o_pszOutput = 0 ;
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessSsiCalendar( DWORD i_dwParam2,
                                     char * o_pszOutput, WORD i_byOutSize )
{
   s_DateTime DateTime ;
   BYTE byWeekday ;
   s_Time StartTime ;
   s_Time EndTime ;
   DWORD dwStartTimeCnt ;
   DWORD dwEndTimeCnt ;

   clk_GetDateTime( &DateTime, &byWeekday ) ;

   switch ( i_dwParam2 )
   {
      case HTML_CALENDAR_SSI_WEEKDAY :
         switch ( byWeekday )
         {
            case 0 :
               strncpy( o_pszOutput, "Lundi", i_byOutSize ) ;
               break ;
            case 1 :
               strncpy( o_pszOutput, "Mardi", i_byOutSize ) ;
               break ;
            case 2 :
               strncpy( o_pszOutput, "Mercredi", i_byOutSize ) ;
               break ;
            case 3 :
               strncpy( o_pszOutput, "Jeudi", i_byOutSize ) ;
               break ;
            case 4 :
               strncpy( o_pszOutput, "Vendredi", i_byOutSize ) ;
               break ;
            case 5 :
               strncpy( o_pszOutput, "Samedi", i_byOutSize ) ;
               break ;
            case 6 :
               strncpy( o_pszOutput, "Dimanche", i_byOutSize ) ;
               break ;
            default :
               strncpy( o_pszOutput, "---", i_byOutSize ) ;
               break ;
         }
         break ;

      case HTML_CALENDAR_SSI_DAY :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byDays ) ;
         break ;

      case HTML_CALENDAR_SSI_MONTH :
         switch ( DateTime.byMonth )
         {
            case 1 :
               strncpy( o_pszOutput, "Janvier", i_byOutSize ) ;
               break ;
            case 2 :
               strncpy( o_pszOutput, "F&eacute;vrier", i_byOutSize ) ;
               break ;
            case 3 :
               strncpy( o_pszOutput, "Mars", i_byOutSize ) ;
               break ;
            case 4 :
               strncpy( o_pszOutput, "Avril", i_byOutSize ) ;
               break ;
            case 5 :
               strncpy( o_pszOutput, "Mai", i_byOutSize ) ;
               break ;
            case 6 :
               strncpy( o_pszOutput, "Juin", i_byOutSize ) ;
               break ;
            case 7 :
               strncpy( o_pszOutput, "Juillet", i_byOutSize ) ;
               break ;
            case 8 :
               strncpy( o_pszOutput, "Ao&ucirc;t", i_byOutSize ) ;
               break ;
            case 9 :
               strncpy( o_pszOutput, "Septembre", i_byOutSize ) ;
               break ;
            case 10 :
               strncpy( o_pszOutput, "Octobre", i_byOutSize ) ;
               break ;
            case 11 :
               strncpy( o_pszOutput, "Novembre", i_byOutSize ) ;
               break ;
            case 12 :
               strncpy( o_pszOutput, "D&eacute;cembre", i_byOutSize ) ;
               break ;
            default :
               strncpy( o_pszOutput, "---", i_byOutSize ) ;
               break ;
         }
         break ;

      case HTML_CALENDAR_SSI_YEAR :
         snprintf( o_pszOutput, i_byOutSize, "%04u", ((WORD)DateTime.byYear + 2000 ) ) ;
         break ;

      case HTML_CALENDAR_SSI_HOURS :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byHours ) ;
         break ;

      case HTML_CALENDAR_SSI_MINUTES :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byMinutes ) ;
         break ;

      case HTML_CALENDAR_SSI_SECONDS :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.bySeconds ) ;
         break ;


      case HTML_CALENDAR_SSI_CAL_MONDAY :
      case HTML_CALENDAR_SSI_CAL_TUESDAY :
      case HTML_CALENDAR_SSI_CAL_WEDNESDAY :
      case HTML_CALENDAR_SSI_CAL_THURSDAY :
      case HTML_CALENDAR_SSI_CAL_FRIDAY :
      case HTML_CALENDAR_SSI_CAL_SATURDAY :
      case HTML_CALENDAR_SSI_CAL_SUNDAY :
         byWeekday = i_dwParam2 - 7 ;
         cal_GetDayVals( byWeekday, &StartTime, &EndTime, &dwStartTimeCnt, &dwEndTimeCnt ) ;

         if ( dwStartTimeCnt < dwEndTimeCnt )
         {
            snprintf( o_pszOutput, i_byOutSize,
                      "<TD>de <b>%02u:%02u</b></TD><TD>&agrave; <b>%02u:%02u</b></TD>",
                      StartTime.byHours, StartTime.byMinutes,
                      EndTime.byHours, EndTime.byMinutes ) ;
         }
         else if ( dwStartTimeCnt > dwEndTimeCnt )
         {
            if ( dwEndTimeCnt == 0 )
            {
               snprintf( o_pszOutput, i_byOutSize,
                         "<TD>de <b>%02u:%02u</b></TD><TD>&agrave; <b>minuit</b></TD>",
                         StartTime.byHours, StartTime.byMinutes ) ;
            }
            else
            {
               snprintf( o_pszOutput, i_byOutSize,
                         "<TD>de <b>minuit</b></TD><TD>&agrave; <b>%02u:%02u</b></TD>" \
                         "<TD>et de <b>%02u:%02u</b></TD><TD>&agrave; <b>minuit</b></TD>",
                         EndTime.byHours, EndTime.byMinutes,
                         StartTime.byHours, StartTime.byMinutes ) ;
            }
         }
         else
         {
            strncpy( o_pszOutput, "<TD><b>Off</b></TD>", i_byOutSize ) ;
         }
         break ;

      default :
          strncpy( o_pszOutput, "---", i_byOutSize ) ;
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
         break ;

      case HTML_PAGE_CALENDAR :
         html_ProcessCgiCalendar( i_dwParam2, i_pszValue ) ;
         break ;

      case HTML_PAGE_WIFI :
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
         sscanf( i_pszValue, "%u,%u,%u,%u,%u", &uiDays, &uiMonth, &uiYear,
                                               &uiHours, &uiMinutes  ) ;
         DateTime.byYear = uiYear - 2000 ;
         DateTime.byMonth = uiMonth ;
         DateTime.byDays = uiDays ;
         DateTime.byHours = uiHours ;
         DateTime.byMinutes = uiMinutes ;
         DateTime.bySeconds = 0 ;

         if ( clk_IsValid( &DateTime ) )
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
