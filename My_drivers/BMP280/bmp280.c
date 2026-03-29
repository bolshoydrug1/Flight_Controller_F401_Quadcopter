/*
 * bmp280.c
 *
 *  Created on: Mar 8, 2026
 *      Author: konstantin1
 */


#include "bmp280.h"
#include <math.h> // Для вычисления высоты (можно заменить своей функцией)

/* Private Functions ---------------------------------------------------------*/


/**
  * @brief  Вычисление высоты по формуле Бабинэ (более точная)
  * @param  p1: Давление в нижней точке (мм.рт.ст)
  * @param  p2: Давление в верхней точке (мм.рт.ст)
  * @param  t:  Температура в градусах Цельсия
  * @retval Высота в метрах
  */
static float BMP280_CalculateHeightBabinet(float p1, float p2, float t) {
    // Формула Бабинэ: h = 8000 * (2*(p1-p2)/(p1+p2)) * (1 + t/273)
    // где t - средняя температура в °C
    float pressure_diff = p1 - p2;
    float pressure_sum = p1 + p2;

    if (pressure_sum == 0.0f) return 0.0f;

    // Основной расчет
    float height = 8000.0f * (2.0f * pressure_diff / pressure_sum) * (1.0f + t / 273.0f);

    return height;
}

/**
  * @brief  Отправляет команду и читает/пишет данные по SPI (синхронно).
  * @param  bmp280: Указатель на структуру BMP280.
  * @param  reg: Адрес регистра (полный 8-битный адрес из даташита).
  * @param  data: Указатель на буфер данных.
  * @param  size: Размер данных.
  * @param  rw: 0 для записи, 1 для чтения.
  * @retval HAL статус.
  */
static HAL_StatusTypeDef BMP280_SPI_Transceive(BMP280_HandleTypeDef* bmp280, uint8_t reg, uint8_t* data, uint16_t size, uint8_t rw) {
    HAL_StatusTypeDef status;
    uint8_t command_byte;

    // Формируем команду согласно даташиту (стр. 31):
    // Для SPI используется 7-битный адрес, а 8-й бит = RW (1 - чтение, 0 - запись)
    command_byte = (reg & 0x7F) | (rw << 7);

    // CS в 0 (активен)
    HAL_GPIO_WritePin(bmp280->cs_port, bmp280->cs_pin, GPIO_PIN_RESET);

    if (rw) { // Чтение
        status = HAL_SPI_Transmit(bmp280->hspi, &command_byte, 1, HAL_MAX_DELAY);
        if (status == HAL_OK) {
            status = HAL_SPI_Receive(bmp280->hspi, data, size, HAL_MAX_DELAY);
        }
    } else { // Запись
        status = HAL_SPI_Transmit(bmp280->hspi, &command_byte, 1, HAL_MAX_DELAY);
        if (status == HAL_OK) {
            status = HAL_SPI_Transmit(bmp280->hspi, data, size, HAL_MAX_DELAY);
        }
    }

    // CS в 1 (неактивен)
    HAL_GPIO_WritePin(bmp280->cs_port, bmp280->cs_pin, GPIO_PIN_SET);

    return status;
}

/**
  * @brief  Чтение из одного регистра.
  */
static HAL_StatusTypeDef BMP280_ReadReg(BMP280_HandleTypeDef* bmp280, uint8_t reg, uint8_t* data) {
    return BMP280_SPI_Transceive(bmp280, reg, data, 1, 1);
}

/**
  * @brief  Запись в один регистр.
  */
static HAL_StatusTypeDef BMP280_WriteReg(BMP280_HandleTypeDef* bmp280, uint8_t reg, uint8_t data) {
    return BMP280_SPI_Transceive(bmp280, reg, &data, 1, 0);
}

/**
  * @brief  Чтение нескольких регистров подряд (burst read).
  */
static HAL_StatusTypeDef BMP280_ReadRegs(BMP280_HandleTypeDef* bmp280, uint8_t reg, uint8_t* data, uint16_t size) {
    return BMP280_SPI_Transceive(bmp280, reg, data, size, 1);
}


/**
  * @brief  Чтение калибровочных данных.
  */
