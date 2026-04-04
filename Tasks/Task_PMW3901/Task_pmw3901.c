/*
 * Task_pmw3901.c
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#include "Task_pmw3901.h"
#include "task.h"
#include "spi.h"
#include "driver_pmw3901mb_basic.h"

static TaskHandle_t xTask_pmw3901Handle = NULL;

float height = 1.0f;
float delta_x;
float delta_y;
pmw3901mb_motion_t motion;


// Тело задачи
static void vTask_pmw3901_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);

    for(;;)
    {
    	uint8_t res = pmw3901mb_basic_read(height, &motion, (float *)&delta_x, (float *)&delta_y);
    	if(res != 0)
    	{
    	    //Error_Handler();
    	}

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void Task_pmw3901_Start(void)
{
	//Инициализация датчика при старте программы
	HAL_GPIO_WritePin(PWM3901_RST_GPIO_Port, PWM3901_RST_Pin, GPIO_PIN_RESET);
	vTaskDelay(pdMS_TO_TICKS(10));
	HAL_GPIO_WritePin(PWM3901_RST_GPIO_Port, PWM3901_RST_Pin, GPIO_PIN_SET);
	vTaskDelay(pdMS_TO_TICKS(10));
	uint8_t res = pmw3901mb_basic_init();
	if(res != 0)
	{
	    Error_Handler();
	}

    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_pmw3901_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_pmw3901_BodyFunction,
		TASK_pmw3901_NAME,
		TASK_pmw3901_STACK_SIZE,
        NULL,
		TASK_pmw3901_PRIORITY,
        &xTask_pmw3901Handle
    );

    if (xReturned != pdPASS)
    {
    	xTask_pmw3901Handle = NULL; // На всякий случай
    }
}

void Task_pmw3901_Stop(void)
{
    if (xTask_pmw3901Handle != NULL)
    {
        vTaskDelete(xTask_pmw3901Handle);
        xTask_pmw3901Handle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_pmw3901_IsRunning(void)
{
    if (xTask_pmw3901Handle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_pmw3901Handle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}


