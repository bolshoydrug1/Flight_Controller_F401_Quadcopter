/*
 * MPU6050.h
 *
 *  Created on: Jul 8, 2026
 *      Author: konstantin1
 */

#ifndef MPU6050_MPU6050_H_
#define MPU6050_MPU6050_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Definitions ---------------------------------------------------------------*/
// I2C адрес устройства (уже сдвинут на 1 бит под HAL, зависит от уровня на пине AD0)
#define MPU6050_ADDR_AD0_LOW		(0x68 << 1)
#define MPU6050_ADDR_AD0_HIGH		(0x69 << 1)

// Адреса регистров (из даташита / Register Map)
#define MPU6050_REG_SMPLRT_DIV		0x19
#define MPU6050_REG_CONFIG			0x1A
#define MPU6050_REG_GYRO_CONFIG		0x1B
#define MPU6050_REG_ACCEL_CONFIG	0x1C
#define MPU6050_REG_INT_PIN_CFG		0x37
#define MPU6050_REG_INT_ENABLE		0x38
#define MPU6050_REG_INT_STATUS		0x3A
#define MPU6050_REG_ACCEL_XOUT_H	0x3B
#define MPU6050_REG_TEMP_OUT_H		0x41
#define MPU6050_REG_GYRO_XOUT_H		0x43
#define MPU6050_REG_SIGNAL_PATH_RST	0x68
#define MPU6050_REG_USER_CTRL		0x6A
#define MPU6050_REG_PWR_MGMT_1		0x6B
#define MPU6050_REG_PWR_MGMT_2		0x6C
#define MPU6050_REG_WHO_AM_I		0x75

#define MPU6050_WHO_AM_I_VALUE		0x68

// Число байт в непрерывном блоке ACCEL_XOUT_H..GYRO_ZOUT_L (accel[6] + temp[2] + gyro[6])
#define MPU6050_RAW_BURST_LEN		14

// PWR_MGMT_1
#define MPU6050_PWR1_DEVICE_RESET	0x80
#define MPU6050_PWR1_SLEEP			0x40

// PWR_MGMT_1 CLKSEL (биты 2:0) — источник тактирования
#define MPU6050_CLOCK_INTERNAL_8MHZ	0x00
#define MPU6050_CLOCK_PLL_XGYRO		0x01 // рекомендуется производителем
#define MPU6050_CLOCK_PLL_YGYRO		0x02
#define MPU6050_CLOCK_PLL_ZGYRO		0x03
#define MPU6050_CLOCK_PLL_EXT32K	0x04
#define MPU6050_CLOCK_PLL_EXT19M	0x05
#define MPU6050_CLOCK_STOP			0x07

// Чувствительность гироскопа (GYRO_CONFIG, биты FS_SEL[4:3])
#define MPU6050_GYRO_FS_250		0x00 // ±250 °/s
#define MPU6050_GYRO_FS_500		0x08 // ±500 °/s
#define MPU6050_GYRO_FS_1000		0x10 // ±1000 °/s
#define MPU6050_GYRO_FS_2000		0x18 // ±2000 °/s

// Чувствительность акселерометра (ACCEL_CONFIG, биты AFS_SEL[4:3])
#define MPU6050_ACCEL_FS_2G		0x00 // ±2g
#define MPU6050_ACCEL_FS_4G		0x08 // ±4g
#define MPU6050_ACCEL_FS_8G		0x10 // ±8g
#define MPU6050_ACCEL_FS_16G		0x18 // ±16g

// Настройка встроенного цифрового ФНЧ (CONFIG, биты DLPF_CFG[2:0]).
// Общий для гироскопа и акселерометра, значения — из таблицы даташита
// (полоса пропускания гироскопа / частота дискретизации Fs).
#define MPU6050_DLPF_260HZ		0x00 // Gyro 256Hz, Fs=8kHz (фильтр выключен)
#define MPU6050_DLPF_184HZ		0x01 // Gyro 188Hz, Fs=1kHz
#define MPU6050_DLPF_94HZ		0x02 // Gyro 98Hz,  Fs=1kHz
#define MPU6050_DLPF_44HZ		0x03 // Gyro 42Hz,  Fs=1kHz
#define MPU6050_DLPF_21HZ		0x04 // Gyro 20Hz,  Fs=1kHz
#define MPU6050_DLPF_10HZ		0x05 // Gyro 10Hz,  Fs=1kHz
#define MPU6050_DLPF_5HZ		0x06 // Gyro 5Hz,   Fs=1kHz

