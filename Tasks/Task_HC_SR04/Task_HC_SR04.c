/*
 * Task_HC_SR04.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_HC_SR04.h"

#include "task.h"
#include "semphr.h"
#include "tim.h"
#include "main.h"
#include "hc_sr04.h"

#define task_dalay_ms 50

#define ALPHA_IIR 0.7
float height_hc_sr04 = 0.0f;

static hc_sr04_t hc_sr04_m;

static TaskHandle_t xTask_hc_sr04Handle = NULL;
static SemaphoreHandle_t xEchoDoneSemaphore = NULL;

// Тело задачи
static void vTask_hc_sr04_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(task_dalay_ms);

    for(;;)
    {
    	//------Body------
    	hc_sr04_trigger(&hc_sr04_m);

		if (xSemaphoreTake(xEchoDoneSemaphore, pdMS_TO_TICKS(40)) == pdTRUE)
		{
			//TODO:----------Защитить мьютексом-----------
			height_hc_sr04 = hc_sr04_get_distance(&hc_sr04_m, 25) * ALPHA_IIR + height_hc_sr04 * (1.0f - ALPHA_IIR);
			//TODO:----------Защитить мьютексом-----------
		}

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void Task_hc_sr04_Start(void)
{
	hc_sr04_m.htim = &htim3;
	hc_sr04_m.echo_port = HC_echo_GPIO_Port;
	hc_sr04_m.echo_pin = HC_echo_Pin;
	hc_sr04_m.trig_port = HC_trigger_GPIO_Port;
	hc_sr04_m.trig_pin = HC_trigger_Pin;

    hc_sr04_init(&hc_sr04_m);

    if (xEchoDoneSemaphore == NULL)
    {
        xEchoDoneSemaphore = xSemaphoreCreateBinary();
    }

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

void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(HC_echo_Pin);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != HC_echo_Pin)
    {
        return;
    }

    if (HAL_GPIO_ReadPin(HC_echo_GPIO_Port, HC_echo_Pin) == GPIO_PIN_SET)
    {
        echo_start_it(&hc_sr04_m);
    }
    else
    {
        echo_end_it(&hc_sr04_m);

        if (xEchoDoneSemaphore != NULL)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xEchoDoneSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}


