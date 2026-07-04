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
#include "Telemetry_Data.h"

#define task_dalay_ms 10
#define ALPHA_IIR 0.5

static void IIR_BMP280(BMP280_CompensatedData* data, BMP280_CompensatedData* new_data);

static TaskHandle_t xTask_bmp280Handle = NULL;

static BMP280_HandleTypeDef bmp1;

BMP280_CompensatedData bmp_data;

// Тело задачи
static void vTask_bmp280_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(task_dalay_ms);

    for(;;)
    {
    	BMP280_GetMeasuredData(&bmp1);

    	IIR_BMP280(&bmp_data,&bmp1.data);
    	Telemetry_SetBaro(bmp_data.temperature, bmp_data.pressure, bmp_data.Altitude, bmp_data.relative_height);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void Task_bmp280_Start(void)
{
	//Инициализация датчика при старте программы
	bmp1.hspi = &hspi1;
	bmp1.cs_pin = BMP280_SS_Pin;
	bmp1.cs_port = BMP280_SS_GPIO_Port;
	HAL_StatusTypeDef status =  BMP280_Init(&bmp1);
	if(status != HAL_OK){
		//Error_Handler();
		//TODO: написать свою функци обработки исключительной ситуации
	}

	BMP280_GetMeasuredData(&bmp1);
	bmp_data = bmp1.data;

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

void IIR_BMP280(BMP280_CompensatedData* data, BMP280_CompensatedData* new_data)
{
	data->Altitude = data->Altitude * ALPHA_IIR + new_data->Altitude * (1 - ALPHA_IIR);
	data->pressure = data->pressure * ALPHA_IIR + new_data->pressure * (1 - ALPHA_IIR);;
	data->relative_height = data->relative_height * ALPHA_IIR + new_data->relative_height * (1 - ALPHA_IIR);;
	data->temperature = data->temperature * ALPHA_IIR + new_data->temperature * (1 - ALPHA_IIR);;
}