// INT_ENABLE
#define MPU6050_INT_DATA_RDY_EN		0x01

// INT_PIN_CFG
#define MPU6050_INTCFG_LATCH_INT_EN	0x20 // 1 - держать INT до чтения INT_STATUS, 0 - импульс 50мкс
#define MPU6050_INTCFG_OPEN_DRAIN	0x40
#define MPU6050_INTCFG_INT_LEVEL_LOW	0x80

// Таймаут одной I2C-транзакции регистрового доступа (синхронные Init/Set-функции)
#define MPU6050_I2C_TIMEOUT_MS		10

// Задержка между отсчётами при усреднении в MPU6050_CalibrateGyro/CalibrateAccel
#define MPU6050_CALIB_SAMPLE_DELAY_MS	1

// Хранение офсета акселерометра во flash.
// Сектор 5 (последний, 128K) исключён из области линковки в
// STM32F401RCTX_FLASH.ld — линкер никогда не разместит туда код/rodata/data,
// независимо от того, насколько вырастет прошивка.
#define MPU6050_ACCEL_OFFSET_FLASH_SECTOR	FLASH_SECTOR_5
#define MPU6050_ACCEL_OFFSET_FLASH_ADDR	0x08020000U
#define MPU6050_ACCEL_OFFSET_FLASH_MAGIC	0x4D505541U // "MPUA"

/* Type Definitions ----------------------------------------------------------*/

// Сырые данные, как они лежат в регистрах (big-endian int16 из даташита)
typedef struct {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int16_t temp;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
} MPU6050_RawData;

// Офсет калибровки одной тройки осей, в LSB сырых значений
typedef struct {
	int16_t x;
	int16_t y;
	int16_t z;
} MPU6050_AxisOffset;

// Формат записи офсета акселерометра во flash. magic и crc32 нужны, чтобы
// отличить валидную запись от стёртой (0xFF...) или "пустой" (0x00...) flash.
typedef struct {
	uint32_t magic;			// MPU6050_ACCEL_OFFSET_FLASH_MAGIC
	MPU6050_AxisOffset offset;
	uint32_t crc32;			// CRC32 по полям magic+offset
} MPU6050_AccelOffsetFlash;

// Данные, переведённые в физические величины
typedef struct {
	float accel_x_g;
	float accel_y_g;
	float accel_z_g;
	float gyro_x_dps;
	float gyro_y_dps;
	float gyro_z_dps;
	float temp_c;
} MPU6050_ScaledData;

// Структура объекта MPU6050
typedef struct {
	I2C_HandleTypeDef* hi2c;		// Указатель на I2C (должен быть настроен, для DMA-чтения - с DMA на Rx)
	uint16_t dev_address;			// I2C адрес (MPU6050_ADDR_AD0_LOW/HIGH)

	// Конфигурация, задаётся перед MPU6050_Init через макросы выше
	uint8_t gyro_fs;			// MPU6050_GYRO_FS_x
	uint8_t accel_fs;			// MPU6050_ACCEL_FS_x
	uint8_t dlpf_cfg;			// MPU6050_DLPF_x (встроенный ФНЧ)
	uint8_t sample_rate_div;		// SMPLRT_DIV
	uint8_t clock_source;			// MPU6050_CLOCK_x

	float gyro_sens_lsb;			// LSB на °/s, выводится из gyro_fs в MPU6050_Init
	float accel_sens_lsb;			// LSB на g, выводится из accel_fs в MPU6050_Init

	MPU6050_AxisOffset gyro_offset;		// офсет калибровки гироскопа, см. MPU6050_CalibrateGyro
	MPU6050_AxisOffset accel_offset;		// офсет калибровки акселерометра, см. MPU6050_CalibrateAccel

	MPU6050_RawData raw;			// Последние разобранные сырые данные
	MPU6050_ScaledData data;		// Последние скомпенсированные данные
} MPU6050_HandleTypeDef;

