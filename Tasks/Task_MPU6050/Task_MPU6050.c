/*
 * Task_MPU6050.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_MPU6050.h"

#include "task.h"
#include "tim.h"

static TaskHandle_t xTask_mpu6050Handle = NULL;


// Тело задачи
static void vTask_mpu6050_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    for(;;)
    {
    	//------Body------


        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void Task_mpu6050_Start(void)
{


    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_mpu6050_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_mpu6050_BodyFunction,
		TASK_mpu6050_NAME,
		TASK_mpu6050_STACK_SIZE,
        NULL,
		TASK_mpu6050_PRIORITY,
        &xTask_mpu6050Handle
    );

    if (xReturned != pdPASS)
    {
    	xTask_mpu6050Handle = NULL; // На всякий случай
    }
}

void Task_mpu6050_Stop(void)
{
    if (xTask_mpu6050Handle != NULL)
    {
        vTaskDelete(xTask_mpu6050Handle);
        xTask_mpu6050Handle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_mpu6050_IsRunning(void)
{
    if (xTask_mpu6050Handle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_mpu6050Handle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}

