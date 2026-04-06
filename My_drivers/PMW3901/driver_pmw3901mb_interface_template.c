/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_pmw3901mb_interface_template.c
 * @brief     driver pmw3901mb interface template source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-01-08
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/01/08  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_pmw3901mb_interface.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi.h"
/* ==========================================================================
 * КОНФИГУРАЦИЯ
 * ========================================================================== */
#define PMW3901MB_SPI_HANDLE      hspi1              // Экземпляр SPI
#define PMW3901MB_CS_GPIO_Port    PWM3901_SS_GPIO_Port
#define PMW3901MB_CS_Pin          PWM3901_SS_Pin
#define PMW3901MB_RESET_GPIO_Port PWM3901_RST_GPIO_Port
#define PMW3901MB_RESET_Pin       PWM3901_RST_Pin

/* Таймаут SPI-транзакций в миллисекундах */
#define PMW3901MB_SPI_TIMEOUT_MS  1U

/* ==========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================================== */

static inline void pmw3901mb_cs_select(void)
{
    HAL_GPIO_WritePin(PMW3901MB_CS_GPIO_Port, PMW3901MB_CS_Pin, GPIO_PIN_RESET);
}

static inline void pmw3901mb_cs_deselect(void)
{
    HAL_GPIO_WritePin(PMW3901MB_CS_GPIO_Port, PMW3901MB_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief  interface spi bus init
 * @return status code
 *         - 0 success
 *         - 1 spi init failed
 * @note   none
 */
uint8_t pmw3901mb_interface_spi_init(void)
{
	pmw3901mb_interface_reset_gpio_write(1);
    return 0;
}

/**
 * @brief  interface spi bus deinit
 * @return status code
 *         - 0 success
 *         - 1 spi deinit failed
 * @note   none
 */
uint8_t pmw3901mb_interface_spi_deinit(void)
{
    HAL_GPIO_DeInit(PMW3901MB_CS_GPIO_Port, PMW3901MB_CS_Pin);
    HAL_GPIO_DeInit(PMW3901MB_RESET_GPIO_Port, PMW3901MB_RESET_Pin);

    return (HAL_SPI_DeInit(&PMW3901MB_SPI_HANDLE) == HAL_OK) ? 0 : 1;
}

/**
 * @brief      interface spi bus read
 * @param[in]  reg register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t pmw3901mb_interface_spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
   if (buf == NULL || len == 0) return 1;

	/* PMW3901: чтение = адрес с битом 7 = 0 */
	uint8_t cmd = reg & 0x7F;
//   uint8_t cmd = reg;
//   uint8_t cmd = reg &~ 0x80u;

	pmw3901mb_cs_select();

	/* Отправка адреса регистра */
	if (HAL_SPI_Transmit(&PMW3901MB_SPI_HANDLE, &cmd, 1, PMW3901MB_SPI_TIMEOUT_MS) != HAL_OK)
	{
		pmw3901mb_cs_deselect();
		return 1;
	}

	/* Чтение данных */
	if (HAL_SPI_Receive(&PMW3901MB_SPI_HANDLE, buf, len, PMW3901MB_SPI_TIMEOUT_MS) != HAL_OK)
	{
		pmw3901mb_cs_deselect();
		return 1;
	}

	pmw3901mb_cs_deselect();
	return 0;
}

/**
 * @brief     interface spi bus write
 * @param[in] reg register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t pmw3901mb_interface_spi_write(uint8_t reg, uint8_t *buf, uint16_t len)
{
	if (buf == NULL || len == 0) return 1;

	/* PMW3901: запись = адрес с битом 7 = 1 */
	uint8_t cmd = reg | 0x80;

	/* Буфер: [команда][данные] */
	uint8_t tx_buf[32 + 1];  /* Макс. длина + 1 байт команды */
	if (len > sizeof(tx_buf) - 1) return 1;

	tx_buf[0] = cmd;
	memcpy(&tx_buf[1], buf, len);

	pmw3901mb_cs_select();

	if (HAL_SPI_Transmit(&PMW3901MB_SPI_HANDLE, tx_buf, len + 1, PMW3901MB_SPI_TIMEOUT_MS) != HAL_OK)
	{
		pmw3901mb_cs_deselect();
		return 1;
	}

	pmw3901mb_cs_deselect();
	return 0;
}

/**
 * @brief  interface reset gpio init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
uint8_t pmw3901mb_interface_reset_gpio_init(void)
{
    HAL_GPIO_WritePin(PMW3901MB_RESET_GPIO_Port, PMW3901MB_RESET_Pin, GPIO_PIN_SET);

    return 0;
}

/**
 * @brief  interface reset gpio deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t pmw3901mb_interface_reset_gpio_deinit(void)
{
    HAL_GPIO_DeInit(PMW3901MB_RESET_GPIO_Port, PMW3901MB_RESET_Pin);
    return 0;
}

/**
 * @brief     interface reset gpio write
 * @param[in] data written data
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t pmw3901mb_interface_reset_gpio_write(uint8_t data)
{
    HAL_GPIO_WritePin(PMW3901MB_RESET_GPIO_Port,
                      PMW3901MB_RESET_Pin,
                      (data != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void pmw3901mb_interface_delay_ms(uint32_t ms)
{
#ifdef USE_FreeRTOS
	vTaskDelay(pdMS_TO_TICKS(ms));
#else
	HAL_Delay(ms);
#endif
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void pmw3901mb_interface_debug_print(const char *const fmt, ...)
{
    
}
