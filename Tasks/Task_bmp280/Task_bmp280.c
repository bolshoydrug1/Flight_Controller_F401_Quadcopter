/*
 * Task_bmp280.c
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#include "Task_bmp280.h"
#include "task.h"
#include "bmp280.h"
#include "spi.h"

static TaskHandle_t xTask_bmp280Handle = NULL;

BMP280_HandleTypeDef bmp1;

// Тело задачи
static void vTask_bmp280_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    for(;;)
    {
    	BMP280_GetMeasuredData(&bmp1);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void Task_bmp280_Start(void)
{
	//Инициализация датчика при старте программы
	bmp1.hspi = &hspi1;
	bmp1.cs_pin = BMP280_SS_Pin;
	bmp1.cs_port = BMP280_SS_GPIO_Port;
	BMP280_Init(&bmp1);

    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_bmp280_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_bmp280_BodyFunction,
		TASK_bmp280_NAME,
		TASK_bmp280_STACK_SIZE,
        NULL,
		TASK_bmp280_PRIORITY,
        &xTask_bmp280Handle
    );

    if (xReturned != pdPASS)
    {
    	xTask_bmp280Handle = NULL; // На всякий случай
    }
}

void Task_bmp280_Stop(void)
{
    if (xTask_bmp280Handle != NULL)
    {
        vTaskDelete(xTask_bmp280Handle);
        xTask_bmp280Handle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_bmp280_IsRunning(void)
{
    if (xTask_bmp280Handle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_bmp280Handle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}


