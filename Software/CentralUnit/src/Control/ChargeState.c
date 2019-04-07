/******************************************************************************/
/*                                                                            */
/*                               ChargeState.c                                */
/*                                                                            */
/******************************************************************************/
/* Created on:   28 nov. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "Control.h"
#include "System.h"
#include "System/Hard.h"


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define CSTATE_BUTTON_FLT_DUR          100
#define CSTATE_BUTTON_LONGPRESS_DUR   5000

#define CSTATE_LED_BLINK               500

#define CSTATE_ENDOFCHARGE_DELAY       180   /* delai before taking End of charge in account, sec */

typedef enum
{
   CSTATE_LED_OFF = 0,
   CSTATE_LED_RED,
   CSTATE_LED_BLUE,
   CSTATE_LED_GREEN,
   CSTATE_LED_RED_BLINK,
   CSTATE_LED_BLUE_BLINK,
   CSTATE_LED_GREEN_BLINK,
} e_cstateLedColor ;


typedef struct
{
   BOOL bWifiMaint ;
   BOOL bWifiConnect ;

   BOOL bEnabled ;
   BOOL bEvPlugged ;
   BOOL bCharging ;
   BOOL bEndOfCharge ;
   DWORD dwCurrentMinStop ;
   e_cstateForceSt eForceState ;
   e_cstateChargeSt eChargeState ;
} s_cstateData ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cstate_ProcessState( BOOL i_bToogleForce ) ;
static e_cstateForceSt cstate_GetNextForcedState( e_cstateForceSt i_eForceState ) ;
static void cstate_UpdateForceState( e_cstateForceSt i_eForceState ) ;

static void cstate_ProcessLed( void ) ;
static BOOL cstate_ProcessButton( BOOL * o_bLongPress ) ;

