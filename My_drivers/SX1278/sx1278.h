/*
 * sx1278.h  — драйвер SX1278 (LoRa) по SPI в стиле BMP280 (HAL + структура handle).
 *
 * Created on: May 16, 2026
 *      Author: konstantin1
 */

#ifndef SX1278_H_
#define SX1278_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/* Регистры (режим LoRa, см. datasheet SX1276/77/78/79) ---------------------*/
#define SX1278_REG_FIFO                 0x00U
#define SX1278_REG_OP_MODE              0x01U
#define SX1278_REG_FRF_MSB              0x06U
#define SX1278_REG_FRF_MID              0x07U
#define SX1278_REG_FRF_LSB              0x08U
#define SX1278_REG_PA_CONFIG            0x09U
#define SX1278_REG_PA_RAMP              0x0AU
#define SX1278_REG_OCP                  0x0BU
#define SX1278_REG_LNA                  0x0CU
#define SX1278_REG_FIFO_ADDR_PTR        0x0DU
#define SX1278_REG_FIFO_TX_BASE_ADDR    0x0EU
#define SX1278_REG_FIFO_RX_BASE_ADDR    0x0FU
#define SX1278_REG_FIFO_RX_CURRENT_ADDR 0x10U
#define SX1278_REG_IRQ_FLAGS_MASK       0x11U
#define SX1278_REG_IRQ_FLAGS            0x12U
#define SX1278_REG_RX_NB_BYTES          0x13U
#define SX1278_REG_PKT_SNR_VALUE        0x19U
#define SX1278_REG_PKT_RSSI_VALUE       0x1AU
#define SX1278_REG_MODEM_CONFIG_1       0x1DU
#define SX1278_REG_MODEM_CONFIG_2       0x1EU
#define SX1278_REG_SYMB_TIMEOUT_LSB     0x1FU
#define SX1278_REG_PREAMBLE_MSB         0x20U
#define SX1278_REG_PREAMBLE_LSB         0x21U
#define SX1278_REG_PAYLOAD_LENGTH       0x22U
#define SX1278_REG_MAX_PAYLOAD_LENGTH   0x23U
#define SX1278_REG_MODEM_CONFIG_3       0x26U
#define SX1278_REG_DETECT_OPTIMIZE      0x31U
#define SX1278_REG_DETECTION_THRESHOLD  0x37U
#define SX1278_REG_SYNC_WORD            0x39U
#define SX1278_REG_DIO_MAPPING_1        0x40U
#define SX1278_REG_DIO_MAPPING_2        0x41U
#define SX1278_REG_VERSION              0x42U
#define SX1278_REG_PA_DAC               0x4DU

/* Режим OP_MODE: бит 7 — LoRa (1), младшие 3 бита — режим RF ----------------*/
#define SX1278_MODE_LONG_RANGE_MODE     0x80U
#define SX1278_MODE_SLEEP               0x00U
#define SX1278_MODE_STDBY               0x01U
#define SX1278_MODE_TX                  0x03U
#define SX1278_MODE_RX_CONTINUOUS       0x05U
#define SX1278_MODE_RX_SINGLE           0x06U

/* IRQ FLAGS (REG_IRQ_FLAGS) ------------------------------------------------*/
#define SX1278_IRQ_RX_TIMEOUT           0x80U
#define SX1278_IRQ_RX_DONE              0x40U
#define SX1278_IRQ_PAYLOAD_CRC_ERR      0x20U
#define SX1278_IRQ_VALID_HEADER         0x10U
#define SX1278_IRQ_TX_DONE              0x08U
#define SX1278_IRQ_CAD_DONE             0x04U
#define SX1278_IRQ_FHSS_CHANGE_CH       0x02U
#define SX1278_IRQ_CAD_DETECTED         0x01U

/* Ожидаемое значение REG_VERSION для SX1276/77/78/79 -----------------------*/
#define SX1278_CHIP_VERSION             0x12U

#define SX1278_XTAL_FREQ_HZ             32000000UL

