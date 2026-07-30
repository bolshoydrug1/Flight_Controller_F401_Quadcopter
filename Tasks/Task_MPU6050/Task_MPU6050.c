/*
 * Task_MPU6050.c
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#include "Task_MPU6050.h"

#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "i2c.h"

static TaskHandle_t xTask_mpu6050Handle = NULL;

#define MPU6050_SAMPLE_PERIOD_MS       1U
/* Реальная I2C1-транзакция (адресная фаза + 14 байт по DMA на 400кГц) с
 * запасом укладывается в ~0.5мс - 2 такта таймаута оставляют запас, но не
 * дают зависшей транзакции сожрать больше одного периода цикла. */
#define MPU6050_DMA_WAIT_TIMEOUT_MS    2U
#define MPU6050_INIT_RETRY_DELAY_MS    500U
#define MPU6050_GYRO_CALIB_SAMPLES     1000U

/* Уведомления (task notification bits) от HAL_I2C_MemRxCpltCallback /
 * HAL_I2C_ErrorCallback (см. ниже). Единственный владелец - эта задача. */
#define MPU_NOTIFY_RX_DONE      (1UL << 0)
#define MPU_NOTIFY_RX_ERROR     (1UL << 1)

static MPU6050_HandleTypeDef g_mpu;
/* Буфер под DMA обязан жить до завершения транзакции - static, не стек. */
static uint8_t s_rx_buf[MPU6050_RAW_BURST_LEN];

/* Мейлбокс на 1 сообщение: запрос калибровки акселерометра извне (см.
 * Task_mpu6050_RequestAccelCalibration). xQueueOverwrite делает публикацию
 * атомарной без критических секций на стороне вызывающего кода. */
static QueueHandle_t s_calib_request_q = NULL;
static volatile bool s_calibrating_accel = false;

/* Последние скомпенсированные данные - под мьютексом, читаются другими
 * задачами через Task_mpu6050_GetData (по аналогии с Telemetry_Data.c). */
static SemaphoreHandle_t s_data_mutex = NULL;
static MPU6050_ScaledData s_last_data;

static bool mpu6050_bridge_init_module(void)
{
    g_mpu.hi2c = &hi2c1;
    g_mpu.dev_address = MPU6050_ADDR_AD0_LOW;

    /* Параметры ниже - типичные для полётного контроллера, легко поменять:
     * поля g_mpu.* можно задать другими макросами до MPU6050_Init. */
    g_mpu.gyro_fs = MPU6050_GYRO_FS_2000;
    g_mpu.accel_fs = MPU6050_ACCEL_FS_8G;
    /* DLPF обязателен включённым (не MPU6050_DLPF_260HZ) - иначе внутренний
     * Fs гироскопа 8кГц, а не 1кГц, и SMPLRT_DIV=0 даст не тот темп. */
    g_mpu.dlpf_cfg = MPU6050_DLPF_184HZ;
    /* Sample Rate = 1кГц(GyroOutputRate при включённом DLPF) / (1+0) = 1кГц,
     * ровно частота цикла задачи. */
    g_mpu.sample_rate_div = 0;
    g_mpu.clock_source = MPU6050_CLOCK_PLL_XGYRO;

    if (MPU6050_Init(&g_mpu) != HAL_OK) {
        return false;
    }

    /* Калибровка гироскопа - при каждом старте программы, блокирующе (датчик
     * должен быть неподвижен). Результат остаётся только в ОЗУ
     * (g_mpu.gyro_offset), см. MPU6050_CalibrateGyro - персистентное
     * хранение для гироскопа не имеет смысла. */
    if (MPU6050_CalibrateGyro(&g_mpu, MPU6050_GYRO_CALIB_SAMPLES) != HAL_OK) {
        return false;
    }

    if (MPU6050_CalibrateAccel(&g_mpu, MPU6050_GYRO_CALIB_SAMPLES) != HAL_OK) {
        return false;
    }

    return true;
}

/**
 * @brief Публикует свежие данные для Task_mpu6050_GetData. Некритично, если
 *        мьютекс занят - следующий такт (через 1мс) перезапишет.
 */
static void mpu6050_publish_data(void)
{
    if (s_data_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(s_data_mutex, 0) == pdTRUE) {
        s_last_data = g_mpu.data;
        xSemaphoreGive(s_data_mutex);
    }
}

/**
 * @brief Один такт неблокирующего чтения: запускает DMA-транзакцию и ждёт
 *        её завершения через task notification (задача не занимает CPU,
 *        а спит на xTaskNotifyWait). Вызывать из тела задачи раз в период.
 */
