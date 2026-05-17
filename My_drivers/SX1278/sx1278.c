/*
 * sx1278.c
 *
 * Created on: May 16, 2026
 *      Author: konstantin1
 */

#include "sx1278.h"

static uint32_t sx1278_bw_hz(SX1278_Bandwidth_t bw)
{
	switch (bw) {
	case SX1278_BW_7_8_KHZ:
		return 7800UL;
	case SX1278_BW_10_4_KHZ:
		return 10400UL;
	case SX1278_BW_15_6_KHZ:
		return 15600UL;
	case SX1278_BW_20_8_KHZ:
		return 20800UL;
	case SX1278_BW_31_25_KHZ:
		return 31250UL;
	case SX1278_BW_41_7_KHZ:
		return 41700UL;
	case SX1278_BW_62_5_KHZ:
		return 62500UL;
	case SX1278_BW_125_KHZ:
		return 125000UL;
	case SX1278_BW_250_KHZ:
		return 250000UL;
	case SX1278_BW_500_KHZ:
		return 500000UL;
	default:
		return 125000UL;
	}
}

static HAL_StatusTypeDef SX1278_SPI_Transceive(SX1278_HandleTypeDef *dev, uint8_t reg, uint8_t *data,
					       uint16_t size, uint8_t rw)
{
	HAL_StatusTypeDef status;
	uint8_t command_byte = (uint8_t)((reg & 0x7FU) | (rw ? 0x80U : 0x00U));

	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_RESET);

	if (rw) {
		status = HAL_SPI_Transmit(dev->hspi, &command_byte, 1, HAL_MAX_DELAY);
		if (status == HAL_OK) {
			status = HAL_SPI_Receive(dev->hspi, data, size, HAL_MAX_DELAY);
		}
	} else {
		status = HAL_SPI_Transmit(dev->hspi, &command_byte, 1, HAL_MAX_DELAY);
		if (status == HAL_OK && size > 0U) {
			status = HAL_SPI_Transmit(dev->hspi, data, size, HAL_MAX_DELAY);
		}
	}

	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_SET);

	return status;
}

static HAL_StatusTypeDef SX1278_ReadReg(SX1278_HandleTypeDef *dev, uint8_t reg, uint8_t *data)
{
	return SX1278_SPI_Transceive(dev, reg, data, 1U, 1U);
}

static HAL_StatusTypeDef SX1278_WriteReg(SX1278_HandleTypeDef *dev, uint8_t reg, uint8_t data)
{
	return SX1278_SPI_Transceive(dev, reg, &data, 1U, 0U);
}

static HAL_StatusTypeDef SX1278_ReadRegs(SX1278_HandleTypeDef *dev, uint8_t reg, uint8_t *data, uint16_t size)
{
	return SX1278_SPI_Transceive(dev, reg, data, size, 1U);
}

static HAL_StatusTypeDef SX1278_WriteRegs(SX1278_HandleTypeDef *dev, uint8_t reg, const uint8_t *data,
					  uint16_t size)
{
	HAL_StatusTypeDef status;
	uint8_t command_byte = (uint8_t)((reg & 0x7FU) | 0x00U);

	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_RESET);
	status = HAL_SPI_Transmit(dev->hspi, &command_byte, 1, HAL_MAX_DELAY);
	if (status == HAL_OK && size > 0U) {
		status = HAL_SPI_Transmit(dev->hspi, (uint8_t *)data, size, HAL_MAX_DELAY);
	}
	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_SET);

	return status;
}

HAL_StatusTypeDef SX1278_Reset(SX1278_HandleTypeDef *dev)
{
	HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_SET);
	HAL_Delay(10);
	return HAL_OK;
}

HAL_StatusTypeDef SX1278_ReadVersion(SX1278_HandleTypeDef *dev, uint8_t *ver)
{
	if (ver == NULL) {
		return HAL_ERROR;
	}
	return SX1278_ReadReg(dev, SX1278_REG_VERSION, ver);
}

static HAL_StatusTypeDef sx1278_set_op_mode(SX1278_HandleTypeDef *dev, uint8_t mode)
{
	return SX1278_WriteReg(dev, SX1278_REG_OP_MODE, mode);
}

