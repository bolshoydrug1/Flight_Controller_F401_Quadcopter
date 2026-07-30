/*
 * Task_Battery_CTRL.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_Battery_CTRL.h"

#include "task.h"
#include "adc.h"
#include "Telemetry_Data.h"

static TaskHandle_t xTask_battery_ctrlHandle = NULL;

#define BATTERY_TASK_PERIOD_MS      20U

/* Опорное напряжение АЦП - в проекте нет отдельного вывода VREF, ADC мерит
 * относительно VDDA. Если после калибровки процент "плавает" от платы к
 * плате - в первую очередь поправить эту константу (замерить реальный VDDA
 * мультиметром), а не логику фильтра/маппинга. */
#define BATTERY_ADC_VREF_V          3.3f
#define BATTERY_ADC_MAX_RAW         4095.0f

/* Калибровка под вашу схемотехнику: напряжение на входе АЦП, не напряжение
 * батареи напрямую (делитель/масштабирование - за кадром этого файла). */
#define BATTERY_VOLTAGE_EMPTY_V     2.16f
#define BATTERY_VOLTAGE_FULL_V      3.06f

/* alpha однополюсного БИХ (экспоненциальное скользящее среднее) при периоде
 * задачи 20мс даёт постоянную времени фильтра ~1с (alpha = 1 - exp(-T/tau)).
 * Больше alpha - быстрее реакция и больше шума, меньше - глаже, но инерционнее. */
#define BATTERY_IIR_ALPHA           0.6f

/* Без аппаратного триггера: каждый такт задача сама запускает одну
 * software-triggered DMA-конверсию (ADC1 настроен на "Regular Conversion
 * launched by Software" в .ioc, никакого TIM4/EXTSEL) и отдаёт CPU на 1мс -
 * этого с огромным запасом хватает на завершение (реально ~50мкс при
 * 480 циклах выборки и текущем делителе ADC-клока). К моменту чтения
 * s_adc_raw результат гарантированно готов. */
volatile uint16_t s_adc_raw;

static float s_filtered_v = 0.0f;
static bool s_filter_primed = false;
static uint8_t s_last_percent = 0U;

/**
 * @brief Один шаг однополюсного БИХ-фильтра (экспоненциальное среднее):
 *        y[n] = y[n-1] + alpha*(x[n]-y[n-1]).
 */
static inline float battery_iir_step(float prev, float sample, float alpha)
{
    return prev + alpha * (sample - prev);
}

/**
 * @brief Линейная интерполяция напряжения в проценты заряда с насыщением по
 *        краям диапазона [BATTERY_VOLTAGE_EMPTY_V; BATTERY_VOLTAGE_FULL_V].
 */
static inline uint8_t battery_voltage_to_percent(float voltage_v)
{
    if (voltage_v <= BATTERY_VOLTAGE_EMPTY_V) {
        return 0U;
    }
    if (voltage_v >= BATTERY_VOLTAGE_FULL_V) {
        return 100U;
    }

    float span = BATTERY_VOLTAGE_FULL_V - BATTERY_VOLTAGE_EMPTY_V;
    float percent = (voltage_v - BATTERY_VOLTAGE_EMPTY_V) / span * 100.0f;
    return (uint8_t)(percent + 0.5f);
}

/**
 * @brief Триггерит одну конверсию (software start, т.к. ADC1.ExternalTrigConv
 *        = ADC_SOFTWARE_START в .ioc) и сразу возвращает управление - HAL
 *        внутри выставляет ADC_CR2_SWSTART и не ждёт завершения.
 */
static void battery_trigger_conversion(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&s_adc_raw, 1);
}

// Тело задачи
static void vTask_battery_ctrl_BodyFunction(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(BATTERY_TASK_PERIOD_MS);

    for (;;)
    {
        battery_trigger_conversion();
        vTaskDelay(pdMS_TO_TICKS(1)); // отдаём CPU, конверсия+DMA завершатся за десятки мкс

        uint16_t raw = s_adc_raw;
        float voltage_v = (float)raw * BATTERY_ADC_VREF_V / BATTERY_ADC_MAX_RAW;

        if (!s_filter_primed) {
            /* Первое чтение - сразу инициализируем фильтр реальным значением,
             * иначе он минуту "разгонялся" бы от 0В и показывал ложный 0%. */
            s_filtered_v = voltage_v;
            s_filter_primed = true;
        } else {
            s_filtered_v = battery_iir_step(s_filtered_v, voltage_v, BATTERY_IIR_ALPHA);
        }

        s_last_percent = battery_voltage_to_percent(s_filtered_v);
        Telemetry_SetBattery(s_last_percent);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
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

uint8_t Task_battery_ctrl_GetPercent(void)
{
    return s_last_percent;
}
