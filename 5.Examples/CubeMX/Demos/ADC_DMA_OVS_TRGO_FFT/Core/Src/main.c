/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "bdma.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp.h"
#include "analog/dsp_fft.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

#if defined(__CC_ARM)  // AC5
#define __AT_ADDR(addr) __attribute__((at(addr)))
#elif defined(__GNUC__)  // AC6
#define __AT_ADDR(addr) __attribute__((section(".ARM.__at_" #addr)))
#endif

#define ADC2_BUFSIZE 512  // config

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// ADC1: PC2,PC3
// ADC2: PC5
// ADC3: PC1,PC2,VRefint,ChipTemp

__AT_ADDR(0x24000000)
ALIGN_32BYTES(__IO uint16_t ADC1_Conv[2]) = {0};  // AXI

__AT_ADDR(0x24000000)
ALIGN_32BYTES(__IO uint16_t ADC2_Conv[ADC2_BUFSIZE * 2]) = {0};  // AXI

__AT_ADDR(0x38000000)
ALIGN_32BYTES(__IO uint16_t ADC3_Conv[4]) = {0};  // SRAM4

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t ADC2_Conv_DmaStage = 0;

void ADC2_DMA_CallBack(void)  // call in DMAx_Streamx_IRQHandler()
{
    // DMA double buffer

    if ((DMA1->LISR & DMA_FLAG_HTIF1_5) != RESET)  // HalfCplt
    {
        SCB_InvalidateDCache_by_Addr((uint32_t*)(&ADC2_Conv[0]), ADC2_BUFSIZE);
        ADC2_Conv_DmaStage = 1;
    }
    else if ((DMA1->LISR & DMA_FLAG_TCIF1_5) != RESET)  // Cplt
    {
        SCB_InvalidateDCache_by_Addr((uint32_t*)(&ADC2_Conv[ADC2_BUFSIZE]), ADC2_BUFSIZE);
        ADC2_Conv_DmaStage = 2;
    }
}

void ADC2_FFT_Proc(void)
{
    float32_t aInput[ADC2_BUFSIZE];

     uint16_t*  pSrc;
     float32_t* pDest = &aInput[0];
	
	    ADC2_Conv_DmaStage = 1;

    switch (ADC2_Conv_DmaStage)
    {
        case 1:
            pSrc = (uint16_t*) &ADC2_Conv[0];
            break;
        case 2:
            pSrc = (uint16_t*) &ADC2_Conv[ADC2_BUFSIZE];
            break;
        default:
            return;
    }

    ADC2_Conv_DmaStage = 0;

    for (uint32_t i = 0; i < ADC2_BUFSIZE; i++)
    {
#if 0
        // 50Hz cos + 100Hz cos, fs = 1024, phase = 60
        *pDest++ = 1.2 + 2.5 * cos(2 * 3.1415926 * 50 * i / 1024 + 3.1415926 / 3) + 1.9 * cos(2 * 3.1415926 * 100 * i / 1024 + 3.1415926 / 4);
#else
        *pDest = *pSrc;
#endif
    }

    FFT(aInput, ADC2_BUFSIZE, 2e3);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

/* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_BDMA_Init();
  MX_DMA_Init();
  MX_ADC3_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_ADC2_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

    DelayNonInit();

    // TIM6 APB1 = 240MHz

    uint32_t ADC_CalibrationMode;
#if 1
    ADC_CalibrationMode = ADC_CALIB_OFFSET;  // ƫ��У׼
#else
    ADC_CalibrationMode = ADC_CALIB_OFFSET_LINEARITY;  // ���Զ�У׼
#endif

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CalibrationMode, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CalibrationMode, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_CalibrationMode, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC1_Conv, ARRAY_SIZE(ADC1_Conv));
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)ADC2_Conv, ARRAY_SIZE(ADC2_Conv));
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)ADC3_Conv, ARRAY_SIZE(ADC3_Conv));

    HAL_TIM_Base_Start(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
		 printf("111111111111111\n");
		// ADC2_FFT_Proc();
		while(1){}

    while (1)
    {
        //-----------------------------------------------------------------------------
        // Display Voltage

        static uint32_t tDispConvAD = 0;

        if (DelayNonBlock(tDispConvAD, 1000))
        {
            static uint16_t nSampleIndex = 0;

            tDispConvAD = DelayNonGetTick();

            SCB_InvalidateDCache_by_Addr((uint32_t*)(&ADC1_Conv[0]), ARRAY_SIZE(ADC1_Conv));
            SCB_InvalidateDCache_by_Addr((uint32_t*)(&ADC3_Conv[0]), ARRAY_SIZE(ADC3_Conv));

#if 1
					
            printf("%5d | ", nSampleIndex++);

            for (uint16_t i = 0; i < ARRAY_SIZE(ADC1_Conv); ++i)
            {
                // ADC1_Conv[i] /= hadc1.Init.Oversampling.Ratio;
                printf("%.6f ", AD2VOL(ADC1_Conv[i]));
            }

            for (uint16_t i = 0; i < ARRAY_SIZE(ADC3_Conv); ++i)
            {
                printf("%.6f ", (i == 3) ? TS_ConvTemp(ADC3_Conv[i]) : AD2VOL(ADC3_Conv[i]));
            }

            printf("\n");

#endif
        }

        //-----------------------------------------------------------------------------
        // Display Frequency and Phase

        if (ADC2_Conv_DmaStage != 0)
        {
            static uint16_t nSampleIndex = 0;

            printf("%5d | ", nSampleIndex++);
            ADC2_FFT_Proc();
            printf("\n");
        }

        // 240e6 / 240 / 500 = 2k

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInitStruct.PLL2.PLL2M = 2;
  PeriphClkInitStruct.PLL2.PLL2N = 12;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */