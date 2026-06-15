/*
 * Task_Battery_CTRL.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_Battery_CTRL.h"

#include "task.h"
#include "tim.h"

static TaskHandle_t xTask_battery_ctrlHandle = NULL;


// Тело задачи
static void vTask_battery_ctrl_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    for(;;)
    {
    	//------Body------


        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void Task_battery_ctrl_Start(void)
{


    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_battery_ctrl_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_battery_ctrl_BodyFunction,
		TASK_battery_ctrl_NAME,
		TASK_battery_ctrl_STACK_SIZE,
        NULL,
		TASK_battery_ctrl_PRIORITY,
        &xTask_battery_ctrlHandle
    );

    if (xReturned != pdPASS)
    {
    	xTask_battery_ctrlHandle = NULL;
    }
}

void Task_battery_ctrl_Stop(void)
{
    if (xTask_battery_ctrlHandle != NULL)
    {
        vTaskDelete(xTask_battery_ctrlHandle);
        xTask_battery_ctrlHandle = NULL;
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_battery_ctrl_IsRunning(void)
{
    if (xTask_battery_ctrlHandle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_battery_ctrlHandle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}