static HAL_StatusTypeDef BMP280_ReadCalibrationData(BMP280_HandleTypeDef* bmp280) {
    uint8_t calib[24]; // 0x88 - 0xA1 включительно (24 байта)
    HAL_StatusTypeDef status;

    status = BMP280_ReadRegs(bmp280, BMP280_REG_CALIB_START, calib, 24);
    if (status != HAL_OK) {
        return status;
    }

    // Распаковка согласно таблице 17 даташита (стр. 21)
    bmp280->calib.dig_T1 = (uint16_t)((calib[1] << 8) | calib[0]);
    bmp280->calib.dig_T2 = (int16_t)((calib[3] << 8) | calib[2]);
    bmp280->calib.dig_T3 = (int16_t)((calib[5] << 8) | calib[4]);
    bmp280->calib.dig_P1 = (uint16_t)((calib[7] << 8) | calib[6]);
    bmp280->calib.dig_P2 = (int16_t)((calib[9] << 8) | calib[8]);
    bmp280->calib.dig_P3 = (int16_t)((calib[11] << 8) | calib[10]);
    bmp280->calib.dig_P4 = (int16_t)((calib[13] << 8) | calib[12]);
    bmp280->calib.dig_P5 = (int16_t)((calib[15] << 8) | calib[14]);
    bmp280->calib.dig_P6 = (int16_t)((calib[17] << 8) | calib[16]);
    bmp280->calib.dig_P7 = (int16_t)((calib[19] << 8) | calib[18]);
    bmp280->calib.dig_P8 = (int16_t)((calib[21] << 8) | calib[20]);
    bmp280->calib.dig_P9 = (int16_t)((calib[23] << 8) | calib[22]);

    return HAL_OK;
}


/**
  * @brief  Чтение сырых данных температуры и давления.
  */
static HAL_StatusTypeDef BMP280_ReadRawData(BMP280_HandleTypeDef* bmp280, BMP280_RawData* raw) {
    uint8_t data[6];
    HAL_StatusTypeDef status;

    // Читаем 6 байт, начиная с PRESS_MSB (0xF7)
    status = BMP280_ReadRegs(bmp280, BMP280_REG_PRESS_MSB, data, 6);
    if (status != HAL_OK) {
        return status;
    }

    // Сборка 20-битных значений (см. разделы 4.3.6 и 4.3.7)
    raw->adc_press = (int32_t)(((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((uint32_t)data[2] >> 4));
    raw->adc_temp = (int32_t)(((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((uint32_t)data[5] >> 4));

    return HAL_OK;
}
/**
  * @brief  Компенсация сырых данных (32-битная целочисленная) с заполнением структуры float
  * @param  bmp280: Указатель на структуру BMP280 (содержит калибровочные данные и first_press)
  * @param  raw: Указатель на структуру с сырыми данными
  * @param  comp: Указатель на структуру для скомпенсированных данных (с полями float)
  * @param  sea_level_pressure: Давление на уровне моря в мм.рт.ст (макрос для юга России)
  * @retval HAL статус.
  */
static HAL_StatusTypeDef BMP280_CompensateData(BMP280_HandleTypeDef* bmp280, BMP280_RawData* raw) {
    int32_t var1, var2, t_fine;
    uint32_t p;
    int32_t adc_T = raw->adc_temp;
    int32_t adc_P = raw->adc_press;

    // Константа для перевода Паскалей в мм.рт.ст
    const float PA_TO_MMHG = 0.00750062f;

    // Компенсация температуры (из раздела 8.2 даташита)
    var1 = ((((adc_T >> 3) - ((int32_t)bmp280->calib.dig_T1 << 1))) * ((int32_t)bmp280->calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp280->calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)bmp280->calib.dig_T1))) >> 12) * ((int32_t)bmp280->calib.dig_T3)) >> 14;
    t_fine = var1 + var2;

    // Температура в 0.01°C, переводим в float °C
    int32_t temp_int = (t_fine * 5 + 128) >> 8;
    bmp280->data.temperature = (float)temp_int / 100.0f;

    // Компенсация давления (из раздела 8.2 даташита, 32-bit)
    var1 = ((int32_t)t_fine >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)bmp280->calib.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)bmp280->calib.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)bmp280->calib.dig_P4) << 16);
    var1 = (((bmp280->calib.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)bmp280->calib.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)bmp280->calib.dig_P1)) >> 15);

    if (var1 == 0) {
        return HAL_ERROR; // Деление на ноль
    }

    p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * (uint32_t)3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var1);
    } else {
        p = (p / (uint32_t)var1) * 2;
    }

    var1 = (((int32_t)bmp280->calib.dig_P9) * ((int32_t)((p >> 3) * (p >> 3)) >> 13)) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)bmp280->calib.dig_P8)) >> 13;

    // Давление в Паскалях (uint32_t), переводим в мм.рт.ст (float)
    uint32_t pressure_pa = (uint32_t)((int32_t)p + ((var1 + var2 + bmp280->calib.dig_P7) >> 4));
    bmp280->data.pressure = (float)pressure_pa * PA_TO_MMHG;

    // Вычисляем высоту над уровнем моря (стандартная барометрическая формула)
    // sea_level_pressure уже в мм.рт.ст
    bmp280->data.Altitude = 44330.0f * (1.0f - powf(bmp280->data.pressure / SEA_LEVEL_PRESSURE, 0.1903f));

    float first_pressure_mmhg = (float)bmp280->first_pressure_pa * PA_TO_MMHG;

    bmp280->data.relative_height = BMP280_CalculateHeightBabinet(bmp280->data.pressure, first_pressure_mmhg, bmp280->data.temperature);

    return HAL_OK;
}