HAL_StatusTypeDef SX1278_SetSleep(SX1278_HandleTypeDef *dev)
{
	return sx1278_set_op_mode(dev, (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_SLEEP));
}

HAL_StatusTypeDef SX1278_SetStandby(SX1278_HandleTypeDef *dev)
{
	return sx1278_set_op_mode(dev, (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_STDBY));
}

static HAL_StatusTypeDef sx1278_set_frequency(SX1278_HandleTypeDef *dev, uint32_t freq_hz)
{
	uint64_t frf = ((uint64_t)freq_hz << 19) / (uint64_t)SX1278_XTAL_FREQ_HZ;
	uint8_t msb = (uint8_t)((frf >> 16) & 0xFFU);
	uint8_t mid = (uint8_t)((frf >> 8) & 0xFFU);
	uint8_t lsb = (uint8_t)(frf & 0xFFU);
	HAL_StatusTypeDef st;

	st = SX1278_WriteReg(dev, SX1278_REG_FRF_MSB, msb);
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_FRF_MID, mid);
	if (st != HAL_OK) {
		return st;
	}
	return SX1278_WriteReg(dev, SX1278_REG_FRF_LSB, lsb);
}

/** PA_BOOST, упрощённая установка мощности 2…17 dBm (как в типичных примерах RadioHead) */
static HAL_StatusTypeDef sx1278_set_tx_power(SX1278_HandleTypeDef *dev, int8_t power_dbm)
{
	int8_t p = power_dbm;
	HAL_StatusTypeDef st;

	if (p < 2) {
		p = 2;
	}
	if (p > 17) {
		p = 17;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_PA_DAC, 0x84U);
	if (st != HAL_OK) {
		return st;
	}

	uint8_t pa_config = (uint8_t)(0x80U | (uint8_t)(p - 2));
	st = SX1278_WriteReg(dev, SX1278_REG_PA_CONFIG, pa_config);
	if (st != HAL_OK) {
		return st;
	}

	/* запас по току для усилителя */
	return SX1278_WriteReg(dev, SX1278_REG_OCP, 0x3FU);
}

HAL_StatusTypeDef SX1278_ConfigureLoRa(SX1278_HandleTypeDef *dev, const SX1278_LoRaParams_t *params)
{
	if (dev == NULL || params == NULL || dev->hspi == NULL) {
		return HAL_ERROR;
	}

	uint8_t sf = params->sf;
	if (sf < 6U) {
		sf = 6U;
	}
	if (sf > 12U) {
		sf = 12U;
	}

	bool implicit = params->implicit_header;
	if (sf == 6U) {
		implicit = true;
	}

	HAL_StatusTypeDef st = SX1278_SetStandby(dev);
	if (st != HAL_OK) {
		return st;
	}

	st = sx1278_set_frequency(dev, params->freq_hz);
	if (st != HAL_OK) {
		return st;
	}

	st = sx1278_set_tx_power(dev, params->tx_power_dbm);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_LNA, 0x23U);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_FIFO_TX_BASE_ADDR, 0x00U);
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_FIFO_RX_BASE_ADDR, 0x00U);
	if (st != HAL_OK) {
		return st;
	}

	uint8_t mc1 = (uint8_t)((uint8_t)params->bandwidth | (uint8_t)((uint8_t)params->coding_rate << 1));
	if (implicit) {
		mc1 |= 0x01U;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_MODEM_CONFIG_1, mc1);
	if (st != HAL_OK) {
		return st;
	}

	/* Таймаут символов для заголовка (10 бит): старшие 2 бита в MC2, младшие 8 — в SYMB_TIMEOUT_LSB */
	const uint16_t symb_timeout = 0x3FFU;
	uint8_t mc2 = (uint8_t)((sf << 4) | ((symb_timeout >> 8) & 0x03U));
	if (params->crc_enable) {
		mc2 |= 0x04U;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_MODEM_CONFIG_2, mc2);
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_SYMB_TIMEOUT_LSB, (uint8_t)(symb_timeout & 0xFFU));
	if (st != HAL_OK) {
		return st;
	}

	uint32_t bw_hz = sx1278_bw_hz(params->bandwidth);
	uint32_t ts_usec_x256 = (256U * (1UL << sf) * 1000000UL) / bw_hz;
	bool need_ldr = ts_usec_x256 > (16U * 256U * 1000UL);

	uint8_t mc3 = need_ldr ? 0x08U : 0x00U;
	st = SX1278_WriteReg(dev, SX1278_REG_MODEM_CONFIG_3, mc3);
	if (st != HAL_OK) {
		return st;
	}

	if (sf == 6U) {
		st = SX1278_WriteReg(dev, SX1278_REG_DETECT_OPTIMIZE, 0xC5U);
		if (st != HAL_OK) {
			return st;
		}
		st = SX1278_WriteReg(dev, SX1278_REG_DETECTION_THRESHOLD, 0x0CU);
	} else if (sf >= 11U) {
		st = SX1278_WriteReg(dev, SX1278_REG_DETECT_OPTIMIZE, 0xC3U);
		if (st != HAL_OK) {
			return st;
		}
		st = SX1278_WriteReg(dev, SX1278_REG_DETECTION_THRESHOLD, 0x0AU);
	} else {
		st = SX1278_WriteReg(dev, SX1278_REG_DETECT_OPTIMIZE, 0xC5U);
		if (st != HAL_OK) {
			return st;
		}
		st = SX1278_WriteReg(dev, SX1278_REG_DETECTION_THRESHOLD, 0x15U);
	}
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_SYNC_WORD, params->sync_word);
	if (st != HAL_OK) {
		return st;
	}

	uint16_t preamble = params->preamble_len;
	if (preamble < 6U) {
		preamble = 6U;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_PREAMBLE_MSB, (uint8_t)((preamble >> 8) & 0xFFU));
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_WriteReg(dev, SX1278_REG_PREAMBLE_LSB, (uint8_t)(preamble & 0xFFU));
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_MAX_PAYLOAD_LENGTH, 0xFFU);
	if (st != HAL_OK) {
		return st;
	}

	dev->cfg = *params;
	dev->cfg.sf = sf;
	dev->cfg.implicit_header = implicit;

	return HAL_OK;
}

