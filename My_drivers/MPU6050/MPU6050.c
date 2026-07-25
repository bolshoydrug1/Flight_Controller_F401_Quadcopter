/*
 * MPU6050.c
 *
 *  Created on: Jul 8, 2026
 *      Author: konstantin1
 */

#include "MPU6050.h"
#include <string.h>

/* Private Functions ---------------------------------------------------------*/

/**
  * @brief  CRC32 (poly 0xEDB88320, как в Ethernet/zip) программно, без
  *         аппаратного модуля CRC (в проекте не задействован).
  */
static uint32_t MPU6050_CRC32(const uint8_t* data, uint32_t len) {
	uint32_t crc = 0xFFFFFFFFU;

	for (uint32_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (crc & 1U) {
				crc = (crc >> 1) ^ 0xEDB88320U;
			} else {
				crc >>= 1;
			}
		}
	}

	return ~crc;
}

/**
  * @brief  Запись в один регистр (синхронно).
  */
static HAL_StatusTypeDef MPU6050_WriteReg(MPU6050_HandleTypeDef* mpu, uint8_t reg, uint8_t value) {
	return HAL_I2C_Mem_Write(mpu->hi2c, mpu->dev_address, reg, I2C_MEMADD_SIZE_8BIT,
			&value, 1, MPU6050_I2C_TIMEOUT_MS);
}

/**
  * @brief  Чтение из одного регистра (синхронно).
  */
static HAL_StatusTypeDef MPU6050_ReadReg(MPU6050_HandleTypeDef* mpu, uint8_t reg, uint8_t* value) {
	return HAL_I2C_Mem_Read(mpu->hi2c, mpu->dev_address, reg, I2C_MEMADD_SIZE_8BIT,
			value, 1, MPU6050_I2C_TIMEOUT_MS);
}

/**
  * @brief  Чтение нескольких регистров подряд (синхронно, burst read).
  */
static HAL_StatusTypeDef MPU6050_ReadRegs(MPU6050_HandleTypeDef* mpu, uint8_t reg, uint8_t* data, uint16_t size) {
	return HAL_I2C_Mem_Read(mpu->hi2c, mpu->dev_address, reg, I2C_MEMADD_SIZE_8BIT,
			data, size, MPU6050_I2C_TIMEOUT_MS);
}

/**
  * @brief  Переводит значение FS_SEL/AFS_SEL (регистровые макросы) в чувствительность.
  */
static float MPU6050_GyroSensFromFS(uint8_t gyro_fs) {
	switch (gyro_fs) {
		case MPU6050_GYRO_FS_250:  return 131.0f;
		case MPU6050_GYRO_FS_500:  return 65.5f;
		case MPU6050_GYRO_FS_1000: return 32.8f;
		case MPU6050_GYRO_FS_2000: return 16.4f;
		default:                   return 131.0f;
	}
}

static float MPU6050_AccelSensFromFS(uint8_t accel_fs) {
	switch (accel_fs) {
		case MPU6050_ACCEL_FS_2G:  return 16384.0f;
		case MPU6050_ACCEL_FS_4G:  return 8192.0f;
		case MPU6050_ACCEL_FS_8G:  return 4096.0f;
		case MPU6050_ACCEL_FS_16G: return 2048.0f;
		default:                   return 16384.0f;
	}
}

/* Public Functions ----------------------------------------------------------*/

/**
  * @brief  Чтение WHO_AM_I.
  */
HAL_StatusTypeDef MPU6050_ReadID(MPU6050_HandleTypeDef* mpu, uint8_t* id) {
	return MPU6050_ReadReg(mpu, MPU6050_REG_WHO_AM_I, id);
}

/**
  * @brief  Программный сброс устройства.
  */