/* Полоса пропускания (modem config 1, биты 7–4) ----------------------------*/
typedef enum {
	SX1278_BW_7_8_KHZ   = 0x00U,
	SX1278_BW_10_4_KHZ  = 0x10U,
	SX1278_BW_15_6_KHZ  = 0x20U,
	SX1278_BW_20_8_KHZ  = 0x30U,
	SX1278_BW_31_25_KHZ = 0x40U,
	SX1278_BW_41_7_KHZ  = 0x50U,
	SX1278_BW_62_5_KHZ  = 0x60U,
	SX1278_BW_125_KHZ   = 0x70U,
	SX1278_BW_250_KHZ   = 0x80U,
	SX1278_BW_500_KHZ   = 0x90U,
} SX1278_Bandwidth_t;

/* Coding rate 4/x (modem config 1, биты 3–1): CR_4_5 = 1 … CR_4_8 = 4 -------*/
typedef enum {
	SX1278_CR_4_5 = 1U,
	SX1278_CR_4_6 = 2U,
	SX1278_CR_4_7 = 3U,
	SX1278_CR_4_8 = 4U,
} SX1278_CodingRate_t;

/** Параметры модуляции LoRa для Init / Configure */
typedef struct {
	uint32_t freq_hz;
	uint8_t sf;                      /* 6 … 12 */
	SX1278_Bandwidth_t bandwidth;
	SX1278_CodingRate_t coding_rate;
	uint16_t preamble_len;           /* длина преамбулы (символов) */
	uint8_t sync_word;
	bool crc_enable;
	bool implicit_header;            /* для SF6 обычно true */
	int8_t tx_power_dbm;             /* 2 … 17 для PA_BOOST (типичный модуль Ra-02) */
} SX1278_LoRaParams_t;

/** Экземпляр радио: заполнить hspi, nss_*, rst_* до вызова Init */
typedef struct {
	SPI_HandleTypeDef *hspi;
	GPIO_TypeDef *nss_port;
	uint16_t nss_pin;
	GPIO_TypeDef *rst_port;
	uint16_t rst_pin;
	SX1278_LoRaParams_t cfg;
} SX1278_HandleTypeDef;

/* Низкоуровневые операции ---------------------------------------------------*/
HAL_StatusTypeDef SX1278_ReadVersion(SX1278_HandleTypeDef *dev, uint8_t *ver);
HAL_StatusTypeDef SX1278_Reset(SX1278_HandleTypeDef *dev);

/**
 * Инициализация: аппаратный Reset, проверка версии, переход в LoRa standby,
 * базовая настройка FIFO и модема по полям cfg (после Init cfg совпадает с тем, что было передано).
 */
HAL_StatusTypeDef SX1278_Init(SX1278_HandleTypeDef *dev, const SX1278_LoRaParams_t *params);

/** Перенастройка частоты/LoRa без полного reset (из standby) */
HAL_StatusTypeDef SX1278_ConfigureLoRa(SX1278_HandleTypeDef *dev, const SX1278_LoRaParams_t *params);

HAL_StatusTypeDef SX1278_SetSleep(SX1278_HandleTypeDef *dev);
HAL_StatusTypeDef SX1278_SetStandby(SX1278_HandleTypeDef *dev);

HAL_StatusTypeDef SX1278_StartRxContinuous(SX1278_HandleTypeDef *dev);
HAL_StatusTypeDef SX1278_StartRxSingle(SX1278_HandleTypeDef *dev);

/** Блокирующая отправка пакета (ожидание IRQ TX_DONE, таймаут в мс) */
HAL_StatusTypeDef SX1278_SendPacket(SX1278_HandleTypeDef *dev, const uint8_t *data, uint8_t len,
				    uint32_t timeout_ms);

HAL_StatusTypeDef SX1278_GetIrqFlags(SX1278_HandleTypeDef *dev, uint8_t *flags);
HAL_StatusTypeDef SX1278_ClearIrqFlags(SX1278_HandleTypeDef *dev, uint8_t mask);

/**
 * Чтение принятого пакета после IRQ_RX_DONE (flags очищает вызывающий через ClearIrqFlags).
 * RSSI в dBm (оценка для HF), SNR в дБ.
 */
HAL_StatusTypeDef SX1278_ReadPacket(SX1278_HandleTypeDef *dev, uint8_t *buf, uint8_t buf_size,
				    uint8_t *out_len, int16_t *rssi_dbm, float *snr_db);

#ifdef __cplusplus
}
#endif

#endif /* SX1278_H_ */
