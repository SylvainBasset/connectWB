/**
  ******************************************************************************
  * @file    CORTEXM/CORTEXM_SysTick/Src/main.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    24-April-2014
  * @brief   This example shows how to configure the SysTick.
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include <stm32l0xx_hal.h>
#include "main.h"
#include "stm32l0xx_nucleo.h"


/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
int main(void)
{
  /* STM32L0xx HAL library initialization:
		 - Configure the Flash prefetch, Flash preread and Buffer caches
		 - Systick timer is configured by default as source of time base, but user
				 can eventually implement his proper time base source (a general purpose
				 timer for example or other time source), keeping in mind that Time base
				 duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
				 handled in milliseconds basis.
		 - Low Level Initialization
	  */
  HAL_Init();

  /* Configure LED2 */
  BSP_LED_Init(LED2);
  /* Configure the system clock */

  /* Turn on LED2 */
  BSP_LED_On(LED2);

  /* Toggle LED2 */
  BSP_LED_Toggle(LED2);


  /* SysTick Timer is configured by default to generate an interrupt each 1 msec.
	  ---------------------------------------------------------------------------
	 1. The configuration is done using HAL_SYSTICK_Config() located in HAL_Init().

	 2. The HAL_SYSTICK_Config() function configure:
		 - The SysTick Reload register with value passed as function parameter.
		 - Configure the SysTick IRQ priority to the lowest value (0x03).
		 - Reset the SysTick Counter register.
		 - Configure the SysTick Counter clock source to be Core Clock Source (HCLK).
		 - Enable the SysTick Interrupt.
		 - Start the SysTick Counter.

	 3. The SysTick time base 1 msec is computed using the following formula:

			Reload Value = SysTick Counter Clock (Hz) x  Desired Time base (s)

		 - Reload Value is the parameter to be passed for SysTick_Config() function
		 - Reload Value should not exceed 0xFFFFFF

	 @note: Caution, the SysTick time base 1 msec must not be changed due to use
			  of these time base by HAL driver.
  */

  /* Infinite loop */
  while (1)
  {
	 /* Toggle LED2 */
	 BSP_LED_Toggle(LED2);

	 /* Insert 50 ms delay */
	 HAL_Delay(50);

  }
}


