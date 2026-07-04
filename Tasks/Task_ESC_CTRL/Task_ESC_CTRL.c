/*
 * Task_esc_ctrl.c
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#include "Task_ESC_CTRL.h"

#include "task.h"
#include "tim.h"

static TaskHandle_t xTask_esc_ctrlHandle = NULL;


// Тело задачи
static void vTask_esc_ctrl_BodyFunction(void *pvParameters)
{

    // Запускаем ШИМ один раз (таймер уже настроен)
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    for(;;)
    {
//        // Средняя скорость: импульс 1.5 мс
//        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1 || TIM_CHANNEL_2 || TIM_CHANNEL_3 || TIM_CHANNEL_4, 1500);
//        vTaskDelay(pdMS_TO_TICKS(1000));

        // Стоп: импульс 1 мс (можно 1000) или полностью выключить канал
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1000);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1000);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 1000);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 1000);
        vTaskDelay(pdMS_TO_TICKS(1000));
//
//        // Средняя скорость: импульс 1.5 мс
//        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1 || TIM_CHANNEL_2 || TIM_CHANNEL_3 || TIM_CHANNEL_4, 1100);
//        vTaskDelay(pdMS_TO_TICKS(1000));
//
//        // Стоп: импульс 1 мс (можно 1000) или полностью выключить канал
//        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1 || TIM_CHANNEL_2 || TIM_CHANNEL_3 || TIM_CHANNEL_4, 1000);
//        vTaskDelay(pdMS_TO_TICKS(1000));
//
//        // Средняя скорость: импульс 1.5 мс
//        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1 || TIM_CHANNEL_2 || TIM_CHANNEL_3 || TIM_CHANNEL_4, 1250);
//        vTaskDelay(pdMS_TO_TICKS(1000));
//
//        // Стоп: импульс 1 мс (можно 1000) или полностью выключить канал
//        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1 || TIM_CHANNEL_2 || TIM_CHANNEL_3 || TIM_CHANNEL_4, 1000);
//        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void Task_esc_ctrl_Start(void)
{


    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_esc_ctrl_IsRunning())
        return;

    BaseType_t xReturned = xTaskCreate(
        vTask_esc_ctrl_BodyFunction,
		TASK_esc_ctrl_NAME,
		TASK_esc_ctrl_STACK_SIZE,
        NULL,
		TASK_esc_ctrl_PRIORITY,
        &xTask_esc_ctrlHandle
    );

    if (xReturned != pdPASS)
    {
    	xTask_esc_ctrlHandle = NULL; // На всякий случай
    }
}

void Task_esc_ctrl_Stop(void)
{
    if (xTask_esc_ctrlHandle != NULL)
    {
        vTaskDelete(xTask_esc_ctrlHandle);
        xTask_esc_ctrlHandle = NULL; // ⚠️ Обязательно! Иначе будет висячий указатель
    }
}

// Функция проверки статуса через API FreeRTOS
bool Task_esc_ctrl_IsRunning(void)
{
    if (xTask_esc_ctrlHandle == NULL)
        return false;

    // Получаем состояние задачи
    eTaskState eState = eTaskGetState(xTask_esc_ctrlHandle);

    // Задача считается "работающей", если она не удалена
    // (Ready, Running, Blocked, Suspended — всё это валидные состояния)
    return (eState != eDeleted);
}


