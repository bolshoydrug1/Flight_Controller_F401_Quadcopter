/*
 * Task_Telemetry.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_Telemetry.h"
#include "LoRa.h"
#include "Telemetry_Data.h"
#include "spi.h"
#include "task.h"

static TaskHandle_t xTask_telemetryHandle = NULL;
static LoRa g_lora;

#define TELEMETRY_TX_PERIOD_MS    50U
#define LORA_INIT_RETRY_DELAY_MS  500U
#define LORA_RX_BUF_LEN           64U

/* Безопасно относительно переполнения TickType_t: true, если тик a наступил
 * не раньше тика b. */
#define TICK_AFTER_OR_EQUAL(a, b)  ((int32_t)((a) - (b)) >= 0)

static bool lora_bridge_init_module(void)
{
    g_lora = newLoRa();
    g_lora.CS_port = Ra_01_SS_GPIO_Port;
    g_lora.CS_pin = Ra_01_SS_Pin;
    g_lora.reset_port = Ra_01_RST_GPIO_Port;
    g_lora.reset_pin = Ra_01_RST_Pin;
    g_lora.DIO0_port = Ra_01_DIO0_GPIO_Port;
    g_lora.DIO0_pin = Ra_01_DIO0_Pin;
    g_lora.hSPIx = &hspi2;

    /* Максимальная скорость: SF7, BW 500 kHz, CR 4/5, короткая преамбула */
    g_lora.spredingFactor = SF_7;
    g_lora.bandWidth = BW_500KHz;
    g_lora.crcRate = CR_4_5;
    g_lora.preamble = 6;
    g_lora.frequency = 433;
    g_lora.power = POWER_20db;

    LoRa_reset(&g_lora);
    if (LoRa_init(&g_lora) != LORA_OK) {
        return false;
    }

    LoRa_startReceiving(&g_lora);
    return true;
}

/**
 * @brief Читает пакет, пришедший по LoRa, и отдаёт его дальше.
 * Вызывается только после уведомления LORA_NOTIFY_RXDONE, поэтому не опрашивает
 * модуль в цикле - один короткий обмен по SPI и возврат.
 */
static void lora_handle_rx(void)
{
    uint8_t rx_buf[LORA_RX_BUF_LEN] = {0};
    uint8_t rx_len = LoRa_receive(&g_lora, rx_buf, sizeof(rx_buf));

    if (rx_len > 0U) {
        // TODO: разобрать команды/данные от другого передатчика (rx_buf, rx_len)
    }
}

// Тело задачи
static void vTask_telemetry_BodyFunction(void *pvParameters)
{
    while (!lora_bridge_init_module()) {
        vTaskDelay(pdMS_TO_TICKS(LORA_INIT_RETRY_DELAY_MS));
    }

    const TickType_t xPeriod = pdMS_TO_TICKS(TELEMETRY_TX_PERIOD_MS);
    TickType_t xNextTx = xTaskGetTickCount() + xPeriod;

    for (;;)
    {
        TickType_t xNow = xTaskGetTickCount();
        TickType_t xWait = TICK_AFTER_OR_EQUAL(xNow, xNextTx) ? 0 : (xNextTx - xNow);

        uint32_t notified = 0;
        /* Пока ждём момента следующей передачи, задача ничего не блокирует и
         * не потребляет CPU - она спит на xTaskNotifyWait и просыпается либо
         * по таймауту (пора передавать), либо сразу же по прерыванию DIO0,
         * если пришёл пакет от другого передатчика. */
        xTaskNotifyWait(0, LORA_NOTIFY_RXDONE, &notified, xWait);

        if (notified & LORA_NOTIFY_RXDONE) {
            lora_handle_rx();
        }

        if (TICK_AFTER_OR_EQUAL(xTaskGetTickCount(), xNextTx)) {
            TelemetryFrame_t frame;
            Telemetry_GetFrame(&frame);
            LoRa_transmit(&g_lora, (uint8_t *)&frame, sizeof(frame), TRANSMIT_TIMEOUT);
            xNextTx += xPeriod;
        }
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

void Task_telemetry_HandleExti(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != Ra_01_DIO0_Pin)
        return;

    LoRa_handleDio0IRQ(&g_lora);
}
