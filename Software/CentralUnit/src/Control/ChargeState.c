/******************************************************************************/
/*                               ChargeState.c                                */
/******************************************************************************/
/*
   OpenEVSE Charge state management

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
   @history 1.0, 28 nov. 2018, creation
   @brief
   This module manage the enable/disable state of OpenEVSE.

   The enable/disable state is computed the FSM located in
   cstate_ProcessState(), given the following entries :
    - Calendard state : Given by cal_IsChargeEnable() to check if a regular
      enable is currently allowed by the calendar.
    - Charge force : 3 types of force are managed by the module :
    - Actual charging status, with consumed current, provided by commEVSE.c
      module

   Three forcing levels are available :
      . CSTATE_FORCE_NONE : no force. Charge is allowed only if it set in
        calendar, and the consumed current is above the theshold limit
        (l_Data.dwCurrentMinStop)
      . CSTATE_FORCE_AMPMIN : Ampere min forceed, Charge is allowed only
        if it set in  calendar, the actual consumed current is not checked
      . CSTATE_FORCE_ALL : Charge is allowed in any circumstances.
   These 3 levels can be switched by a short press on the CSTATE_BUTTON.
   The 'color' of the charge state led can indicable the current forcing
   level,(cf led state)

   There are 5 state in the FSM:
      - CSTATE_OFF : idle state. This is the device state when calendar
        charing is not enabled and there are no forcing mode. Charge is not
        enabled
      - CSTATE_FORCE_WAIT : forced and waiting for EV : One forced mode
        is enabled (which means that calendar charging is not used, always
        true). So, the charge is enabled but not currently on progress.
      - CSTATE_ON_WAIT : calendar enabled and waiting for EV : calendar
        charging allows the charge, but it is not currently on progress.
        (no force mode is set, otherwise the state would be CSTATE_FORCE_WAIT)
        Charge is enabled.
      - CSTATE_CHARGING : charging in progress. The charge is allowed by
        calendar or by forced mode, and a vehicle is charging.
        Charge is enabled.
      - CSTATE_EOC_LOWCUR : end of charge for low current. This state is
        used when the minimum current is checked (no force mode): If
        the real charging current cross the defined low limit during charging,
        this state is activated, until calandar or fored charge is not
        allowed. Charge is disabled.

   Note : The Cp line is read by the Adc. This allow to determine of a EV is
   plugged on. In CSTATE_OFF state, if the plug state comes from disonnected
   to connected, the openEVSE charge is allowed for 30 sec.
   This is a workaround for the Zoe sleep state.

   There are 2 LEDs indicating the device status :
      - wifi led :
         . Off : no connection
         . Steady blue : connection with home wifi
         . blinking blue : a maintenance AP point is settled for external
                           devices (see CommWifi.c)
      - charge led :
         . Off : idle or end of charge state, and date/time is correctly set
         . blinking red : idle or end of charge state, and date/time is lost
         . blinking blue : forced and waiting for EV state, with minimum
                           current force mode
         . steady blue : forced and waiting for EV state, with minimum
                         always forced force mode
         . blinking green : calendar enabled and waiting for EV state
         . steady green : charging in progress state
*/


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

#define CSTATE_ENDOFCHARGE_DELAY        30   /* delai before taking End of charge in account, sec */

#define CSTATE_ADC_EV_CONNECT_TH      1184

#define CSTATE_PLUGING_DELAY            30   /* delai for OPENEVSE enable at pluging, sec */

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
   BOOL bWifiMaint ;                   /* maintenance AP state from commWifi.c */
   BOOL bEnabled ;                     /* openEVSE enable status */
   DWORD dwCurrentMinStop ;            /* real charging current low limit */
   e_cstateForceSt eForceState ;       /* force status */
   e_cstateChargeSt eChargeState ;     /* FSM charge state */
   WORD awAdcValues [16] ;             /* Cp line Adc row measurments */
   WORD wAdcValuesIdx ;                /* measurment index */
   WORD wAdcMoy ;                      /* average value of Cp line measurment */
   BOOL bEvConnected ;                 /* plug state from cp line measurment */
   BOOL bPrevEvConnected ;             /* previous plug state */
   DWORD dwTmpPluging ;                /* delay to enable openEVSE because of plugging */
} s_cstateData ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void cstate_ProcessState( BOOL i_bToogleForce ) ;
static BOOL cstate_CheckEoc( void ) ;
static e_cstateForceSt cstate_GetNextForcedState( e_cstateForceSt i_eForceState ) ;
static void cstate_UpdateForceState( e_cstateForceSt i_eForceState ) ;

