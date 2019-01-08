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
   BOOL bForce ;
} s_cstateData ;


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static BOOL cstate_ButtonProcess( BOOL * o_bLongPress ) ;
static void cstate_LedUpdate( s_cstateData C* i_pData ) ;

static void cstate_HrdInitButton( void ) ;
static void cstate_HrdInitLed( void ) ;
static void cstate_HrdSetColorLedWifi( e_cstateLedColor i_eLedColor ) ;
static void cstate_HrdSetColorLedCharge( e_cstateLedColor i_eLedColor ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

s_cstateData l_Data ;

static DWORD l_dwTmpButtonFilt ;
static DWORD l_dwTmpButtonLongPress ;
static BOOL l_bButtonState ;

static e_cstateLedColor l_eWifiLedColor ;
static e_cstateLedColor l_eChargeLedColor ;

DWORD l_dwTmpBlinkLedWifi ;
DWORD l_dwTmpBlinkLedCharge ;


/*----------------------------------------------------------------------------*/
/* Module initialization                                                      */
/*----------------------------------------------------------------------------*/

void cstate_Init( void )
{
   cstate_HrdInitButton() ;
   cstate_HrdInitLed() ;
}


/*----------------------------------------------------------------------------*/
/* periodic task                                                              */
/*----------------------------------------------------------------------------*/

void cstate_TaskCyc( void )
{
   s_cstateData CurData ;
   BOOL bPress ;
   BOOL bLongPress ;


   CurData.bWifiMaint = cwifi_IsMaintMode() ;
   CurData.bWifiConnect = cwifi_IsConnected() ;

   CurData.bEvPlugged = coevse_IsPlugged() ;
   CurData.bCharging = coevse_IsCharging() ;
   CurData.bForce = l_Data.bForce ;

   bPress = cstate_ButtonProcess( &bLongPress ) ;

   if ( bPress )
   {
      if ( bLongPress )
      {
         CurData.bWifiMaint = ! CurData.bWifiMaint ;
         cwifi_SetMaintMode( CurData.bWifiMaint ) ;
      }
      else
      {
         CurData.bForce = ! CurData.bForce ;
      }
   }

   if ( l_Data.bCharging && ( ! CurData.bCharging ) )
   {
      CurData.bForce = FALSE ;
   }

                                        // limite min courant: test si carge en cours et courant < seuil
   CurData.bEnabled = CurData.bForce || ( cal_IsChargeEnable() && TRUE ) ;

   if ( l_Data.bEnabled != CurData.bEnabled )
   {
      coevse_SetEnable( CurData.bEnabled ) ;
   }

   cstate_LedUpdate( &CurData ) ;

   l_Data = CurData ;
}


/*=========================================================================*/

/*----------------------------------------------------------------------------*/
/* Button read proces                                                         */
/*----------------------------------------------------------------------------*/

static BOOL cstate_ButtonProcess( BOOL * o_bLongPress )
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
/* Update Led color/blink                                                     */
/*----------------------------------------------------------------------------*/

static void cstate_LedUpdate( s_cstateData C* i_pData )
{
   e_cstateLedColor eWifiLedColor ;
   e_cstateLedColor eChargeLedColor ;

   if ( i_pData->bWifiMaint )
   {
      eWifiLedColor = CSTATE_LED_BLUE_BLINK ;
   }
   else if ( i_pData->bWifiConnect )
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


   if ( i_pData->bCharging )
   {
      eChargeLedColor = CSTATE_LED_GREEN ;
   }
   else if ( i_pData->bForce )
   {
	  eChargeLedColor = CSTATE_LED_BLUE_BLINK ;
   }
   else if ( i_pData->bEvPlugged && ( ! i_pData->bEnabled ) )
   {
      eChargeLedColor = CSTATE_LED_GREEN_BLINK ;
   }
   else
   {
      eChargeLedColor = CSTATE_LED_OFF ;
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
