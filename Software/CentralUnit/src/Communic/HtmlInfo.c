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
      case 1 :
         html_ProcessSsiCalendar( i_dwParam2, o_pszOutput, i_byOutSize ) ;
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

   clk_GetDateTime( &DateTime, &byWeekday ) ;

   switch ( i_dwParam2 )
   {
      case 0 :
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

      case 1 :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byDays ) ;
         break ;

      case 2 :
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

      case 3 :
         snprintf( o_pszOutput, i_byOutSize, "%04u", ((WORD)DateTime.byYear + 2000 ) ) ;
         break ;

      case 4 :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byHours ) ;
         break ;

      case 5 :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.byMinutes ) ;
         break ;

      case 6 :
         snprintf( o_pszOutput, i_byOutSize, "%02u", DateTime.bySeconds ) ;
         break ;


      case 7 :
      case 8 :
      case 9 :
      case 10 :
      case 11 :
      case 12 :
      case 13 :
         byWeekday = i_dwParam2 - 7 ;
         cal_GetDayVals( byWeekday, &StartTime, &EndTime ) ;

         if ( ( StartTime.byHours == EndTime.byHours ) &&
              ( StartTime.byMinutes == EndTime.byMinutes ) &&
              ( StartTime.bySeconds == EndTime.bySeconds ) )
         {
            strncpy( o_pszOutput, "<TD><b>Off</b></TD>", i_byOutSize ) ;
         }
         else
         {
            snprintf( o_pszOutput, i_byOutSize,
                      "<TD>de <b>%02u:%02u</b></TD><TD>&agrave; <b>%02u:%02u</b></TD>",
                      StartTime.byHours, StartTime.byMinutes,
                      EndTime.byHours, EndTime.byMinutes ) ;
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
      case 1 :
         html_ProcessCgiCalendar( i_dwParam2, i_pszValue ) ;
         break ;

      default :
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void html_ProcessCgiCalendar( DWORD i_dwParam2, char C* i_pszValue )
{
   s_DateTime DateTime ;

   unsigned int uiDays ;
   unsigned int uiMonth ;
   unsigned int uiYear ;
   unsigned int uiHours ;
   unsigned int uiMinutes ;

   switch ( i_dwParam2 )
   {
      case 0 :
         sscanf( i_pszValue, "%u,%u,%u,%u,%u", &uiDays,
                                               &uiMonth,
                                               &uiYear,
                                               &uiHours,
                                               &uiMinutes  ) ;
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

      default :
         break ;
   }
}
