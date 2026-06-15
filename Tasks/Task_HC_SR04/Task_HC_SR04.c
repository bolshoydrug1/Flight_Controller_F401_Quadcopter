/*
 * Task_HC_SR04.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_HC_SR04.h"

#include "task.h"
#include "tim.h"

static TaskHandle_t xTask_hc_sr04Handle = NULL;


// Тело задачи
static void vTask_hc_sr04_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    for(;;)
    {
    	//------Body------


        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void Task_hc_sr04_Start(void)
{


    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_hc_sr04_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_hc_sr04_BodyFunction,
		TASK_hc_sr04_NAME,
		TASK_hc_sr04_STACK_SIZE,
        NULL,
		TASK_hc_sr04_PRIORITY,
        &xTask_hc_sr04Handle
    );

    if (xReturned != pdPASS)
    {
    	xTask_hc_sr04Handle = NULL;
    }
}

void Task_hc_sr04_Stop(void)
{
    if (xTask_hc_sr04Handle != NULL)
    {
        vTaskDelete(xTask_hc_sr04Handle);
        xTask_hc_sr04Handle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_hc_sr04_IsRunning(void)
{
    if (xTask_hc_sr04Handle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_hc_sr04Handle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}


