/*
 * Task_Telemetry.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_Telemetry.h"

#include "task.h"
#include "tim.h"

static TaskHandle_t xTask_telemetryHandle = NULL;


// Тело задачи
static void vTask_telemetry_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    for(;;)
    {
    	//------Body------


        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void Task_telemetry_Start(void)
{


    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_telemetry_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_telemetry_BodyFunction,
		TASK_telemetry_NAME,
		TASK_telemetry_STACK_SIZE,
        NULL,
		TASK_telemetry_PRIORITY,
        &xTask_telemetryHandle
    );

    if (xReturned != pdPASS)
    {
    	xTask_telemetryHandle = NULL;
    }
}

void Task_telemetry_Stop(void)
{
    if (xTask_telemetryHandle != NULL)
    {
        vTaskDelete(xTask_telemetryHandle);
        xTask_telemetryHandle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_telemetry_IsRunning(void)
{
    if (xTask_telemetryHandle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_telemetryHandle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}