HAL_StatusTypeDef MPU6050_Reset(MPU6050_HandleTypeDef* mpu) {
	HAL_StatusTypeDef status;

	status = MPU6050_WriteReg(mpu, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET);
	if (status != HAL_OK) return status;
	HAL_Delay(100);

	status = MPU6050_WriteReg(mpu, MPU6050_REG_SIGNAL_PATH_RST, 0x07);
	HAL_Delay(100);

	return status;
}

/**
  * @brief  Установка чувствительности гироскопа.
  */
HAL_StatusTypeDef MPU6050_SetGyroFullScale(MPU6050_HandleTypeDef* mpu, uint8_t gyro_fs) {
	HAL_StatusTypeDef status = MPU6050_WriteReg(mpu, MPU6050_REG_GYRO_CONFIG, gyro_fs);
	if (status != HAL_OK) return status;

	mpu->gyro_fs = gyro_fs;
	mpu->gyro_sens_lsb = MPU6050_GyroSensFromFS(gyro_fs);
	return HAL_OK;
}

/**
  * @brief  Установка чувствительности акселерометра.
  */
HAL_StatusTypeDef MPU6050_SetAccelFullScale(MPU6050_HandleTypeDef* mpu, uint8_t accel_fs) {
	HAL_StatusTypeDef status = MPU6050_WriteReg(mpu, MPU6050_REG_ACCEL_CONFIG, accel_fs);
	if (status != HAL_OK) return status;

	mpu->accel_fs = accel_fs;
	mpu->accel_sens_lsb = MPU6050_AccelSensFromFS(accel_fs);
	return HAL_OK;
}

/**
  * @brief  Установка встроенного ФНЧ (CONFIG.DLPF_CFG), общего для гироскопа и акселерометра.
  */
HAL_StatusTypeDef MPU6050_SetDLPF(MPU6050_HandleTypeDef* mpu, uint8_t dlpf_cfg) {
	HAL_StatusTypeDef status = MPU6050_WriteReg(mpu, MPU6050_REG_CONFIG, dlpf_cfg);
	if (status != HAL_OK) return status;

	mpu->dlpf_cfg = dlpf_cfg;
	return HAL_OK;
}

/**
  * @brief  Установка делителя частоты дискретизации (SMPLRT_DIV).
  * @note   Sample Rate = GyroOutputRate / (1 + div), где GyroOutputRate = 8кГц
  *         при DLPF выключенном (MPU6050_DLPF_260HZ) и 1кГц при включённом.
  */
HAL_StatusTypeDef MPU6050_SetSampleRateDiv(MPU6050_HandleTypeDef* mpu, uint8_t div) {
	HAL_StatusTypeDef status = MPU6050_WriteReg(mpu, MPU6050_REG_SMPLRT_DIV, div);
	if (status != HAL_OK) return status;

	mpu->sample_rate_div = div;
	return HAL_OK;
}

/**
  * @brief  Включает прерывание Data Ready на пине INT (push-pull, активный высокий,
  *         импульс 50мкс - см. MPU6050_INTCFG_x для другого поведения).
  */
HAL_StatusTypeDef MPU6050_EnableDataReadyInterrupt(MPU6050_HandleTypeDef* mpu) {
	HAL_StatusTypeDef status = MPU6050_WriteReg(mpu, MPU6050_REG_INT_PIN_CFG, 0x00);
	if (status != HAL_OK) return status;

	return MPU6050_WriteReg(mpu, MPU6050_REG_INT_ENABLE, MPU6050_INT_DATA_RDY_EN);
}

/**
  * @brief  Инициализация MPU6050. Также восстанавливает mpu->accel_offset
  *         из flash (MPU6050_LoadAccelOffsetFromFlash), если там есть
  *         валидная запись - см. MPU6050_SaveAccelOffsetToFlash.
  * @param  mpu: Указатель на структуру MPU6050 (должны быть заполнены hi2c, dev_address,
  *         gyro_fs, accel_fs, dlpf_cfg, sample_rate_div, clock_source перед вызовом).
  * @retval HAL статус.
  */