static void cstate_ProcessLed( void ) ;
static BOOL cstate_ProcessButton( BOOL * o_bLongPress ) ;
static WORD cstate_ProcessAdc( void ) ;

static void cstate_HrdInitCPLine( void ) ;
static void cstate_HrdInitButton( void ) ;
static void cstate_HrdInitLed( void ) ;
static void cstate_HrdSetColorLedWifi( e_cstateLedColor i_eLedColor ) ;
static void cstate_HrdSetColorLedCharge( e_cstateLedColor i_eLedColor ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

static s_cstateData l_Data ;

static e_cstateChargeSt l_aeHistState [10] ;
static BYTE l_byHistStateIdx ;

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
                                             /* init charge state history */
   l_aeHistState[l_byHistStateIdx] = CSTATE_OFF ;
   l_byHistStateIdx = NEXTIDX( l_byHistStateIdx, l_aeHistState ) ;

   cstate_HrdInitCPLine() ;
   cstate_HrdInitButton() ;
   cstate_HrdInitLed() ;
}


/*----------------------------------------------------------------------------*/
/* Set power save mode (reset LEDs)                                           */
/*----------------------------------------------------------------------------*/

void cstate_EnterSaveMode( void )
{
   cstate_HrdSetColorLedWifi( CSTATE_LED_OFF ) ;
   cstate_HrdSetColorLedCharge( CSTATE_LED_OFF ) ;
}


/*----------------------------------------------------------------------------*/
/* Set new value for minimum real charge current                              */
/*----------------------------------------------------------------------------*/

