/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmp280.h"
#include "VL53L0X.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
//===============BMP280===================
extern BMP280_HandleTypeDef bmp1;

//===============VL53L0====================
uint32_t refSpadCount;
uint8_t isApertureSpads;
uint8_t VhvSettings;
uint8_t PhaseCal;

// Флаги состояния
volatile uint8_t vl53l0x_ready = 0;
volatile uint8_t vl53l0x_error = 0;


statInfo_t_VL53L0X distanceStr;
uint16_t distance;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MPU6050_task */
osThreadId_t MPU6050_taskHandle;
const osThreadAttr_t MPU6050_task_attributes = {
  .name = "MPU6050_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for BMP280_Task */
osThreadId_t BMP280_TaskHandle;
const osThreadAttr_t BMP280_Task_attributes = {
  .name = "BMP280_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for VL53L0_Task */
osThreadId_t VL53L0_TaskHandle;
const osThreadAttr_t VL53L0_Task_attributes = {
  .name = "VL53L0_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void Start_MPU6050_task(void *argument);
void Start_BMP280_Task(void *argument);
void Start_VL53L0_Task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of MPU6050_task */
  MPU6050_taskHandle = osThreadNew(Start_MPU6050_task, NULL, &MPU6050_task_attributes);

  /* creation of BMP280_Task */
  BMP280_TaskHandle = osThreadNew(Start_BMP280_Task, NULL, &BMP280_Task_attributes);

  /* creation of VL53L0_Task */
  VL53L0_TaskHandle = osThreadNew(Start_VL53L0_Task, NULL, &VL53L0_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_Start_MPU6050_task */
/**
* @brief Function implementing the MPU6050_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_MPU6050_task */
void Start_MPU6050_task(void *argument)
{
  /* USER CODE BEGIN Start_MPU6050_task */

  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Start_MPU6050_task */
}

/* USER CODE BEGIN Header_Start_BMP280_Task */
/**
* @brief Function implementing the BMP280_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_BMP280_Task */
void Start_BMP280_Task(void *argument)
{
  /* USER CODE BEGIN Start_BMP280_Task */
	BMP280_Init(&bmp1);
	osDelay(50);
  /* Infinite loop */
  for(;;)
  {
	  BMP280_GetMeasuredData(&bmp1);

	  osDelay(10);
  }
  /* USER CODE END Start_BMP280_Task */
}

/* USER CODE BEGIN Header_Start_VL53L0_Task */
/**
* @brief Function implementing the VL53L0_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Start_VL53L0_Task */
void Start_VL53L0_Task(void *argument)
{
  /* USER CODE BEGIN Start_VL53L0_Task */

    /* === Основной цикл измерений === */
    for(;;)
    {
    	distance = readRangeSingleMillimeters(&distanceStr);
        osDelay(50);  // ~20 Гц опроса, можно настроить под ваши нужды
    }
  /* USER CODE END Start_VL53L0_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