/* Function Prototypes -------------------------------------------------------*/
HAL_StatusTypeDef MPU6050_Init(MPU6050_HandleTypeDef* mpu);
HAL_StatusTypeDef MPU6050_ReadID(MPU6050_HandleTypeDef* mpu, uint8_t* id);
HAL_StatusTypeDef MPU6050_Reset(MPU6050_HandleTypeDef* mpu);

HAL_StatusTypeDef MPU6050_SetGyroFullScale(MPU6050_HandleTypeDef* mpu, uint8_t gyro_fs);
HAL_StatusTypeDef MPU6050_SetAccelFullScale(MPU6050_HandleTypeDef* mpu, uint8_t accel_fs);
HAL_StatusTypeDef MPU6050_SetDLPF(MPU6050_HandleTypeDef* mpu, uint8_t dlpf_cfg);
HAL_StatusTypeDef MPU6050_SetSampleRateDiv(MPU6050_HandleTypeDef* mpu, uint8_t div);
HAL_StatusTypeDef MPU6050_EnableDataReadyInterrupt(MPU6050_HandleTypeDef* mpu);

// Синхронное (блокирующее) чтение — для инициализации/отладки
HAL_StatusTypeDef MPU6050_GetRawData(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw);
HAL_StatusTypeDef MPU6050_GetMeasuredData(MPU6050_HandleTypeDef* mpu);

/**
 * @brief  Запускает чтение accel+temp+gyro (14 байт, начиная с ACCEL_XOUT_H)
 *         по DMA и немедленно возвращает управление, не дожидаясь завершения.
 * @note   Требует, чтобы hi2c->hdmarx был настроен (DMA на приём I2C).
 *         Завершение транзакции и синхронизация с задачей (семафор/notify)
 *         реализуются на уровне задачи в HAL_I2C_MemRxCpltCallback /
 *         HAL_I2C_ErrorCallback — драйвер их не переопределяет.
 * @param  mpu: Указатель на структуру MPU6050.
 * @param  rx_buf: Буфер минимум на MPU6050_RAW_BURST_LEN байт, живущий до
 *         завершения DMA-транзакции (обычно статический/член структуры задачи).
 * @retval HAL статус запуска DMA-транзакции (не результат самого чтения).
 */
HAL_StatusTypeDef MPU6050_ReadRawDataDMA(MPU6050_HandleTypeDef* mpu, uint8_t* rx_buf);

/**
 * @brief  Разбирает буфер, заполненный MPU6050_ReadRawDataDMA после завершения
 *         DMA-транзакции, в структуру сырых данных.
 * @param  rx_buf: Буфер, переданный в MPU6050_ReadRawDataDMA (MPU6050_RAW_BURST_LEN байт).
 * @param  raw: Куда записать разобранные данные.
 */
void MPU6050_ParseRawBuffer(const uint8_t* rx_buf, MPU6050_RawData* raw);

/**
 * @brief  Переводит сырые данные в физические величины (g, °/s, °C) с учётом
 *         текущей чувствительности (gyro_sens_lsb/accel_sens_lsb) из mpu.
 */
void MPU6050_ConvertData(MPU6050_HandleTypeDef* mpu, const MPU6050_RawData* raw, MPU6050_ScaledData* out);

/**
 * @brief  Калибровка нуля гироскопа. Датчик должен быть неподвижен.
 *         Синхронно (блокирующе) усредняет `samples` сырых отсчётов и
 *         сохраняет смещение в mpu->gyro_offset.
 * @note   Вызывать при каждом включении, сразу после MPU6050_Init, пока
 *         аппарат не начал двигаться — bias гироскопа не стабилен между
 *         включениями и с температурой, персистентное хранение не имеет
 *         смысла, полная пересборка на старте дешевле и надёжнее.
 * @param  samples: Число отсчётов для усреднения (типично 1000-2000).
 * @retval HAL статус.
 */