HAL_StatusTypeDef SX1278_Init(SX1278_HandleTypeDef *dev, const SX1278_LoRaParams_t *params)
{
	if (dev == NULL || params == NULL || dev->hspi == NULL || dev->nss_port == NULL || dev->rst_port == NULL) {
		return HAL_ERROR;
	}

	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_SET);
	(void)SX1278_Reset(dev);

	uint8_t ver = 0;
	HAL_StatusTypeDef st = SX1278_ReadVersion(dev, &ver);
	if (st != HAL_OK || ver != SX1278_CHIP_VERSION) {
		return HAL_ERROR;
	}

	/* После reset чип в FSK/OOK: sleep → LoRa sleep → LoRa standby (см. datasheet) */
	st = SX1278_WriteReg(dev, SX1278_REG_OP_MODE, SX1278_MODE_SLEEP);
	if (st != HAL_OK) {
		return st;
	}
	HAL_Delay(1);

	st = SX1278_WriteReg(dev, SX1278_REG_OP_MODE,
			     (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_SLEEP));
	if (st != HAL_OK) {
		return st;
	}
	HAL_Delay(1);

	st = SX1278_WriteReg(dev, SX1278_REG_OP_MODE,
			     (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_STDBY));
	if (st != HAL_OK) {
		return st;
	}
	HAL_Delay(1);

	st = SX1278_ClearIrqFlags(dev, 0xFFU);
	if (st != HAL_OK) {
		return st;
	}

	return SX1278_ConfigureLoRa(dev, params);
}

HAL_StatusTypeDef SX1278_GetIrqFlags(SX1278_HandleTypeDef *dev, uint8_t *flags)
{
	if (flags == NULL) {
		return HAL_ERROR;
	}
	return SX1278_ReadReg(dev, SX1278_REG_IRQ_FLAGS, flags);
}

HAL_StatusTypeDef SX1278_ClearIrqFlags(SX1278_HandleTypeDef *dev, uint8_t mask)
{
	return SX1278_WriteReg(dev, SX1278_REG_IRQ_FLAGS, mask);
}

HAL_StatusTypeDef SX1278_StartRxContinuous(SX1278_HandleTypeDef *dev)
{
	HAL_StatusTypeDef st = SX1278_SetStandby(dev);
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_ClearIrqFlags(dev, 0xFFU);
	if (st != HAL_OK) {
		return st;
	}
	return sx1278_set_op_mode(dev, (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_RX_CONTINUOUS));
}

