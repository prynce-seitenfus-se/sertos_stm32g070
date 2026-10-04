/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>

#include "sertos.h"
#include "sertos_demo_queue.h"
#include "sertos_task_consumer.h"
#include "sertos_task_profiler.h"
#include "sertos_task_producer.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SERTOS_DEMO_STACK_BYTES (256U)
#define SERTOS_PROF_STACK_BYTES (2048U)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t s_producer_stack[SERTOS_DEMO_STACK_BYTES]
    __attribute__((aligned(SERTOS_STACK_ALIGNMENT_BYTES)));
static uint8_t s_consumer_stack[SERTOS_DEMO_STACK_BYTES]
    __attribute__((aligned(SERTOS_STACK_ALIGNMENT_BYTES)));
static uint8_t s_profiler_stack[SERTOS_PROF_STACK_BYTES]
    __attribute__((aligned(SERTOS_STACK_ALIGNMENT_BYTES)));
static uint8_t s_idle_task_stack[SERTOS_CONFIG_IDLE_TASK_STACK_SIZE]
    __attribute__((aligned(SERTOS_STACK_ALIGNMENT_BYTES)));
static SertosTaskControlBlock s_producer_tcb;
static SertosTaskControlBlock s_consumer_tcb;
static SertosTaskControlBlock s_profiler_tcb;
static SertosTaskHandle s_producer_handle;
static SertosTaskHandle s_consumer_handle;
static SertosTaskHandle s_profiler_handle;
static SertosDemoQueue* s_demo_queue;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
    SertosStatus status;
    SertosTaskConfig producer_config;
    SertosTaskConfig consumer_config;
    SertosTaskConfig profiler_config;

    SertosConfig sertos_cfg = {
        .tick_rate_hz = 1000U,
        .enable_time_slicing = true,
        .idle_task_stack = s_idle_task_stack,
        .idle_task_stack_size = sizeof(s_idle_task_stack),
        .tick_hook = NULL,
        .idle_hook = NULL
    };

    sertos_task_profiler_init();

    status = sertos_scheduler_init_with_config(&sertos_cfg);
    if (status != SERTOS_STATUS_OK) {
        Error_Handler();
        return 0;
    }

    status = sertos_demo_queue_init(&s_demo_queue);
    if (status != SERTOS_STATUS_OK) {
        Error_Handler();
        return 0;
    }

    producer_config.name = "sertos_task_producer";
    producer_config.entry_func = sertos_task_producer;
    producer_config.param = s_demo_queue;
    producer_config.priority = 3U;
    producer_config.stack_buffer = s_producer_stack;
    producer_config.stack_size = sizeof(s_producer_stack);

    status = sertos_task_create_static(&producer_config,
                                       &s_producer_tcb,
                                       &s_producer_handle);
    if (status != SERTOS_STATUS_OK) {
        Error_Handler();
        return 0;
    }

    consumer_config.name = "sertos_task_consumer";
    consumer_config.entry_func = sertos_task_consumer;
    consumer_config.param = s_demo_queue;
    consumer_config.priority = 2U;
    consumer_config.stack_buffer = s_consumer_stack;
    consumer_config.stack_size = sizeof(s_consumer_stack);

    status = sertos_task_create_static(&consumer_config,
                                       &s_consumer_tcb,
                                       &s_consumer_handle);
    if (status != SERTOS_STATUS_OK) {
        Error_Handler();
        return 0;
    }

    profiler_config.name = "sertos_task_profiler";
    profiler_config.entry_func = sertos_task_profiler;
    profiler_config.param = NULL;
    profiler_config.priority = 1U;
    profiler_config.stack_buffer = s_profiler_stack;
    profiler_config.stack_size = sizeof(s_profiler_stack);

    status = sertos_task_create_static(&profiler_config,
                                       &s_profiler_tcb,
                                       &s_profiler_handle);
    if (status != SERTOS_STATUS_OK) {
        Error_Handler();
        return 0;
    }

    if (!sertos_task_profiler_start_uart_receive()) {
        Error_Handler();
        return 0;
    }

    sertos_scheduler_start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
#ifdef USE_FULL_ASSERT
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
