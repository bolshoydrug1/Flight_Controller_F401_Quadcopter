/*
 * bmp280.h
 *
 *  Created on: Mar 8, 2026
 *      Author: konstantin1
 */

#ifndef INC_BMP280_H_
#define INC_BMP280_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Definitions ---------------------------------------------------------------*/
// Адреса регистров (из даташита)
#define BMP280_REG_TEMP_XLSB        0xFC
#define BMP280_REG_TEMP_LSB         0xFB
#define BMP280_REG_TEMP_MSB         0xFA
#define BMP280_REG_PRESS_XLSB       0xF9
#define BMP280_REG_PRESS_LSB        0xF8
#define BMP280_REG_PRESS_MSB        0xF7
#define BMP280_REG_CONFIG           0xF5
#define BMP280_REG_CTRL_MEAS        0xF4
#define BMP280_REG_STATUS           0xF3
#define BMP280_REG_RESET             0xE0
#define BMP280_REG_ID                0xD0
#define BMP280_REG_CALIB_START       0x88
#define BMP280_REG_CALIB_END         0xA1

// Значения для инициализации
#define BMP280_RESET_VALUE           0xB6
#define BMP280_CHIP_ID               0x58

// Режимы работы (osrs_t, osrs_p, mode)
#define BMP280_MODE_SLEEP            0x00
#define BMP280_MODE_FORCED           0x01
#define BMP280_MODE_NORMAL           0x03

#define BMP280_OSRS_T_SKIPPED        0x00
#define BMP280_OSRS_T_X1             0x20 // 0010 0000
#define BMP280_OSRS_T_X2             0x40 // 0100 0000
#define BMP280_OSRS_T_X4             0x60 // 0110 0000
#define BMP280_OSRS_T_X8             0x80 // 1000 0000
#define BMP280_OSRS_T_X16            0xA0 // 1010 0000

#define BMP280_OSRS_P_SKIPPED        0x00
#define BMP280_OSRS_P_X1             0x04 // 0000 0100
#define BMP280_OSRS_P_X2             0x08 // 0000 1000
#define BMP280_OSRS_P_X4             0x0C // 0000 1100
#define BMP280_OSRS_P_X8             0x10 // 0001 0000
#define BMP280_OSRS_P_X16            0x14 // 0001 0100

// Настройки конфигурации (t_sb, filter, spi3w_en)
#define BMP280_T_SB_0_5_MS           0x00
#define BMP280_T_SB_62_5_MS          0x20 // 0010 0000
#define BMP280_T_SB_125_MS           0x40 // 0100 0000
#define BMP280_T_SB_250_MS           0x60 // 0110 0000
#define BMP280_T_SB_500_MS           0x80 // 1000 0000
#define BMP280_T_SB_1000_MS          0xA0 // 1010 0000
#define BMP280_T_SB_2000_MS          0xC0 // 1100 0000
#define BMP280_T_SB_4000_MS          0xE0 // 1110 0000

#define BMP280_FILTER_OFF            0x00
#define BMP280_FILTER_2              0x04 // 0000 0100
#define BMP280_FILTER_4              0x08 // 0000 1000
#define BMP280_FILTER_8              0x0C // 0000 1100
#define BMP280_FILTER_16             0x10 // 0001 0000

#define BMP280_SPI3W_DISABLE         0x00
#define BMP280_SPI3W_ENABLE          0x01

// Время ожидания измерения в зависимости от oversampling (в мс)
#define BMP280_MEAS_TIME_ULTRA_LOW   6   // ×1
#define BMP280_MEAS_TIME_LOW         8   // ×2
#define BMP280_MEAS_TIME_STANDARD    12  // ×4
#define BMP280_MEAS_TIME_HIGH        20  // ×8
#define BMP280_MEAS_TIME_ULTRA_HIGH  38  // ×16

/* Type Definitions ----------------------------------------------------------*/
// Структура для хранения калибровочных коэффициентов
typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} BMP280_CalibData;

// Структура для хранения сырых данных
typedef struct {
    int32_t adc_temp;
    int32_t adc_press;
} BMP280_RawData;

// Структура для хранения скомпенсированных данных
typedef struct {
    int32_t temperature;  // Температура в градусах Цельсия * 100 (2345 = 23.45°C)
    uint32_t pressure;     // Давление в Паскалях (101325 = 1013.25 гПа)
} BMP280_CompensatedData;

// Структура объекта BMP280
typedef struct {
    SPI_HandleTypeDef* hspi;        // Указатель на SPI
    GPIO_TypeDef* cs_port;           // Порт для CS (CSB)
    uint16_t cs_pin;                  // Пин для CS (CSB)
    BMP280_CalibData calib;           // Калибровочные данные
    BMP280_RawData raw_data;
    BMP280_CompensatedData data;      // Последние скомпенсированные данные
    uint8_t ctrl_meas_reg;            // Текущее значение CTRL_MEAS
    uint8_t config_reg;               // Текущее значение CONFIG
    uint8_t meas_time_ms;              // Время измерения в мс (для forced mode)
} BMP280_HandleTypeDef;

/* Function Prototypes -------------------------------------------------------*/
HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef* bmp280);
HAL_StatusTypeDef BMP280_ReadID(BMP280_HandleTypeDef* bmp280, uint8_t* id);
HAL_StatusTypeDef BMP280_SoftReset(BMP280_HandleTypeDef* bmp280);
HAL_StatusTypeDef BMP280_ReadCalibrationData(BMP280_HandleTypeDef* bmp280);
HAL_StatusTypeDef BMP280_SetConfig(BMP280_HandleTypeDef* bmp280, uint8_t config);
HAL_StatusTypeDef BMP280_SetCtrlMeas(BMP280_HandleTypeDef* bmp280, uint8_t ctrl_meas);
HAL_StatusTypeDef BMP280_ReadRawData(BMP280_HandleTypeDef* bmp280, BMP280_RawData* raw);
HAL_StatusTypeDef BMP280_CompensateData(BMP280_HandleTypeDef* bmp280, BMP280_RawData* raw, BMP280_CompensatedData* comp);
HAL_StatusTypeDef BMP280_GetMeasuredData(BMP280_HandleTypeDef* bmp280);

// Утилиты
HAL_StatusTypeDef BMP280_ForcedMeasurement(BMP280_HandleTypeDef* bmp280);
float BMP280_GetTemperature_C(BMP280_HandleTypeDef* bmp280);
float BMP280_GetPressure_hPa(BMP280_HandleTypeDef* bmp280);
float BMP280_GetAltitude_m(BMP280_HandleTypeDef* bmp280, float sea_level_pressure_hPa);

#ifdef __cplusplus
}
#endif

#endif /* INC_BMP280_H_ */