HAL_StatusTypeDef SX1278_StartRxSingle(SX1278_HandleTypeDef *dev)
{
	HAL_StatusTypeDef st = SX1278_SetStandby(dev);
	if (st != HAL_OK) {
		return st;
	}
	st = SX1278_ClearIrqFlags(dev, 0xFFU);
	if (st != HAL_OK) {
		return st;
	}
	return sx1278_set_op_mode(dev, (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_RX_SINGLE));
}

HAL_StatusTypeDef SX1278_SendPacket(SX1278_HandleTypeDef *dev, const uint8_t *data, uint8_t len,
				    uint32_t timeout_ms)
{
	if (dev == NULL || data == NULL || len == 0U) {
		return HAL_ERROR;
	}

	HAL_StatusTypeDef st = SX1278_SetStandby(dev);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_ClearIrqFlags(dev, 0xFFU);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_FIFO_ADDR_PTR, 0x00U);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteRegs(dev, SX1278_REG_FIFO, data, len);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_PAYLOAD_LENGTH, len);
	if (st != HAL_OK) {
		return st;
	}

	st = sx1278_set_op_mode(dev, (uint8_t)(SX1278_MODE_LONG_RANGE_MODE | SX1278_MODE_TX));
	if (st != HAL_OK) {
		return st;
	}

	const uint32_t t0 = HAL_GetTick();
	while ((HAL_GetTick() - t0) < timeout_ms) {
		uint8_t irq = 0;
		st = SX1278_GetIrqFlags(dev, &irq);
		if (st != HAL_OK) {
			return st;
		}
		if ((irq & SX1278_IRQ_TX_DONE) != 0U) {
			(void)SX1278_ClearIrqFlags(dev, SX1278_IRQ_TX_DONE);
			return SX1278_SetStandby(dev);
		}
	}

	(void)SX1278_SetStandby(dev);
	return HAL_TIMEOUT;
}

HAL_StatusTypeDef SX1278_ReadPacket(SX1278_HandleTypeDef *dev, uint8_t *buf, uint8_t buf_size, uint8_t *out_len,
				    int16_t *rssi_dbm, float *snr_db)
{
	if (dev == NULL || buf == NULL || out_len == NULL) {
		return HAL_ERROR;
	}

	uint8_t nb = 0;
	HAL_StatusTypeDef st = SX1278_ReadReg(dev, SX1278_REG_RX_NB_BYTES, &nb);
	if (st != HAL_OK) {
		return st;
	}

	if (nb > buf_size) {
		nb = buf_size;
	}

	uint8_t fifo_addr = 0;
	st = SX1278_ReadReg(dev, SX1278_REG_FIFO_RX_CURRENT_ADDR, &fifo_addr);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_WriteReg(dev, SX1278_REG_FIFO_ADDR_PTR, fifo_addr);
	if (st != HAL_OK) {
		return st;
	}

	st = SX1278_ReadRegs(dev, SX1278_REG_FIFO, buf, nb);
	if (st != HAL_OK) {
		return st;
	}

	if (rssi_dbm != NULL) {
		uint8_t pkt_rssi = 0;
		int8_t pkt_snr_raw = 0;
		(void)SX1278_ReadReg(dev, SX1278_REG_PKT_RSSI_VALUE, &pkt_rssi);
		(void)SX1278_ReadReg(dev, SX1278_REG_PKT_SNR_VALUE, (uint8_t *)&pkt_snr_raw);

		float snr = (float)pkt_snr_raw / 4.0f;
		int16_t rssi = (int16_t)(-157 + (int16_t)pkt_rssi);
		if (snr < 0.0f) {
			rssi = (int16_t)((float)rssi + snr * 0.25f);
		}
		*rssi_dbm = rssi;
		if (snr_db != NULL) {
			*snr_db = snr;
		}
	} else if (snr_db != NULL) {
		int8_t pkt_snr_raw = 0;
		(void)SX1278_ReadReg(dev, SX1278_REG_PKT_SNR_VALUE, (uint8_t *)&pkt_snr_raw);
		*snr_db = (float)pkt_snr_raw / 4.0f;
	}

	*out_len = nb;
	return HAL_OK;
}