void cstate_SetCurrentMinStop( DWORD i_dwCurrentMinStop )
{
                                       /* if value is different */
   if ( i_dwCurrentMinStop != l_Data.dwCurrentMinStop )
   {
      l_Data.dwCurrentMinStop = i_dwCurrentMinStop ;
                                       /* store in eeprom */
      eep_write( (DWORD)&g_sDataEeprom->sChargeStateData.dwCurrentMinStop,
                 i_dwCurrentMinStop ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Read minimum real charge current value                                     */
/*----------------------------------------------------------------------------*/

DWORD cstate_GetCurrentMinStop( void )
{
   return l_Data.dwCurrentMinStop ;
}


/*----------------------------------------------------------------------------*/
/* Toggle force state mode                                                    */
/*----------------------------------------------------------------------------*/

void cstate_ToggleForce( void )
{
   e_cstateForceSt eForceState ;

   eForceState = cstate_GetNextForcedState( l_Data.eForceState ) ;
   cstate_UpdateForceState( eForceState ) ;
}


/*----------------------------------------------------------------------------*/
/* Read force state mode                                                      */
/*----------------------------------------------------------------------------*/

e_cstateForceSt cstate_GetForceState( void )
{
   return l_Data.eForceState ;
}


/*----------------------------------------------------------------------------*/
/* Read charge state                                                          */
/*----------------------------------------------------------------------------*/

e_cstateChargeSt cstate_GetChargeState( void )
{
   return l_Data.eChargeState ;
}


/*----------------------------------------------------------------------------*/
/* Read charge state history                                                  */
/*----------------------------------------------------------------------------*/

void cstate_GetHistState( CHAR * o_pszHistState, WORD i_wSize )
{
   BYTE byIdxOut ;
   CHAR * pszOut ;
   WORD wSize ;

   byIdxOut = l_byHistStateIdx ;
   pszOut = o_pszHistState ;
   wSize = i_wSize ;

   do
   {
      if ( wSize >= 2 )
      {
         *pszOut++ = '0' + (BYTE) l_aeHistState[byIdxOut] ;
         wSize-- ;
         *pszOut++= ' ' ;
         wSize-- ;
      }
      else
      {
         ERR_FATAL() ;
      }

      byIdxOut = NEXTIDX( byIdxOut, l_aeHistState ) ;
   } while ( byIdxOut != l_byHistStateIdx ) ;

   *pszOut = 0 ;
}


/*----------------------------------------------------------------------------*/
/* Read Cp line adc average value                                             */
/*----------------------------------------------------------------------------*/

WORD cstate_GetAdcVal( CHAR * o_pszAdcVal, WORD i_wSize )
{
   snprintf( o_pszAdcVal, i_wSize, "%u", l_Data.wAdcMoy ) ;

   return l_Data.wAdcMoy ;
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

   l_Data.bWifiMaint = cwifi_IsMaintMode() ;          /* check if wifi is in maintenance mode */

   l_Data.wAdcMoy = cstate_ProcessAdc() ;             /* read Cp line adc value */

   if ( l_Data.wAdcMoy < CSTATE_ADC_EV_CONNECT_TH )   /* if value is below the connect thershold */
   {
      if ( ! l_Data.bEvConnected )                    /* if there was no Ev connected before */
      {
         l_Data.bEvConnected = TRUE ;
         tim_StartSecTmp( &l_Data.dwTmpPluging ) ;
      }
   }
   else
   {
      l_Data.bEvConnected = FALSE ;
   }

   bPress = cstate_ProcessButton( &bLongPress ) ;     /* check main button */

   bToogleWifi = bPress && bLongPress ;
   bToogleForce = bPress && ( ! bLongPress ) ;

   if ( bToogleWifi )                                 /* if there is long press */
   {
      cwifi_SetMaintMode( ! l_Data.bWifiMaint ) ;     /* toggle wifi maintenance mode */
   }

   cstate_ProcessState( bToogleForce ) ;              /* update FSM state */

   cstate_ProcessLed() ;                              /* update LEDs */
}


/*=========================================================================*/

/*----------------------------------------------------------------------------*/
/* Update FSM state                                                           */
/*----------------------------------------------------------------------------*/

static void cstate_ProcessState( BOOL i_bToogleForce )
{
   e_cstateChargeSt eNextChargeState ;
   e_cstateForceSt eForceState ;
   BOOL bForce ;
   BOOL bCal ;
   BOOL bCharge ;
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
   bCharge = ( coevse_GetEvseState() == COEVSE_STATE_CHARGING ) ;

   if ( l_Data.eChargeState != CSTATE_CHARGING )
   {
      l_sdwPrevCurrent = SDWORD_MAX ;
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

         if ( tim_GetRemainSecTmp( &l_Data.dwTmpPluging, CSTATE_PLUGING_DELAY ) != 0 )
         {
            bEnabled = TRUE ;
         }
         else
         {
            bEnabled = FALSE ;
         }
         break ;

      case CSTATE_FORCE_WAIT :
         if ( ! bForce )
         {
            eNextChargeState = CSTATE_OFF ;
         }
         else if ( bCharge )
         {
            tim_StartSecTmp( &l_dwTmpEndOfCharge ) ;
            eNextChargeState = CSTATE_CHARGING ;
         }
         bEnabled = TRUE ;
         break ;

      case CSTATE_ON_WAIT :
         if ( bForce )
         {
            eNextChargeState = CSTATE_FORCE_WAIT ;
         }
         else if ( ! bCal )
         {
            eNextChargeState = CSTATE_OFF ;
         }
         else if ( bCharge )
         {
            tim_StartSecTmp( &l_dwTmpEndOfCharge ) ;
            eNextChargeState = CSTATE_CHARGING ;
         }
         bEnabled = TRUE ;
         break ;

      case CSTATE_CHARGING :
         if ( ! bCharge )
         {
            eNextChargeState = CSTATE_ON_WAIT ;
            eForceState = CSTATE_FORCE_NONE ;
         }
         else if ( ( l_Data.eForceState != CSTATE_FORCE_ALL ) && ( ! bCal ) )
         {
            eNextChargeState = CSTATE_OFF ;
            eForceState = CSTATE_FORCE_NONE ;
         }
         else if ( ( l_Data.eForceState == CSTATE_FORCE_NONE ) && cstate_CheckEoc() )
         {
            eNextChargeState = CSTATE_EOC_LOWCUR ;
            eForceState = CSTATE_FORCE_NONE ;
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
            eNextChargeState = CSTATE_OFF ;
         }
         bEnabled = FALSE ;
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
      l_aeHistState[l_byHistStateIdx] = eNextChargeState ;
      l_byHistStateIdx = NEXTIDX( l_byHistStateIdx, l_aeHistState ) ;
   }
}


/*----------------------------------------------------------------------------*/
/* Check end of charge by low current                                         */
/*----------------------------------------------------------------------------*/

static BOOL cstate_CheckEoc( void )
{
	BOOL bEocLowCur ;
   SDWORD sdwCurrent ;

   bEocLowCur = FALSE ;

   if ( tim_IsEndSecTmp( &l_dwTmpEndOfCharge, CSTATE_ENDOFCHARGE_DELAY ) )
   {
      tim_StartSecTmp( &l_dwTmpEndOfCharge ) ;

      sdwCurrent = coevse_GetCurrent() ;

      if ( ( sdwCurrent < ( l_Data.dwCurrentMinStop * 1000 )  ) &&
           ( l_sdwPrevCurrent < ( l_Data.dwCurrentMinStop * 1000 ) ) )
      {
         bEocLowCur = TRUE ;
      }
      l_sdwPrevCurrent = sdwCurrent ;
   }

   return bEocLowCur ;
}


/*----------------------------------------------------------------------------*/
/* Get the next forced mode status                                            */
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
/* Set forced mode status                                                     */
/*----------------------------------------------------------------------------*/

static void cstate_UpdateForceState( e_cstateForceSt i_eForceState )
{
   if ( l_Data.eForceState != i_eForceState )
   {
      l_Data.eForceState = i_eForceState ;
                                       /* store in eeprom */
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
   else if ( cwifi_IsConnected() )
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
         if ( l_Data.eForceState == CSTATE_FORCE_AMPMIN )
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

static WORD cstate_ProcessAdc( void )
{
   BYTE i ;
   WORD wAdcMoy ;

   if ( ISSET( ADC_CSTATE->ISR, ADC_ISR_ADRDY ) )
   {
      ADC_CSTATE->CR |= ADC_CR_ADSTART ;
   }

   l_Data.awAdcValues[l_Data.wAdcValuesIdx] = ADC_CSTATE->DR ;
   l_Data.wAdcValuesIdx = NEXTIDX( l_Data.wAdcValuesIdx, l_Data.awAdcValues ) ;

   wAdcMoy = 0 ;
   for ( i = 0 ; i < ARRAY_SIZE( l_Data.awAdcValues ) ; i++ )
   {
      wAdcMoy += l_Data.awAdcValues[i] ;
   }
   wAdcMoy = wAdcMoy / ARRAY_SIZE( l_Data.awAdcValues ) ;

   return wAdcMoy ;
}



/*----------------------------------------------------------------------------*/
/* Hardware initialization for CP line                                        */
/*----------------------------------------------------------------------------*/

static void cstate_HrdInitCPLine( void )
{
   GPIO_InitTypeDef sGpioInit ;

   sGpioInit.Pin = CSTATE_CP_LINE_PIN ;
   sGpioInit.Mode = GPIO_MODE_ANALOG ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = CSTATE_CP_LINE_AF ;
   HAL_GPIO_Init( CSTATE_CP_LINE_GPIO, &sGpioInit ) ;

   ADC_CSTATE_CLK_ENABLE() ;

   ADC_CSTATE->CFGR2 = ADC_CFGR2_CKMODE_0 ;
   ADC_CSTATE->SMPR = ADC_SMPR_SMPR_0 | ADC_SMPR_SMPR_1 | ADC_SMPR_SMPR_2 ;

   __SYSCFG_CLK_ENABLE() ;
   SYSCFG->CFGR3 = SYSCFG_CFGR3_ENBUF_SENSOR_ADC | SYSCFG_CFGR3_ENBUF_VREFINT_ADC ;

   ADC_CSTATE->CHSELR = ADC_CHSELR_CHSEL6 ;

   ADC1_COMMON->CCR = ADC_CCR_PRESC_2 ;

   ADC_CSTATE->CR = ADC_CR_ADEN ;
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
/* Low level color set for wifi Led                                           */
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
/* Low level color set for charge state Led                                   */
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