HAL_StatusTypeDef MPU6050_Init(MPU6050_HandleTypeDef* mpu) {
	uint8_t id = 0;
	HAL_StatusTypeDef status;

	status = MPU6050_Reset(mpu);
	if (status != HAL_OK) return status;

	status = MPU6050_ReadID(mpu, &id);
	if (status != HAL_OK || id != MPU6050_WHO_AM_I_VALUE) {
		return HAL_ERROR;
	}

	// Выход из SLEEP + выбор источника тактирования (пробуждает устройство)
	status = MPU6050_WriteReg(mpu, MPU6050_REG_PWR_MGMT_1, mpu->clock_source & ~MPU6050_PWR1_SLEEP);
	if (status != HAL_OK) return status;
	HAL_Delay(10);

	status = MPU6050_WriteReg(mpu, MPU6050_REG_PWR_MGMT_2, 0x00);
	if (status != HAL_OK) return status;

	status = MPU6050_SetSampleRateDiv(mpu, mpu->sample_rate_div);
	if (status != HAL_OK) return status;

	status = MPU6050_SetDLPF(mpu, mpu->dlpf_cfg);
	if (status != HAL_OK) return status;

	status = MPU6050_SetGyroFullScale(mpu, mpu->gyro_fs);
	if (status != HAL_OK) return status;

	status = MPU6050_SetAccelFullScale(mpu, mpu->accel_fs);
	if (status != HAL_OK) return status;

	// Офсеты калибровки сбрасываются при каждой инициализации: гироскоп
	// обязан быть перекалиброван заново (MPU6050_CalibrateGyro). Офсет
	// акселерометра механический и стабилен между включениями, поэтому его
	// пытаемся восстановить из flash; если валидной записи нет (первое
	// включение, ещё не калибровался) - остаётся нулевым.
	mpu->gyro_offset.x = 0;
	mpu->gyro_offset.y = 0;
	mpu->gyro_offset.z = 0;
	mpu->accel_offset.x = 0;
	mpu->accel_offset.y = 0;
	mpu->accel_offset.z = 0;
	MPU6050_LoadAccelOffsetFromFlash(&mpu->accel_offset);

	return HAL_OK;
}

/**
  * @brief  Синхронное (блокирующее) чтение сырых данных accel+temp+gyro.
  */
HAL_StatusTypeDef MPU6050_GetRawData(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw) {
	uint8_t data[MPU6050_RAW_BURST_LEN];
	HAL_StatusTypeDef status;

	status = MPU6050_ReadRegs(mpu, MPU6050_REG_ACCEL_XOUT_H, data, MPU6050_RAW_BURST_LEN);
	if (status != HAL_OK) return status;

	MPU6050_ParseRawBuffer(data, raw);
	return HAL_OK;
}

/**
  * @brief  Синхронное чтение + масштабирование, результат сохраняется в mpu->raw/mpu->data.
  */
HAL_StatusTypeDef MPU6050_GetMeasuredData(MPU6050_HandleTypeDef* mpu) {
	HAL_StatusTypeDef status = MPU6050_GetRawData(mpu, &mpu->raw);
	if (status != HAL_OK) return status;

	MPU6050_ApplyGyroOffset(mpu, &mpu->raw);
	MPU6050_ApplyAccelOffset(mpu, &mpu->raw);
	MPU6050_ConvertData(mpu, &mpu->raw, &mpu->data);
	return HAL_OK;
}

/**
  * @brief  Неблокирующее чтение сырых данных по DMA. Запускает транзакцию и
  *         сразу возвращает управление — ожидание завершения и синхронизация
  *         с задачей выполняются вызывающим кодом (см. описание в MPU6050.h).
  */
HAL_StatusTypeDef MPU6050_ReadRawDataDMA(MPU6050_HandleTypeDef* mpu, uint8_t* rx_buf) {
	return HAL_I2C_Mem_Read_DMA(mpu->hi2c, mpu->dev_address, MPU6050_REG_ACCEL_XOUT_H,
			I2C_MEMADD_SIZE_8BIT, rx_buf, MPU6050_RAW_BURST_LEN);
}