/* Public Functions ----------------------------------------------------------*/

/**
  * @brief  Чтение ID чипа.
  */
HAL_StatusTypeDef BMP280_ReadID(BMP280_HandleTypeDef* bmp280, uint8_t* id) {
    return BMP280_ReadReg(bmp280, BMP280_REG_ID, id);
}

/**
  * @brief  Мягкий сброс.
  */
HAL_StatusTypeDef BMP280_SoftReset(BMP280_HandleTypeDef* bmp280) {
    return BMP280_WriteReg(bmp280, BMP280_REG_RESET, BMP280_RESET_VALUE);
}

/**
  * @brief  Установка регистра CONFIG.
  * @note   Для изменения конфигурации рекомендуется перевести датчик в SLEEP mode.
  */
HAL_StatusTypeDef BMP280_SetConfig(BMP280_HandleTypeDef* bmp280, uint8_t config) {
    //uint8_t current_mode = bmp280->ctrl_meas_reg & 0x03;
    HAL_StatusTypeDef status;

    // Переход в sleep mode
    status = BMP280_WriteReg(bmp280, BMP280_REG_CTRL_MEAS, bmp280->ctrl_meas_reg & ~0x03);
    if (status != HAL_OK) return status;
    HAL_Delay(1);

    // Запись конфига
    status = BMP280_WriteReg(bmp280, BMP280_REG_CONFIG, config);
    if (status != HAL_OK) return status;
    bmp280->config_reg = config;

    // Возврат в исходный режим
    status = BMP280_WriteReg(bmp280, BMP280_REG_CTRL_MEAS, bmp280->ctrl_meas_reg);
    HAL_Delay(1);
    return status;
}

/**
  * @brief  Установка регистра CTRL_MEAS.
  */
HAL_StatusTypeDef BMP280_SetCtrlMeas(BMP280_HandleTypeDef* bmp280, uint8_t ctrl_meas) {
    bmp280->ctrl_meas_reg = ctrl_meas;

    // Обновляем время измерения в зависимости от настроек oversampling
    uint8_t osrs_p = (ctrl_meas >> 2) & 0x07;
    uint8_t osrs_t = (ctrl_meas >> 5) & 0x07;
    uint8_t max_osrs = (osrs_p > osrs_t) ? osrs_p : osrs_t;

    switch(max_osrs) {
        case 0x01: // ×1
        case 0x02: // ×2
            bmp280->meas_time_ms = BMP280_MEAS_TIME_LOW;
            break;
        case 0x03: // ×4
            bmp280->meas_time_ms = BMP280_MEAS_TIME_STANDARD;
            break;
        case 0x04: // ×8
            bmp280->meas_time_ms = BMP280_MEAS_TIME_HIGH;
            break;
        default: // ×16
            bmp280->meas_time_ms = BMP280_MEAS_TIME_ULTRA_HIGH;
            break;
    }

    return BMP280_WriteReg(bmp280, BMP280_REG_CTRL_MEAS, ctrl_meas);
}

/**
  * @brief  Инициализация BMP280.
  * @param  bmp280: Указатель на структуру BMP280 (должна быть заполнена hspi, cs_port, cs_pin).
  * @retval HAL статус.
  */
HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef* bmp280) {
    uint8_t id = 0;
    HAL_StatusTypeDef status;

    // 1. КРИТИЧЕСКИ ВАЖНО: Принудительно переключаем в SPI режим
    HAL_GPIO_WritePin(bmp280->cs_port, bmp280->cs_pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(bmp280->cs_port, bmp280->cs_pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(bmp280->cs_port, bmp280->cs_pin, GPIO_PIN_SET);
    HAL_Delay(1);

    // 2. Проверка ID
    status = BMP280_ReadReg(bmp280, BMP280_REG_ID, &id);
    if (status != HAL_OK || id != BMP280_CHIP_ID) {
        return HAL_ERROR;
    }

    // 3. Мягкий сброс
    status = BMP280_WriteReg(bmp280, BMP280_REG_RESET, BMP280_RESET_VALUE);
    HAL_Delay(10);

    // 4. Чтение калибровочных данных
    status = BMP280_ReadCalibrationData(bmp280);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // 5. Настройка конфигурации
    status = BMP280_SetConfig(bmp280, BMP280_T_SB_1000_MS | BMP280_FILTER_OFF | BMP280_SPI3W_DISABLE);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // 6. Настройка измерений
    status = BMP280_SetCtrlMeas(bmp280, BMP280_OSRS_T_X4 | BMP280_OSRS_P_X4 | BMP280_MODE_NORMAL);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    // Ждем первое измерение
    HAL_Delay(bmp280->meas_time_ms + 15);

    // Читаем сырые данные (и температуру, и давление)
    BMP280_RawData raw_first;
    status = BMP280_ReadRawData(bmp280, &raw_first);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    int32_t var1, var2, t_fine;

    // ТЕПЕРЬ ИСПОЛЬЗУЕМ raw_first.adc_temp для adc_T!
    int32_t adc_T = raw_first.adc_temp;
    int32_t first_adc = raw_first.adc_press;

    // Компенсация температуры
    var1 = ((((adc_T >> 3) - ((int32_t)bmp280->calib.dig_T1 << 1))) * ((int32_t)bmp280->calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp280->calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)bmp280->calib.dig_T1))) >> 12) * ((int32_t)bmp280->calib.dig_T3)) >> 14;
    t_fine = var1 + var2;

    // Компенсация первого давления
    var1 = ((int32_t)t_fine >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)bmp280->calib.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)bmp280->calib.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)bmp280->calib.dig_P4) << 16);
    var1 = (((bmp280->calib.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)bmp280->calib.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)bmp280->calib.dig_P1)) >> 15);

    uint32_t first_pressure_pa = 0;
    if (var1 != 0) {
        uint32_t p_first = (((uint32_t)(((int32_t)1048576) - first_adc) - (var2 >> 12))) * (uint32_t)3125;
        if (p_first < 0x80000000) {
            p_first = (p_first << 1) / ((uint32_t)var1);
        } else {
            p_first = (p_first / (uint32_t)var1) * 2;
        }

        var1 = (((int32_t)bmp280->calib.dig_P9) * ((int32_t)((p_first >> 3) * (p_first >> 3)) >> 13)) >> 12;
        var2 = (((int32_t)(p_first >> 2)) * ((int32_t)bmp280->calib.dig_P8)) >> 13;
        first_pressure_pa = (uint32_t)((int32_t)p_first + ((var1 + var2 + bmp280->calib.dig_P7) >> 4));
    }

    bmp280->first_pressure_pa = first_pressure_pa;

    return HAL_OK;
}

/**
  * @brief  Высокоуровневая функция: читает сырые данные, компенсирует и сохраняет в структуре.
  * @param  bmp280: Указатель на структуру BMP280.
  * @retval HAL статус.
  */
HAL_StatusTypeDef BMP280_GetMeasuredData(BMP280_HandleTypeDef* bmp280) {
    BMP280_RawData raw;
    HAL_StatusTypeDef status;

    status = BMP280_ReadRawData(bmp280, &raw);
    if (status != HAL_OK) {
        return status;
    }

    status = BMP280_CompensateData(bmp280, &raw);
    return status;
}

/**
  * @brief  Выполнить одиночное измерение в FORCED mode.
  * @param  bmp280: Указатель на структуру BMP280.
  * @retval HAL статус.
  */
HAL_StatusTypeDef BMP280_ForcedMeasurement(BMP280_HandleTypeDef* bmp280) {
    HAL_StatusTypeDef status;
    uint8_t forced_cmd = (bmp280->ctrl_meas_reg & 0xFC) | BMP280_MODE_FORCED;

    status = BMP280_WriteReg(bmp280, BMP280_REG_CTRL_MEAS, forced_cmd);
    if (status != HAL_OK) return status;

    // Ждем завершения измерения
    HAL_Delay(bmp280->meas_time_ms);

    // Читаем результат
    return BMP280_GetMeasuredData(bmp280);
}


