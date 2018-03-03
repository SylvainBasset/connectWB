/******************************************************************************/
/*                                                                            */
/*                                 CStartup.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:    3 mars 2018   Sylvain BASSET                                */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx.h>
#include <stm32l0xx_hal.h>


/******************************************************************************/
/* Defines 															   				         */
/******************************************************************************/

/*!< Uncomment the following line if you need to relocate your vector Table in
	  Internal SRAM. */
/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x00 /*!< Vector Table base offset field.
											  This value must be a multiple of 0x200. */


/******************************************************************************/
/* @brief  Setup the microcontroller system.                                  */
/* @param  None                                                               */
/* @retval None                                                               */
/******************************************************************************/
void cstart_SystemInit(void)
{
 /*!< Set MSION bit */
  RCC->CR |= (uint32_t)0x00000100;

  /*!< Reset SW[1:0], HPRE[3:0], PPRE1[2:0], PPRE2[2:0], MCOSEL[2:0] and MCOPRE[2:0] bits */
  RCC->CFGR &= (uint32_t) 0x88FF400C;

  /*!< Reset HSION, HSIDIVEN, HSEON, CSSON and PLLON bits */
  RCC->CR &= (uint32_t)0xFEF6FFF6;

  /*!< Reset HSI48ON  bit */
  RCC->CRRCR &= (uint32_t)0xFFFFFFFE;

  /*!< Reset HSEBYP bit */
  RCC->CR &= (uint32_t)0xFFFBFFFF;

  /*!< Reset PLLSRC, PLLMUL[3:0] and PLLDIV[1:0] bits */
  RCC->CFGR &= (uint32_t)0xFF02FFFF;

  /*!< Disable all interrupts */
  RCC->CIER = 0x00000000;

  /* Configure the Vector Table location add offset address ------------------*/
#ifdef VECT_TAB_SRAM
  SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM */
#else
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH */
#endif
}


/******************************************************************************/
/* @brief  System Clock Configuration                                         */
/*          The system Clock is configured as follow :                        */
/*             System Clock source            = MSI                           */
/*             SYSCLK(Hz)                     = 2000000                       */
/*             HCLK(Hz)                       = 2000000                       */
/*             AHB Prescaler                  = 1                             */
/*             APB1 Prescaler                 = 1                             */
/*             APB2 Prescaler                 = 1                             */
/*             Flash Latency(WS)              = 0                             */
/*             Main regulator output voltage  = Scale3 mode                   */
/* @retval None                                                               */
/******************************************************************************/

void cstart_ClkConfig(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
	  clocked below the maximum system frequency, to update the voltage scaling value
	  regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /* Enable MSI Oscillator */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
  RCC_OscInitStruct.MSICalibrationValue=0x00;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);


  /* Select MSI as system clock source and configure the HCLK, PCLK1 and PCLK2
	  clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}