/**
  * @brief  Разбор буфера MPU6050_RAW_BURST_LEN байт (big-endian) в MPU6050_RawData.
  */
void MPU6050_ParseRawBuffer(const uint8_t* rx_buf, MPU6050_RawData* raw) {
	raw->accel_x = (int16_t)((rx_buf[0]  << 8) | rx_buf[1]);
	raw->accel_y = (int16_t)((rx_buf[2]  << 8) | rx_buf[3]);
	raw->accel_z = (int16_t)((rx_buf[4]  << 8) | rx_buf[5]);
	raw->temp    = (int16_t)((rx_buf[6]  << 8) | rx_buf[7]);
	raw->gyro_x  = (int16_t)((rx_buf[8]  << 8) | rx_buf[9]);
	raw->gyro_y  = (int16_t)((rx_buf[10] << 8) | rx_buf[11]);
	raw->gyro_z  = (int16_t)((rx_buf[12] << 8) | rx_buf[13]);
}

/**
  * @brief  Перевод сырых данных в физические величины по текущей чувствительности mpu.
  */
void MPU6050_ConvertData(MPU6050_HandleTypeDef* mpu, const MPU6050_RawData* raw, MPU6050_ScaledData* out) {
	out->accel_x_g   = (float)raw->accel_x / mpu->accel_sens_lsb;
	out->accel_y_g   = (float)raw->accel_y / mpu->accel_sens_lsb;
	out->accel_z_g   = (float)raw->accel_z / mpu->accel_sens_lsb;
	out->gyro_x_dps  = (float)raw->gyro_x / mpu->gyro_sens_lsb;
	out->gyro_y_dps  = (float)raw->gyro_y / mpu->gyro_sens_lsb;
	out->gyro_z_dps  = (float)raw->gyro_z / mpu->gyro_sens_lsb;
	out->temp_c      = (float)raw->temp / 340.0f + 36.53f;
}

/**
  * @brief  Калибровка нуля гироскопа. Датчик должен быть неподвижен на
  *         протяжении всего вызова (блокирующий, синхронное чтение).
  */
HAL_StatusTypeDef MPU6050_CalibrateGyro(MPU6050_HandleTypeDef* mpu, uint16_t samples) {
	MPU6050_RawData raw;
	int32_t sum_x = 0, sum_y = 0, sum_z = 0;
	HAL_StatusTypeDef status;

	if (samples == 0) return HAL_ERROR;

	for (uint16_t i = 0; i < samples; i++) {
		status = MPU6050_GetRawData(mpu, &raw);
		if (status != HAL_OK) return status;

		sum_x += raw.gyro_x;
		sum_y += raw.gyro_y;
		sum_z += raw.gyro_z;

		HAL_Delay(MPU6050_CALIB_SAMPLE_DELAY_MS);
	}

	mpu->gyro_offset.x = (int16_t)(sum_x / samples);
	mpu->gyro_offset.y = (int16_t)(sum_y / samples);
	mpu->gyro_offset.z = (int16_t)(sum_z / samples);

	return HAL_OK;
}

/**
  * @brief  Вычитает офсет калибровки гироскопа из сырых данных (in-place).
  */
void MPU6050_ApplyGyroOffset(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw) {
	raw->gyro_x -= mpu->gyro_offset.x;
	raw->gyro_y -= mpu->gyro_offset.y;
	raw->gyro_z -= mpu->gyro_offset.z;
}

/**
  * @brief  Калибровка нуля акселерометра. Датчик должен лежать неподвижно и
  *         строго горизонтально осью Z вверх на протяжении всего вызова
  *         (блокирующий, синхронное чтение).
  */