static void mpu6050_read_tick(void)
{
    if (MPU6050_ReadRawDataDMA(&g_mpu, s_rx_buf) != HAL_OK) {
        /* HAL_BUSY (предыдущая транзакция ещё не закрылась) или ошибка
         * запуска - пропускаем такт, следующий вызов попробует снова. */
        return;
    }

    uint32_t notified = 0;
    BaseType_t got = xTaskNotifyWait(MPU_NOTIFY_RX_DONE | MPU_NOTIFY_RX_ERROR,
                                      MPU_NOTIFY_RX_DONE | MPU_NOTIFY_RX_ERROR,
                                      &notified,
                                      pdMS_TO_TICKS(MPU6050_DMA_WAIT_TIMEOUT_MS));

    if (got != pdTRUE || (notified & MPU_NOTIFY_RX_DONE) == 0U) {
        /* Таймаут или HAL_I2C_ErrorCallback (NACK/BERR/ARLO/OVR) - данные не
         * готовы, пропускаем такт. Восстановление уровня HAL - через
         * I2C1_ER_IRQHandler/HAL, состояние hi2c1 само вернётся в READY. */
        return;
    }

    MPU6050_ParseRawBuffer(s_rx_buf, &g_mpu.raw);
    MPU6050_ApplyGyroOffset(&g_mpu, &g_mpu.raw);
    MPU6050_ApplyAccelOffset(&g_mpu, &g_mpu.raw);
    MPU6050_ConvertData(&g_mpu, &g_mpu.raw, &g_mpu.data);

    mpu6050_publish_data();
}

/**
 * @brief Обслуживает запрос калибровки акселерометра, если он есть
 *        (неблокирующая проверка). Сама калибровка + запись во flash -
 *        блокирующие, см. Task_mpu6050_RequestAccelCalibration.
 * @retval true, если калибровка была выполнена в этом такте (вызывающий
 *         код должен ресинхронизировать фазу 1кГц цикла).
 */
static bool mpu6050_service_calib_request(void)
{
    uint16_t samples;

    if (s_calib_request_q == NULL || xQueueReceive(s_calib_request_q, &samples, 0) != pdTRUE) {
        return false;
    }

    s_calibrating_accel = true;
    if (MPU6050_CalibrateAccel(&g_mpu, samples) == HAL_OK) {
        MPU6050_SaveAccelOffsetToFlash(&g_mpu.accel_offset);
    }
    s_calibrating_accel = false;

    return true;
}

// Тело задачи
static void vTask_mpu6050_BodyFunction(void *pvParameters)
{
    while (!mpu6050_bridge_init_module()) {
        vTaskDelay(pdMS_TO_TICKS(MPU6050_INIT_RETRY_DELAY_MS));
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(MPU6050_SAMPLE_PERIOD_MS);

    for (;;)
    {
        if (mpu6050_service_calib_request()) {
            /* Калибровка заняла секунды (усреднение + блокирующая запись во
             * flash) - не пытаемся нагнать пропущенные такты одним махом,
             * просто стартуем 1кГц фазу заново. */
            xLastWakeTime = xTaskGetTickCount();
        } else {
            mpu6050_read_tick();
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void Task_mpu6050_Start(void)
{
    // Если задача уже существует (не удалена) — не создаём новую
    if (Task_mpu6050_IsRunning())
        return;

    if (s_calib_request_q == NULL) {
        s_calib_request_q = xQueueCreate(1, sizeof(uint16_t));
    }
    if (s_data_mutex == NULL) {
        s_data_mutex = xSemaphoreCreateMutex();
    }

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
    	xTask_mpu6050Handle = NULL;
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

bool Task_mpu6050_GetData(MPU6050_ScaledData *out)
{
    if (out == NULL || s_data_mutex == NULL) {
        return false;
    }

    bool ok = false;
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        *out = s_last_data;
        xSemaphoreGive(s_data_mutex);
        ok = true;
    }
    return ok;
}

bool Task_mpu6050_RequestAccelCalibration(uint16_t samples)
{
    if (samples == 0U || s_calib_request_q == NULL) {
        return false;
    }
    return xQueueOverwrite(s_calib_request_q, &samples) == pdTRUE;
}

bool Task_mpu6050_IsCalibratingAccel(void)
{
    return s_calibrating_accel;
}

/*
 * HAL_I2C_MemRxCpltCallback/HAL_I2C_ErrorCallback - weak-символы HAL, на весь
 * линковочный образ можно переопределить только один раз (см. аналогичное
 * замечание про HAL_GPIO_EXTI_Callback в Task_HC_SR04.c). Пока по DMA на I2C
 * читается только MPU6050 на I2C1, поэтому диспетчеризация по hi2c->Instance
 * тривиальна; если появится ещё один DMA-потребитель I2C1, диспетчер нужно
 * будет расширить по аналогии с HAL_GPIO_EXTI_Callback.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1 || xTask_mpu6050Handle == NULL) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(xTask_mpu6050Handle, MPU_NOTIFY_RX_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1 || xTask_mpu6050Handle == NULL) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(xTask_mpu6050Handle, MPU_NOTIFY_RX_ERROR, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