static void cstate_HrdInitButton( void ) ;
static void cstate_HrdInitLed( void ) ;
static void cstate_HrdSetColorLedWifi( e_cstateLedColor i_eLedColor ) ;
static void cstate_HrdSetColorLedCharge( e_cstateLedColor i_eLedColor ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

s_cstateData l_Data ;

static DWORD l_dwTmpEndOfCharge ;
static SDWORD l_sdwPrevCurrent ;

static DWORD l_dwTmpButtonFilt ;
static DWORD l_dwTmpButtonLongPress ;
static BOOL l_bButtonState ;

static e_cstateLedColor l_eWifiLedColor ;
static e_cstateLedColor l_eChargeLedColor ;

static DWORD l_dwTmpBlinkLedWifi ;
static DWORD l_dwTmpBlinkLedCharge ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cstate_Init( void )
{
   l_Data.eForceState = (e_cstateForceSt)g_sDataEeprom->sChargeStateData.dwForceState ;
   l_Data.dwCurrentMinStop = g_sDataEeprom->sChargeStateData.dwCurrentMinStop ;

   l_sdwPrevCurrent = SDWORD_MAX ;
   l_Data.bEnabled = BYTE_MAX ;              /* force first update */
   l_Data.eChargeState = CSTATE_OFF ;

   cstate_HrdInitButton() ;
   cstate_HrdInitLed() ;

   tim_StartSecTmp( &l_dwTmpEndOfCharge ) ;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/

void cstate_EnterSaveMode( void )
{
   cstate_HrdSetColorLedWifi( CSTATE_LED_OFF ) ;
   cstate_HrdSetColorLedCharge( CSTATE_LED_OFF ) ;
}


/*----------------------------------------------------------------------------*/

void cstate_SetCurrentMinStop( DWORD i_dwCurrentMinStop )
{
   if ( i_dwCurrentMinStop != l_Data.dwCurrentMinStop )
   {
      l_Data.dwCurrentMinStop = i_dwCurrentMinStop ;
      eep_write( (DWORD)&g_sDataEeprom->sChargeStateData.dwCurrentMinStop, i_dwCurrentMinStop ) ;
   }
}


/*----------------------------------------------------------------------------*/

DWORD cstate_GetCurrentMinStop( void )
{
   return l_Data.dwCurrentMinStop ;
}


/*----------------------------------------------------------------------------*/

void cstate_ToggleForce( void )
{
   e_cstateForceSt eForceState ;

   eForceState = cstate_GetNextForcedState( l_Data.eForceState ) ;
   cstate_UpdateForceState( eForceState ) ;
}


/*----------------------------------------------------------------------------*/

e_cstateForceSt cstate_GetForceState( void )
{
   return l_Data.eForceState ;
}


/*----------------------------------------------------------------------------*/

e_cstateChargeSt cstate_GetChargeState( void )
{
   return l_Data.eChargeState ;
}


/*----------------------------------------------------------------------------*/

BOOL cstate_IsEvPlugged( void )
{
   return l_Data.bEvPlugged ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void cstate_TaskCyc( void )
{
   BOOL bPress ;
   BOOL bLongPress ;
   BOOL bToogleWifi ;
   BOOL bToogleForce ;
   //BOOL bEvPlugged ;

   l_Data.bWifiMaint = cwifi_IsMaintMode() ;
   l_Data.bWifiConnect = cwifi_IsConnected() ;


   //bEvPlugged = ( coevse_GetPlugState() == COEVSE_EV_PLUGGED ) ;
   //
   //if ( l_Data.bEvPlugged != bEvPlugged )
   //{
   //   l_Data.bEvPlugged = bEvPlugged ;
   //   if ( bEvPlugged )
   //   {
   //      l_Data.bEndOfCharge = FALSE ;
   //      l_dwTmpEndOfCharge = 0 ;
   //   }
   //}

   bPress = cstate_ProcessButton( &bLongPress ) ;

   bToogleWifi = bPress && bLongPress ;
   bToogleForce = bPress && ( ! bLongPress ) ;

   if ( bToogleWifi )
   {
      cwifi_SetMaintMode( ! l_Data.bWifiMaint ) ;
   }

   cstate_ProcessState( bToogleForce ) ;

   cstate_ProcessLed() ;
}


/*=========================================================================*/

/*----------------------------------------------------------------------------*/

static void cstate_ProcessState( BOOL i_bToogleForce )
{
   e_cstateChargeSt eNextChargeState ;
   e_cstateForceSt eForceState ;
   SDWORD sdwCurrent ;
   BOOL bForce ;
   BOOL bCal ;
   BOOL bCharge ;
   BOOL bEoc ;
   BOOL bEocLowCur ;
   BOOL bEnabled ;

   if ( i_bToogleForce )
   {
      eForceState = cstate_GetNextForcedState( l_Data.eForceState ) ;
   }
   else
   {
      eForceState = l_Data.eForceState ;
   }

   bForce = ( eForceState != CSTATE_FORCE_NONE ) ;
   bCal = ( ! clk_IsDateTimeLost() ) && cal_IsChargeEnable() ;

   sdwCurrent = coevse_GetCurrent() ;
   bCharge = ( sdwCurrent > 0 ) ;
   bEoc = FALSE ;
   bEocLowCur = FALSE ;

   if ( tim_IsEndSecTmp( &l_dwTmpEndOfCharge, CSTATE_ENDOFCHARGE_DELAY ) )
   {
      tim_StartSecTmp( &l_dwTmpEndOfCharge ) ;
      if ( ( sdwCurrent == 0 ) && ( l_sdwPrevCurrent == 0 ) )
      {
         bEoc = TRUE ;
      }
      if ( ( sdwCurrent == 0 ) &&  ( sdwCurrent < l_Data.dwCurrentMinStop  ) &&
           ( l_sdwPrevCurrent == 0 ) && ( l_sdwPrevCurrent < l_Data.dwCurrentMinStop ) )
      {
         bEocLowCur = TRUE ;
      }

      l_sdwPrevCurrent = sdwCurrent ;
   }

   eNextChargeState = CSTATE_NULL ;

   switch ( l_Data.eChargeState )
   {
      case CSTATE_OFF :
         if ( bForce )
         {
            eNextChargeState = CSTATE_FORCE_WAIT ;
         }
         else if ( bCal )
         {
            eNextChargeState = CSTATE_ON_WAIT ;
         }
         bEnabled = FALSE ;
         break ;

      case CSTATE_FORCE_WAIT :
         if ( ! bForce )
         {
            eNextChargeState = CSTATE_OFF ;
         }
         else if ( bCharge )
         {
            eNextChargeState = CSTATE_CHARGING ;
         }
         bEnabled = TRUE ;
         break ;

      case CSTATE_ON_WAIT :
         if ( bForce )
         {
            eNextChargeState = CSTATE_OFF ;
         }
         else if ( ! bCal )
         {
            eNextChargeState = CSTATE_FORCE_WAIT ;
         }
         else if ( bCharge )
         {
            eNextChargeState = CSTATE_CHARGING ;
         }
         bEnabled = TRUE ;
         break ;

      case CSTATE_CHARGING :
         if ( bEoc || ( ( l_Data.eForceState != CSTATE_FORCE_ALL ) && ( ! bCal ) ) )
         {
            eNextChargeState = CSTATE_EOC ;
         }
         else if ( ( ! bForce ) && bEocLowCur )
         {
            eNextChargeState = CSTATE_EOC_LOWCUR ;
         }
         bEnabled = TRUE ;
         break ;

      case CSTATE_EOC_LOWCUR :
         if ( bForce )
         {
            eNextChargeState = CSTATE_FORCE_WAIT ;
         }
         else if ( ! bCal )
         {
            eNextChargeState = CSTATE_EOC ;
         }
         eForceState = CSTATE_FORCE_NONE ;
         break ;

      case CSTATE_EOC :
         eNextChargeState = CSTATE_EOC ;
         eForceState = CSTATE_FORCE_NONE ;
         break ;

      default :
         ERR_FATAL() ;
         break ;
   }

   cstate_UpdateForceState( eForceState ) ;

   if ( l_Data.bEnabled != bEnabled )
   {
      l_Data.bEnabled = bEnabled ;
      coevse_SetEnable( bEnabled ) ;
   }

   if ( eNextChargeState != CSTATE_NULL )
   {
      l_Data.eChargeState = eNextChargeState ;
   }
}


/*----------------------------------------------------------------------------*/

static e_cstateForceSt cstate_GetNextForcedState( e_cstateForceSt i_eForceState )
{
   e_cstateForceSt eNextForceSt ;

   if ( i_eForceState == CSTATE_FORCE_NONE )
   {
      eNextForceSt = CSTATE_FORCE_AMPMIN ;
   }
   else if ( i_eForceState == CSTATE_FORCE_AMPMIN )
   {
      eNextForceSt = CSTATE_FORCE_ALL ;
   }
   else
   {
      eNextForceSt = CSTATE_FORCE_NONE ;
   }

   return eNextForceSt ;
}


/*----------------------------------------------------------------------------*/

static void cstate_UpdateForceState( e_cstateForceSt i_eForceState )
{
   if ( l_Data.eForceState != i_eForceState )
   {
      l_Data.eForceState = i_eForceState ;
      eep_write( (DWORD)&g_sDataEeprom->sChargeStateData.dwForceState, i_eForceState ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Update Led color/blink                                                     */
/*----------------------------------------------------------------------------*/

static void cstate_ProcessLed( void )
{
   e_cstateLedColor eWifiLedColor ;
   e_cstateLedColor eChargeLedColor ;

   if ( l_Data.bWifiMaint )
   {
      eWifiLedColor = CSTATE_LED_BLUE_BLINK ;
   }
   else if ( l_Data.bWifiConnect )
   {
      eWifiLedColor = CSTATE_LED_BLUE ;
   }
   else
   {
      eWifiLedColor = CSTATE_LED_OFF ;
   }

   if ( l_eWifiLedColor != eWifiLedColor )
   {
      l_eWifiLedColor = eWifiLedColor ;

      cstate_HrdSetColorLedWifi( eWifiLedColor ) ;

      if ( ( eWifiLedColor == CSTATE_LED_RED_BLINK ) ||
           ( eWifiLedColor == CSTATE_LED_BLUE_BLINK ) ||
           ( eWifiLedColor == CSTATE_LED_GREEN_BLINK )  )
      {
         tim_StartMsTmp( &l_dwTmpBlinkLedWifi ) ;
      }
      else
      {
         l_dwTmpBlinkLedWifi = 0 ;
      }
   }
   if ( tim_IsEndMsTmp( &l_dwTmpBlinkLedWifi, CSTATE_LED_BLINK ) )
   {
      HAL_GPIO_TogglePin( CSTATE_LEDWIFI_COMMON_GPIO,CSTATE_LEDWIFI_COMMON_PIN ) ;
      tim_StartMsTmp( &l_dwTmpBlinkLedWifi ) ;
   }

   switch( l_Data.eChargeState )
   {
      case CSTATE_OFF :

      case CSTATE_EOC_LOWCUR :
      case CSTATE_EOC :
         if ( clk_IsDateTimeLost() )
         {
            eChargeLedColor = CSTATE_LED_RED_BLINK ;
         }
         else
         {
            eChargeLedColor = CSTATE_LED_OFF ;
         }
         break ;

      case CSTATE_FORCE_WAIT :
         if ( l_Data.eForceState != CSTATE_FORCE_AMPMIN )
         {
            eChargeLedColor = CSTATE_LED_BLUE_BLINK ;
         }
         else
         {
            eChargeLedColor = CSTATE_LED_BLUE ;
         }
         break ;

      case CSTATE_ON_WAIT :
         eChargeLedColor = CSTATE_LED_GREEN_BLINK ;
         break ;

      case CSTATE_CHARGING :
         eChargeLedColor = CSTATE_LED_GREEN ;
         break ;

      default :
         ERR_FATAL() ;
         break ;
   }

   if ( l_eChargeLedColor != eChargeLedColor )
   {
      l_eChargeLedColor = eChargeLedColor ;

      cstate_HrdSetColorLedCharge( eChargeLedColor ) ;

      if ( ( eChargeLedColor == CSTATE_LED_RED_BLINK ) ||
           ( eChargeLedColor == CSTATE_LED_BLUE_BLINK ) ||
           ( eChargeLedColor == CSTATE_LED_GREEN_BLINK )  )
      {
         tim_StartMsTmp( &l_dwTmpBlinkLedCharge ) ;
      }
      else
      {
         l_dwTmpBlinkLedCharge = 0 ;
      }
   }
   if ( tim_IsEndMsTmp( &l_dwTmpBlinkLedCharge, CSTATE_LED_BLINK ) )
   {
      HAL_GPIO_TogglePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN ) ;
      tim_StartMsTmp( &l_dwTmpBlinkLedCharge ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Button read proces                                                         */
/*----------------------------------------------------------------------------*/

static BOOL cstate_ProcessButton( BOOL * o_bLongPress )
{
   BOOL bPressEvt ;
   BOOL bButtonRaw ;

   bPressEvt = FALSE ;
   *o_bLongPress = FALSE ;

   bButtonRaw = ISSET( CSTATE_BUTTON_P2_GPIO->IDR, CSTATE_BUTTON_P2_PIN ) ;

   if ( tim_IsEndMsTmp( &l_dwTmpButtonFilt, CSTATE_BUTTON_FLT_DUR ) )
   {
      if ( l_bButtonState != bButtonRaw )
      {
         l_bButtonState = bButtonRaw ;

         if ( bButtonRaw )
         {
            tim_StartMsTmp( &l_dwTmpButtonLongPress ) ;
         }
         else
         {
            bPressEvt = TRUE ;
            if ( tim_IsEndMsTmp( &l_dwTmpButtonLongPress, CSTATE_BUTTON_LONGPRESS_DUR ) )
            {
               *o_bLongPress = TRUE ;
            }
            l_dwTmpButtonLongPress = 0 ;
         }
      }
   }

   if ( ( l_bButtonState != bButtonRaw ) &&
        ( tim_GetRemainMsTmp( &l_dwTmpButtonFilt, CSTATE_BUTTON_FLT_DUR ) == 0 ) )
   {
      tim_StartMsTmp( &l_dwTmpButtonFilt ) ;
   }

   return bPressEvt ;
}


/*----------------------------------------------------------------------------*/
/* Hardware initialization for button                                         */
/*----------------------------------------------------------------------------*/

static void cstate_HrdInitButton( void )
{
   GPIO_InitTypeDef sGpioInit ;

      /* configure button 1 pin as output, no push/pull, high freq */
   sGpioInit.Pin = CSTATE_BUTTON_P1_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_BUTTON_P1_AF ;
   HAL_GPIO_Init( CSTATE_BUTTON_P1_GPIO, &sGpioInit ) ;

   HAL_GPIO_WritePin( CSTATE_BUTTON_P1_GPIO, CSTATE_BUTTON_P1_PIN, GPIO_PIN_SET ) ;

      /* configure button 2 pin as input, pull-down, high freq */
   sGpioInit.Pin = CSTATE_BUTTON_P2_PIN ;
   sGpioInit.Mode = GPIO_MODE_INPUT ;
   sGpioInit.Pull = GPIO_PULLDOWN ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_BUTTON_P2_AF ;
   HAL_GPIO_Init( CSTATE_BUTTON_P2_GPIO, &sGpioInit ) ;
}


/*----------------------------------------------------------------------------*/
/* Hardware initialization for Leds                                           */
/*----------------------------------------------------------------------------*/

static void cstate_HrdInitLed( void )
{
   GPIO_InitTypeDef sGpioInit ;

   sGpioInit.Pin = CSTATE_LEDCH_RED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDCH_RED_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_RED_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDCH_BLUE_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDCH_BLUE_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_BLUE_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDCH_GREEN_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDCH_GREEN_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_GREEN_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDCH_COMMON_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDCH_COMMON_AF ;
   HAL_GPIO_Init( CSTATE_LEDCH_COMMON_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;


   sGpioInit.Pin = CSTATE_LEDWIFI_RED_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDWIFI_RED_AF ;
   HAL_GPIO_Init( CSTATE_LEDWIFI_RED_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDWIFI_RED_GPIO, CSTATE_LEDWIFI_RED_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDWIFI_BLUE_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDWIFI_BLUE_AF ;
   HAL_GPIO_Init( CSTATE_LEDWIFI_BLUE_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDWIFI_BLUE_GPIO, CSTATE_LEDWIFI_BLUE_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDWIFI_GREEN_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDWIFI_GREEN_AF ;
   HAL_GPIO_Init( CSTATE_LEDWIFI_GREEN_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDWIFI_GREEN_GPIO, CSTATE_LEDWIFI_GREEN_PIN, GPIO_PIN_RESET ) ;

   sGpioInit.Pin = CSTATE_LEDWIFI_COMMON_PIN ;
   sGpioInit.Mode = GPIO_MODE_OUTPUT_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_LEDWIFI_COMMON_AF ;
   HAL_GPIO_Init( CSTATE_LEDWIFI_COMMON_GPIO, &sGpioInit ) ;
   HAL_GPIO_WritePin( CSTATE_LEDWIFI_COMMON_GPIO, CSTATE_LEDWIFI_COMMON_PIN, GPIO_PIN_RESET ) ;
}


/*----------------------------------------------------------------------------*/

static void cstate_HrdSetColorLedWifi( e_cstateLedColor i_eLedColor )
{
   switch ( i_eLedColor )
   {
      case CSTATE_LED_RED_BLINK :
      case CSTATE_LED_RED :
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_COMMON_GPIO, CSTATE_LEDWIFI_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_RED_GPIO, CSTATE_LEDWIFI_RED_PIN, GPIO_PIN_SET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_BLUE_GPIO, CSTATE_LEDWIFI_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_GREEN_GPIO, CSTATE_LEDWIFI_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;

      case CSTATE_LED_BLUE_BLINK :
      case CSTATE_LED_BLUE :
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_COMMON_GPIO, CSTATE_LEDWIFI_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_RED_GPIO, CSTATE_LEDWIFI_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_BLUE_GPIO, CSTATE_LEDWIFI_BLUE_PIN, GPIO_PIN_SET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_GREEN_GPIO, CSTATE_LEDWIFI_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;

      case CSTATE_LED_GREEN_BLINK :
      case CSTATE_LED_GREEN :
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_COMMON_GPIO, CSTATE_LEDWIFI_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_RED_GPIO, CSTATE_LEDWIFI_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_BLUE_GPIO, CSTATE_LEDWIFI_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_GREEN_GPIO, CSTATE_LEDWIFI_GREEN_PIN, GPIO_PIN_SET ) ;
         break ;

      case CSTATE_LED_OFF :
      default :
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_COMMON_GPIO, CSTATE_LEDWIFI_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_RED_GPIO, CSTATE_LEDWIFI_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_BLUE_GPIO, CSTATE_LEDWIFI_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDWIFI_GREEN_GPIO, CSTATE_LEDWIFI_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;
   }
}


/*----------------------------------------------------------------------------*/

static void cstate_HrdSetColorLedCharge( e_cstateLedColor i_eLedColor )
{
   switch ( i_eLedColor )
   {
      case CSTATE_LED_RED_BLINK :
      case CSTATE_LED_RED :
         HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_SET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;

      case CSTATE_LED_BLUE_BLINK :
      case CSTATE_LED_BLUE :
         HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_SET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;

      case CSTATE_LED_GREEN_BLINK :
      case CSTATE_LED_GREEN :
         HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_SET ) ;
         break ;

      case CSTATE_LED_OFF :
      default :
         HAL_GPIO_WritePin( CSTATE_LEDCH_COMMON_GPIO, CSTATE_LEDCH_COMMON_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_RED_GPIO, CSTATE_LEDCH_RED_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_BLUE_GPIO, CSTATE_LEDCH_BLUE_PIN, GPIO_PIN_RESET ) ;
         HAL_GPIO_WritePin( CSTATE_LEDCH_GREEN_GPIO, CSTATE_LEDCH_GREEN_PIN, GPIO_PIN_RESET ) ;
         break ;
   }
}