HAL_StatusTypeDef MPU6050_CalibrateAccel(MPU6050_HandleTypeDef* mpu, uint16_t samples) {
	MPU6050_RawData raw;
	int32_t sum_x = 0, sum_y = 0, sum_z = 0;
	HAL_StatusTypeDef status;

	if (samples == 0) return HAL_ERROR;

	for (uint16_t i = 0; i < samples; i++) {
		status = MPU6050_GetRawData(mpu, &raw);
		if (status != HAL_OK) return status;

		sum_x += raw.accel_x;
		sum_y += raw.accel_y;
		sum_z += raw.accel_z;

		HAL_Delay(MPU6050_CALIB_SAMPLE_DELAY_MS);
	}

	// X/Y должны читать 0, Z - ровно +accel_sens_lsb (1g), т.к. датчик лежит
	// осью Z вверх.
	mpu->accel_offset.x = (int16_t)(sum_x / samples);
	mpu->accel_offset.y = (int16_t)(sum_y / samples);
	mpu->accel_offset.z = (int16_t)(sum_z / samples - (int32_t)mpu->accel_sens_lsb);

	return HAL_OK;
}

/**
  * @brief  Вычитает офсет калибровки акселерометра из сырых данных (in-place).
  */
void MPU6050_ApplyAccelOffset(MPU6050_HandleTypeDef* mpu, MPU6050_RawData* raw) {
	raw->accel_x -= mpu->accel_offset.x;
	raw->accel_y -= mpu->accel_offset.y;
	raw->accel_z -= mpu->accel_offset.z;
}

/**
  * @brief  Сохраняет офсет акселерометра во flash. См. предупреждение про
  *         блокировку МК на время erase+program в MPU6050.h.
  */
HAL_StatusTypeDef MPU6050_SaveAccelOffsetToFlash(const MPU6050_AxisOffset* offset) {
	MPU6050_AccelOffsetFlash record;
	uint32_t words[(sizeof(record) + 3U) / 4U];
	FLASH_EraseInitTypeDef erase_init;
	uint32_t sector_error = 0;
	HAL_StatusTypeDef status;

	record.magic = MPU6050_ACCEL_OFFSET_FLASH_MAGIC;
	record.offset = *offset;
	record.crc32 = MPU6050_CRC32((const uint8_t*)&record, offsetof(MPU6050_AccelOffsetFlash, crc32));

	memset(words, 0xFF, sizeof(words));
	memcpy(words, &record, sizeof(record));

	status = HAL_FLASH_Unlock();
	if (status != HAL_OK) return status;

	erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_init.Sector = MPU6050_ACCEL_OFFSET_FLASH_SECTOR;
	erase_init.NbSectors = 1;
	erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
	if (status != HAL_OK) {
		HAL_FLASH_Lock();
		return status;
	}

	for (uint32_t i = 0; i < (sizeof(words) / sizeof(words[0])); i++) {
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
				MPU6050_ACCEL_OFFSET_FLASH_ADDR + i * 4U, words[i]);
		if (status != HAL_OK) {
			HAL_FLASH_Lock();
			return status;
		}
	}

	return HAL_FLASH_Lock();
}

/**
  * @brief  Читает и валидирует офсет акселерометра из flash. Flash
  *         отображена в адресное пространство, поэтому чтение - обычный
  *         доступ по указателю, HAL здесь не нужен.
  */
HAL_StatusTypeDef MPU6050_LoadAccelOffsetFromFlash(MPU6050_AxisOffset* offset) {
	const MPU6050_AccelOffsetFlash* record =
			(const MPU6050_AccelOffsetFlash*)MPU6050_ACCEL_OFFSET_FLASH_ADDR;
	uint32_t crc;

	if (record->magic != MPU6050_ACCEL_OFFSET_FLASH_MAGIC) return HAL_ERROR;

	crc = MPU6050_CRC32((const uint8_t*)record, offsetof(MPU6050_AccelOffsetFlash, crc32));
	if (crc != record->crc32) return HAL_ERROR;

	*offset = record->offset;
	return HAL_OK;
}