HAL_StatusTypeDef MPU6050_CalibrateGyro(MPU6050_HandleTypeDef* mpu, uint16_t samples);

/**
 * @brief  Вычитает mpu->gyro_offset из сырых значений гироскопа (in-place).
 *         Вызывать на каждом рабочем чтении перед MPU6050_ConvertData - как
 *         после MPU6050_GetRawData, так и после MPU6050_ParseRawBuffer в
 *         DMA-пути. Внутри MPU6050_GetMeasuredData вызывается автоматически.
 */
void MPU6050_ApplyGyroOffset(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw);

/**
 * @brief  Калибровка нуля акселерометра. Датчик должен лежать неподвижно и
 *         строго горизонтально осью Z вверх (штатное положение покоя).
 *         Синхронно (блокирующе) усредняет `samples` сырых отсчётов и
 *         подбирает смещение так, чтобы после MPU6050_ApplyAccelOffset X/Y
 *         давали 0, а Z - ровно +1g (в текущих единицах accel_sens_lsb).
 * @note   В отличие от гироскопа, вызывать не на каждом включении, а по
 *         внешней команде (например, с земли по LoRa) - офсет акселерометра
 *         механический и стабилен между включениями. Сама функция ничего не
 *         пишет во flash - если результат нужно сохранить между включениями,
 *         вызывающий код должен после успешной калибровки явно вызвать
 *         MPU6050_SaveAccelOffsetToFlash(&mpu->accel_offset).
 * @param  samples: Число отсчётов для усреднения (типично 1000-2000).
 * @retval HAL статус.
 */
HAL_StatusTypeDef MPU6050_CalibrateAccel(MPU6050_HandleTypeDef* mpu, uint16_t samples);

/**
 * @brief  Вычитает mpu->accel_offset из сырых значений акселерометра (in-place).
 *         Вызывать на каждом рабочем чтении перед MPU6050_ConvertData - как
 *         после MPU6050_GetRawData, так и после MPU6050_ParseRawBuffer в
 *         DMA-пути. Внутри MPU6050_GetMeasuredData вызывается автоматически.
 */
void MPU6050_ApplyAccelOffset(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw);

/**
 * @brief  Сохраняет офсет акселерометра во flash (сектор
 *         MPU6050_ACCEL_OFFSET_FLASH_SECTOR), поверх предыдущей записи.
 * @note   На STM32F401 (однобанковая flash) на время erase+program
 *         (счёт секунд для сектора 128К - см. Datasheet, Table Flash memory
 *         erase time) выполнение кода из flash полностью останавливается на
 *         аппаратном уровне, включая тики FreeRTOS - это не критическая
 *         секция, а стоп всего МК. Вызывать только на земле, до/после
 *         полёта, никогда в воздухе (см. MPU6050_CalibrateAccel).
 * @param  offset: Офсет для сохранения (обычно mpu->accel_offset сразу
 *         после успешной MPU6050_CalibrateAccel).
 * @retval HAL_OK при успешной записи, иначе HAL_ERROR.
 */
HAL_StatusTypeDef MPU6050_SaveAccelOffsetToFlash(const MPU6050_AxisOffset* offset);

/**
 * @brief  Читает офсет акселерометра из flash с проверкой валидности
 *         (magic + CRC32). Вызывается из MPU6050_Init.
 * @param  offset: Куда записать прочитанный офсет. Не изменяется, если
 *         валидной записи во flash нет (первый запуск, стёртая flash).
 * @retval HAL_OK, если во flash найдена валидная запись и offset заполнен.
 *         HAL_ERROR, если записи нет/она повреждена - offset не изменяется,
 *         вызывающий код должен оставить смещение нулевым (как после Init).
 */
HAL_StatusTypeDef MPU6050_LoadAccelOffsetFromFlash(MPU6050_AxisOffset* offset);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_MPU6050_H_ */
