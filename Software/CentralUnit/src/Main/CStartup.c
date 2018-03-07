/******************************************************************************/
/*                                                                            */
/*                                 CStartup.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:    3 mars 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include <stm32l0xx.h>
#include <stm32l0xx_hal.h>


/*----------------------------------------------------------------------------*/
/* Defines 															   				         */
/*----------------------------------------------------------------------------*/

	/* Note: Uncomment the following line if you need   */
	/* to relocate your  vector Table in Internal SRAM. */

/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x00          /* Vector Table base offset field.
													/* Must be a multiple of 0x200. */


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

	/* Note: These global variables are used for the */
	/* ST HAL to configure and update system clock   */

uint32_t SystemCoreClock = 2000000 ;   /* Set clock to 2Mhz (MSI value before clk config) */
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t PLLMulTable[9] = {3, 4, 6, 8, 12, 16, 24, 32, 48};


/*----------------------------------------------------------------------------*/
/* Setup the microcontroller system.                                          */
/* @param  None                                                               */
/* @retval None                                                               */
/*----------------------------------------------------------------------------*/
void cstart_SystemInit(void)
{
   SET_BIT(RCC->CR, RCC_CR_MSION) ;    /* Set MSION bit */
   												/* Reset HSION, HSEON, CSSON, PLLON */
   CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_HSION| RCC_CR_HSIKERON| RCC_CR_CSSHSEON | RCC_CR_PLLON);

   CLEAR_REG(RCC->CFGR);	            /* Reset CFGR register */

   CLEAR_BIT(RCC->CR, RCC_CR_HSEBYP) ; /* Reset HSEBYP bit */

   CLEAR_REG(RCC->CIER) ;              /* Disable all interrupts */

   RCC->AHBRSTR = 0xFFFFFFFF ;         /* Reset all AHB periphrals */
   CLEAR_REG(RCC->AHBRSTR ) ;
   RCC->APB1RSTR = 0xFFFFFFFF ;        /* Reset all APB2 periphrals */
   CLEAR_REG(RCC->APB1RSTR ) ;
   RCC->APB1RSTR = 0xFFFFFFFF ;        /* Reset all APB1 periphrals */
   CLEAR_REG(RCC->APB1RSTR ) ;
   RCC->IOPRSTR = 0xFFFFFFFF ;         /* Reset IO ports */
   CLEAR_REG(RCC->IOPRSTR ) ;

   CLEAR_REG( RCC->AHBENR ) ;          /* clock disable for all AHB periph */
   CLEAR_REG( RCC->APB1ENR ) ;         /* clock disable for all APB1 periph */
   CLEAR_REG( RCC->APB2ENR ) ;         /* clock disable for all APB2 periph */
   CLEAR_REG( RCC->IOPENR ) ;          /* clock disable for IO ports */

   CLEAR_REG( RCC->IOPSMENR ) ;        /* sleep clock disable for all AHB periph */
   CLEAR_REG( RCC->AHBSMENR ) ;        /* sleep clock disable for all APB1 periph */
   CLEAR_REG( RCC->APB2SMENR ) ;       /* sleep clock disable for all APB2 periph */
   CLEAR_REG( RCC->APB1SMENR ) ;       /* sleep clock disable for IO ports */

#ifdef VECT_TAB_SRAM
													/* Vector Table Relocation in Internal SRAM */
  SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET;
#else
													/* Vector Table Relocation in Internal FLASH */
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
#endif
}


/*----------------------------------------------------------------------------*/
/* System Clock Configuration                                                 */
/* Note: The system Clock is configured as follow :                           */
/*       System Clock source            = HSI + PLL                           */
/*       SYSCLK(Hz)                     = 32000000                            */
/*       HCLK(Hz)                       = 32000000                            */
/*       AHB Prescaler                  = 1                                   */
/*       APB1 Prescaler                 = 1                                   */
/*       APB2 Prescaler                 = 1                                   */
/*       Flash Latency(WS)              = 1                                   */
/*       Main regulator output voltage  = Scale1 mode                         */
/*----------------------------------------------------------------------------*/
void cstart_ClkConfig(void)
{
   RCC_ClkInitTypeDef RCC_ClkInitStruct ;
   RCC_OscInitTypeDef RCC_OscInitStruct ;

   __PWR_CLK_ENABLE() ;                /* Enable Power Control clock */

      /* Note: The voltage scaling allows optimizing the power consumption */
      /* when the device is clocked below the maximum system frequency, to */
      /* update the voltage scaling value regarding system frequency refer */
      /* to product datasheet.                                             */

   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1) ;

                                       /* Enable HSI Oscillator + PLL */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI ;
   RCC_OscInitStruct.HSIState = RCC_HSI_ON ;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON ;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI ;
   RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4 ;
   RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2 ;
   HAL_RCC_OscConfig(&RCC_OscInitStruct) ;

                                       /* Select PLL as system clock source and */
                                       /* configure the HCLK, PCLK1 and PCLK2 */
                                       /* clocks dividers */
   RCC_ClkInitStruct.ClockType = ( RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 ) ;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK ;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1 ;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1 ;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1 ;
   HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1 ) ;
}