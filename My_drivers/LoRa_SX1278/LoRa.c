/*
 * LoRa.c
 *
 *  Created on: Jun 24, 2026
 *      Author: konstantin1
 */


#include "LoRa.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#define DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))

/* Таймаут одной SPI-транзакции регистрового доступа (микросекунды-единицы мс
 * на SPI2 @ APB1/16). Отдельно от TRANSMIT_TIMEOUT/RECEIVE_TIMEOUT, которые
 * теперь означают время ожидания TxDone/RxDone в эфире. */
#define LORA_SPI_IO_TIMEOUT_MS	10

/* Сколько раз повторять одну SPI-транзакцию (LoRa_readReg/writeReg) при
 * HAL-ошибке/таймауте, прежде чем сдаться. */
#define LORA_SPI_MAX_ATTEMPTS	3

/* Сколько раз повторять пару запись+обратное чтение в LoRa_writeVerified. */
#define LORA_REG_WRITE_MAX_ATTEMPTS	3

/* ----------------------------------------------------------------------------- *\
		name        : newLoRa

		description : it's a constructor for LoRa structure that assign default values
									and pass created object (LoRa struct instanse)

		arguments   : Nothing

		returns     : A LoRa object whith these default values:
											----------------------------------------
										  |   carrier frequency = 433 MHz        |
										  |    spreading factor = 7				       |
											|           bandwidth = 125 KHz        |
											| 		    coding rate = 4/5            |
											----------------------------------------
\* ----------------------------------------------------------------------------- */
LoRa newLoRa(){
	LoRa new_LoRa;

	new_LoRa.frequency             = 433       ;
	new_LoRa.spredingFactor        = SF_7      ;
	new_LoRa.bandWidth			   = BW_125KHz ;
	new_LoRa.crcRate               = CR_4_5    ;
	new_LoRa.power				   = POWER_20db;
	new_LoRa.overCurrentProtection = 100       ;
	new_LoRa.preamble			   = 8         ;
	new_LoRa.ownerTask             = NULL      ;

	return new_LoRa;
}
/* ----------------------------------------------------------------------------- *\
		name        : LoRa_reset

		description : reset module

		arguments   :
			LoRa* LoRa --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_reset(LoRa* _LoRa){
	HAL_GPIO_WritePin(_LoRa->reset_port, _LoRa->reset_pin, GPIO_PIN_RESET);
	DELAY_MS(1);
	HAL_GPIO_WritePin(_LoRa->reset_port, _LoRa->reset_pin, GPIO_PIN_SET);
	DELAY_MS(100);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_gotoMode

		description : set LoRa Op mode

		arguments   :
			LoRa* LoRa    --> LoRa object handler
			mode	        --> select from defined modes

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_gotoMode(LoRa* _LoRa, int mode){
	uint8_t    read;
	uint8_t    data;

	read = LoRa_read(_LoRa, RegOpMode);
	data = read;

	if(mode == SLEEP_MODE){
		data = (read & 0xF8) | 0x00;
	}else if (mode == STNBY_MODE){
		data = (read & 0xF8) | 0x01;
	}else if (mode == TRANSMIT_MODE){
		data = (read & 0xF8) | 0x03;
	}else if (mode == RXCONTIN_MODE){
		data = (read & 0xF8) | 0x05;
	}else if (mode == RXSINGLE_MODE){
		data = (read & 0xF8) | 0x06;
	}else{
		return 0;
	}

	/* current_mode обновляем только после подтверждённой записи - иначе
	 * software-состояние может разойтись с реальным режимом чипа (именно
	 * это давало периодическую поломку: current_mode "менялся", а чип на
	 * самом деле оставался в прежнем режиме из-за потерянной SPI-транзакции). */
	if(!LoRa_writeVerified(_LoRa, RegOpMode, data, LORA_REG_WRITE_MAX_ATTEMPTS)){
		return 0;
	}

	_LoRa->current_mode = mode;
	return 1;
}


/* ----------------------------------------------------------------------------- *\
		name        : LoRa_readReg

		description : read a register(s) by an address and a length,
									then store value(s) at outpur array.
		arguments   :
			LoRa* LoRa        --> LoRa object handler
			uint8_t* address  -->	pointer to the beginning of address array
			uint16_t r_length -->	detemines number of addresse bytes that
														you want to send
			uint8_t* output		--> pointer to the beginning of output array
			uint16_t w_length	--> detemines number of bytes that you want to read

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_readReg(LoRa* _LoRa, uint8_t* address, uint16_t r_length, uint8_t* output, uint16_t w_length){
	HAL_StatusTypeDef status;

	for(uint8_t attempt = 0; attempt < LORA_SPI_MAX_ATTEMPTS; attempt++){
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_RESET);
		status = HAL_SPI_Transmit(_LoRa->hSPIx, address, r_length, LORA_SPI_IO_TIMEOUT_MS);
		if(status == HAL_OK){
			status = HAL_SPI_Receive(_LoRa->hSPIx, output, w_length, LORA_SPI_IO_TIMEOUT_MS);
		}
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_SET);

		if(status == HAL_OK){
			return 1;
		}
		/* Транзакция не прошла (шум/таймаут на шине) - сбрасываем состояние
		 * периферии SPI, чтобы следующая попытка стартовала с чистого листа. */
		HAL_SPI_Abort(_LoRa->hSPIx);
	}
	return 0;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_writeReg

		description : write a value(s) in a register(s) by an address

		arguments   :
			LoRa* LoRa        --> LoRa object handler
			uint8_t* address  -->	pointer to the beginning of address array
			uint16_t r_length -->	detemines number of addresse bytes that
														you want to send
			uint8_t* output		--> pointer to the beginning of values array
			uint16_t w_length	--> detemines number of bytes that you want to send

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_writeReg(LoRa* _LoRa, uint8_t* address, uint16_t r_length, uint8_t* values, uint16_t w_length){
	HAL_StatusTypeDef status;

	for(uint8_t attempt = 0; attempt < LORA_SPI_MAX_ATTEMPTS; attempt++){
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_RESET);
		status = HAL_SPI_Transmit(_LoRa->hSPIx, address, r_length, LORA_SPI_IO_TIMEOUT_MS);
		if(status == HAL_OK){
			status = HAL_SPI_Transmit(_LoRa->hSPIx, values, w_length, LORA_SPI_IO_TIMEOUT_MS);
		}
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_SET);

		if(status == HAL_OK){
			return 1;
		}
		HAL_SPI_Abort(_LoRa->hSPIx);
	}
	return 0;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setLowDaraRateOptimization

		description : set the LowDataRateOptimization flag. Is is mandated for when the symbol length exceeds 16ms.

		arguments   :
			LoRa*	LoRa         --> LoRa object handler
			uint8_t	value        --> 0 to disable, otherwise to enable

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setLowDaraRateOptimization(LoRa* _LoRa, uint8_t value){
	uint8_t	data;
	uint8_t	read;

	read = LoRa_read(_LoRa, RegModemConfig3);

	if(value)
		data = read | 0x08;
	else
		data = read & 0xF7;

	LoRa_write(_LoRa, RegModemConfig3, data);
	DELAY_MS(10);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setAutoLDO

		description : set the LowDataRateOptimization flag automatically based on the symbol length.

		arguments   :
			LoRa*	LoRa         --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setAutoLDO(LoRa* _LoRa){
	double BW[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0};

	LoRa_setLowDaraRateOptimization(_LoRa, (long)((1 << _LoRa->spredingFactor) / ((double)BW[_LoRa->bandWidth])) > 16.0);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setFrequency

		description : set carrier frequency e.g 433 MHz

		arguments   :
			LoRa* LoRa        --> LoRa object handler
			int   freq        --> desired frequency in MHz unit, e.g 434

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setFrequency(LoRa* _LoRa, int freq){
	uint8_t  data;
	uint32_t F;
	F = (freq * 524288)>>5;

	// write Msb:
	data = F >> 16;
	LoRa_write(_LoRa, RegFrMsb, data);
	DELAY_MS(5);

	// write Mid:
	data = F >> 8;
	LoRa_write(_LoRa, RegFrMid, data);
	DELAY_MS(5);

	// write Lsb:
	data = F >> 0;
	LoRa_write(_LoRa, RegFrLsb, data);
	DELAY_MS(5);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setSpreadingFactor

		description : set spreading factor, from 7 to 12.

		arguments   :
			LoRa* LoRa        --> LoRa object handler
			int   SP          --> desired spreading factor e.g 7

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setSpreadingFactor(LoRa* _LoRa, int SF){
	uint8_t	data;
	uint8_t	read;

	if(SF>12)
		SF = 12;
	if(SF<7)
		SF = 7;

	read = LoRa_read(_LoRa, RegModemConfig2);
	DELAY_MS(10);

	data = (SF << 4) + (read & 0x0F);
	LoRa_write(_LoRa, RegModemConfig2, data);
	DELAY_MS(10);

	LoRa_setAutoLDO(_LoRa);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setPower

		description : set power gain.

		arguments   :
			LoRa* LoRa        --> LoRa object handler
			int   power       --> desired power like POWER_17db

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setPower(LoRa* _LoRa, uint8_t power){
	LoRa_write(_LoRa, RegPaConfig, power);
	DELAY_MS(10);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setOCP

		description : set maximum allowed current.

		arguments   :
			LoRa* LoRa        --> LoRa object handler
			int   current     --> desired max currnet in mA, e.g 120

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setOCP(LoRa* _LoRa, uint8_t current){
	uint8_t	OcpTrim = 0;

	if(current<45)
		current = 45;
	if(current>240)
		current = 240;

	if(current <= 120)
		OcpTrim = (current - 45)/5;
	else if(current <= 240)
		OcpTrim = (current + 30)/10;

	OcpTrim = OcpTrim + (1 << 5);
	LoRa_write(_LoRa, RegOcp, OcpTrim);
	DELAY_MS(10);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setTOMsb_setCRCon

		description : set timeout msb to 0xFF + set CRC enable.

		arguments   :
			LoRa* LoRa        --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setTOMsb_setCRCon(LoRa* _LoRa){
	uint8_t read, data;

	read = LoRa_read(_LoRa, RegModemConfig2);

	data = read | 0x07;
	LoRa_write(_LoRa, RegModemConfig2, data);
	DELAY_MS(10);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_setTOMsb_setCRCon

		description : set timeout msb to 0xFF + set CRC enable.

		arguments   :
			LoRa* LoRa        --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_setSyncWord(LoRa* _LoRa, uint8_t syncword){
	LoRa_write(_LoRa, RegSyncWord, syncword);
	DELAY_MS(10);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_read

		description : read a register by an address

		arguments   :
			LoRa*   LoRa        --> LoRa object handler
			uint8_t address     -->	address of the register e.g 0x1D

		returns     : register value
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_read(LoRa* _LoRa, uint8_t address){
	/* 0 по умолчанию: если LoRa_readReg исчерпает все попытки ещё до фазы
	 * приёма (например, транзакция адреса не проходит ни разу), HAL_SPI_Receive
	 * не вызовется вообще, и без этой инициализации отсюда ушёл бы мусор со
	 * стека вместо предсказуемого значения. */
	uint8_t read_data = 0;
	uint8_t data_addr;

	data_addr = address & 0x7F;
	LoRa_readReg(_LoRa, &data_addr, 1, &read_data, 1);

	return read_data;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_write

		description : write a value in a register by an address

		arguments   :
			LoRa*   LoRa        --> LoRa object handler
			uint8_t address     -->	address of the register e.g 0x1D
			uint8_t value       --> value that you want to write

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_write(LoRa* _LoRa, uint8_t address, uint8_t value){
	uint8_t data;
	uint8_t addr;

	addr = address | 0x80;
	data = value;
	LoRa_writeReg(_LoRa, &addr, 1, &data, 1);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_writeVerified

		description : write a value and read it back to confirm the chip actually
					  accepted it - SPI-level success alone doesn't guarantee that
					  (e.g. LongRangeMode в RegOpMode игнорируется чипом вне
					  Sleep-режима). Retries the whole write+verify cycle.

		arguments   :
			LoRa*    LoRa         --> LoRa object handler
			uint8_t  address      --> address of the register e.g 0x01
			uint8_t  value        --> value that you want to write
			uint8_t  max_attempts --> how many write+verify cycles to try

		returns     : 1 if verified, 0 if not confirmed after max_attempts
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_writeVerified(LoRa* _LoRa, uint8_t address, uint8_t value, uint8_t max_attempts){
	for(uint8_t attempt = 0; attempt < max_attempts; attempt++){
		LoRa_write(_LoRa, address, value);
		DELAY_MS(2);
		if(LoRa_read(_LoRa, address) == value){
			return 1;
		}
	}
	return 0;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_BurstWrite

		description : write a set of values in a register by an address respectively

		arguments   :
			LoRa*   LoRa        --> LoRa object handler
			uint8_t address     -->	address of the register e.g 0x1D
			uint8_t *value      --> address of values that you want to write

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_BurstWrite(LoRa* _LoRa, uint8_t address, uint8_t *value, uint8_t length){
	uint8_t addr;
	HAL_StatusTypeDef status;
	addr = address | 0x80;

	for(uint8_t attempt = 0; attempt < LORA_SPI_MAX_ATTEMPTS; attempt++){
		//NSS = 1
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_RESET);

		status = HAL_SPI_Transmit(_LoRa->hSPIx, &addr, 1, LORA_SPI_IO_TIMEOUT_MS);
		if(status == HAL_OK){
			//Write data in FiFo
			status = HAL_SPI_Transmit(_LoRa->hSPIx, value, length, LORA_SPI_IO_TIMEOUT_MS);
		}
		//NSS = 0
		HAL_GPIO_WritePin(_LoRa->CS_port, _LoRa->CS_pin, GPIO_PIN_SET);

		if(status == HAL_OK){
			return;
		}
		HAL_SPI_Abort(_LoRa->hSPIx);
	}
}
/* ----------------------------------------------------------------------------- *\
		name        : LoRa_isvalid

		description : check the LoRa instruct values

		arguments   :
			LoRa* LoRa --> LoRa object handler

		returns     : returns 1 if all of the values were given, otherwise returns 0
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_isvalid(LoRa* _LoRa){

	return 1;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_transmit

		description : Transmit data. Задача блокируется на xTaskNotifyWait в
					  ожидании TxDone от прерывания DIO0 (LoRa_handleDio0IRQ) -
					  CPU в это время свободен для других задач, никакого
					  опроса регистров в цикле нет.

		arguments   :
			LoRa*    LoRa     --> LoRa object handler
			uint8_t  data			--> A pointer to the data you wanna send
			uint8_t	 length   --> Size of your data in Bytes
			uint16_t timeOut	--> Timeout in milliseconds
		returns     : 1 in case of success, 0 in case of timeout
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_transmit(LoRa* _LoRa, uint8_t* data, uint8_t length, uint16_t timeout){
	uint8_t read;
	uint32_t notified = 0;
	BaseType_t gotNotification = pdFALSE;

	int mode = _LoRa->current_mode;
	if(!LoRa_gotoMode(_LoRa, STNBY_MODE)){
		/* Не смогли подтверждённо перейти в Standby - FIFO/регистры трогать
		 * бессмысленно, передачи не будет. */
		return 0;
	}
	read = LoRa_read(_LoRa, RegFiFoTxBaseAddr);
	LoRa_write(_LoRa, RegFiFoAddPtr, read);
	LoRa_write(_LoRa, RegPayloadLength, length);
	LoRa_BurstWrite(_LoRa, RegFiFo, data, length);

	if(LoRa_gotoMode(_LoRa, TRANSMIT_MODE)){
		/* Пока идёт передача, current_mode == TRANSMIT_MODE, поэтому любое
		 * срабатывание DIO0 в этом окне - это TxDone (RxDone физически не может
		 * произойти во время своей же передачи). Чистим оба бита на входе/выходе,
		 * чтобы отбросить случайный устаревший RxDone-флаг, прилетевший в узком
		 * зазоре между выходом из RXCONTIN и переходом в TRANSMIT_MODE. */
		gotNotification = xTaskNotifyWait(LORA_NOTIFY_TXDONE | LORA_NOTIFY_RXDONE,
										   LORA_NOTIFY_TXDONE | LORA_NOTIFY_RXDONE,
										   &notified, pdMS_TO_TICKS(timeout));
	}
	/* Если переход в TRANSMIT_MODE не подтвердился - передача физически не
	 * стартовала, ждать TxDone бессмысленно, gotNotification так и остаётся
	 * pdFALSE. */

	LoRa_write(_LoRa, RegIrqFlags, 0xFF);
	LoRa_gotoMode(_LoRa, mode);

	(void)notified;
	return (gotNotification == pdTRUE) ? 1 : 0;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_startReceiving

		description : Start receiving continuously

		arguments   :
			LoRa*    LoRa     --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_startReceiving(LoRa* _LoRa){
	LoRa_gotoMode(_LoRa, RXCONTIN_MODE);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_Receive

		description : Read received data from module

		arguments   :
			LoRa*    LoRa     --> LoRa object handler
			uint8_t  data			--> A pointer to the array that you want to write bytes in it
			uint8_t	 length   --> Determines how many bytes you want to read

		returns     : The number of bytes received
\* ----------------------------------------------------------------------------- */
uint8_t LoRa_receive(LoRa* _LoRa, uint8_t* data, uint8_t length){
	uint8_t read;
	uint8_t number_of_bytes;
	uint8_t min = 0;

	for(int i=0; i<length; i++)
		data[i]=0;

	/* Если не удалось подтверждённо перейти в Standby - лезть в FIFO
	 * небезопасно (мог остаться в RXCONTIN и продолжать принимать поверх
	 * того, что мы пытаемся читать), пропускаем такт. */
	if(LoRa_gotoMode(_LoRa, STNBY_MODE)){
		read = LoRa_read(_LoRa, RegIrqFlags);
		if((read & 0x40) != 0){
			LoRa_write(_LoRa, RegIrqFlags, 0xFF);
			number_of_bytes = LoRa_read(_LoRa, RegRxNbBytes);
			read = LoRa_read(_LoRa, RegFiFoRxCurrentAddr);
			LoRa_write(_LoRa, RegFiFoAddPtr, read);
			min = length >= number_of_bytes ? number_of_bytes : length;
			for(int i=0; i<min; i++)
				data[i] = LoRa_read(_LoRa, RegFiFo);
		}
	}
	/* Возврат в приём - best-effort в любом случае, включая ветку выше:
	 * если сейчас не получится, ISR/следующий такт этого не заметит, а
	 * оставаться в Standby молча (никогда не приняв следующий пакет) хуже. */
	LoRa_gotoMode(_LoRa, RXCONTIN_MODE);
    return min;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_getRSSI

		description : initialize and set the right setting according to LoRa sruct vars

		arguments   :
			LoRa* LoRa        --> LoRa object handler

		returns     : Returns the RSSI value of last received packet.
\* ----------------------------------------------------------------------------- */
int LoRa_getRSSI(LoRa* _LoRa){
	uint8_t read;
	read = LoRa_read(_LoRa, RegPktRssiValue);
	return -164 + read;
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_handleDio0IRQ

		description : вызывается из HAL_GPIO_EXTI_Callback по пину DIO0.
					  Определяет TxDone/RxDone по текущему режиму модуля и
					  будит ownerTask через уведомление задачи.

		arguments   :
			LoRa* LoRa        --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
void LoRa_handleDio0IRQ(LoRa* _LoRa){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	uint32_t bit = (_LoRa->current_mode == TRANSMIT_MODE) ? LORA_NOTIFY_TXDONE : LORA_NOTIFY_RXDONE;

	if(_LoRa->ownerTask != NULL){
		xTaskNotifyFromISR(_LoRa->ownerTask, bit, eSetBits, &xHigherPriorityTaskWoken);
	}

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ----------------------------------------------------------------------------- *\
		name        : LoRa_init

		description : initialize and set the right setting according to LoRa sruct vars

		arguments   :
			LoRa* LoRa        --> LoRa object handler

		returns     : Nothing
\* ----------------------------------------------------------------------------- */
uint16_t LoRa_init(LoRa* _LoRa){
	uint8_t    data;
	uint8_t    read;

	/* Владелец модуля - задача, вызвавшая LoRa_init. Именно она получит
	 * уведомления о TxDone/RxDone из LoRa_handleDio0IRQ. LoRa_init должен
	 * вызываться из тела RTOS-задачи (после старта планировщика), а не из
	 * main() до xTaskCreate. */
	_LoRa->ownerTask = xTaskGetCurrentTaskHandle();

	if(LoRa_isvalid(_LoRa)){
		/* Включаем LoRa-режим (LongRangeMode=1) и Sleep одной атомарной
		 * write+verify записью, а не двумя раздельными транзакциями
		 * (сначала обычный Sleep, потом отдельно доп-бит LongRangeMode).
		 * По даташиту LongRangeMode принимается чипом только при переходе
		 * через Sleep, и если между этими двумя отдельными записями
		 * терялась SPI-транзакция (шум/таймаут - см. LoRa_readReg/writeReg
		 * без проверки статуса, что и было первопричиной), чип оставался в
		 * FSK/Standby и всё дальнейшее общение с ним било мимо LoRa-регистров.
		 * Явную SLEEP_MODE-константу не используем: current_mode ещё не
		 * инициализирован на этом шаге, пишем regOpMode напрямую. */
		read = LoRa_read(_LoRa, RegOpMode);
		data = (read & 0x78) | 0x80; // LongRangeMode=1, Mode=Sleep(000), остальное как было
		if(!LoRa_writeVerified(_LoRa, RegOpMode, data, LORA_REG_WRITE_MAX_ATTEMPTS)){
			return LORA_NOT_FOUND;
		}
		_LoRa->current_mode = SLEEP_MODE;
		DELAY_MS(10);

		// set frequency:
		LoRa_setFrequency(_LoRa, _LoRa->frequency);

		// set output power gain:
		LoRa_setPower(_LoRa, _LoRa->power);

		// set over current protection:
		LoRa_setOCP(_LoRa, _LoRa->overCurrentProtection);

		// set LNA gain:
		LoRa_write(_LoRa, RegLna, 0x23);

		// set spreading factor, CRC on, and Timeout Msb:
		LoRa_setTOMsb_setCRCon(_LoRa);
		LoRa_setSpreadingFactor(_LoRa, _LoRa->spredingFactor);

		// set Timeout Lsb:
		LoRa_write(_LoRa, RegSymbTimeoutL, 0xFF);

		// set bandwidth, coding rate and expilicit mode:
		// 8 bit RegModemConfig --> | X | X | X | X | X | X | X | X |
		//       bits represent --> |   bandwidth   |     CR    |I/E|
		data = 0;
		data = (_LoRa->bandWidth << 4) + (_LoRa->crcRate << 1);
		LoRa_write(_LoRa, RegModemConfig1, data);
		LoRa_setAutoLDO(_LoRa);

		// set preamble:
		LoRa_write(_LoRa, RegPreambleMsb, _LoRa->preamble >> 8);
		LoRa_write(_LoRa, RegPreambleLsb, _LoRa->preamble >> 0);

		// DIO mapping:   --> DIO: RxDone
		read = LoRa_read(_LoRa, RegDioMapping1);
		data = read | 0x3F;
		LoRa_write(_LoRa, RegDioMapping1, data);

		// goto standby mode:
		if(!LoRa_gotoMode(_LoRa, STNBY_MODE)){
			return LORA_NOT_FOUND;
		}
		DELAY_MS(10);

		read = LoRa_read(_LoRa, RegVersion);
		if(read == 0x12 || read == 0x09)
			return LORA_OK;
		else
			return LORA_NOT_FOUND;
	}
	else {
		return LORA_UNAVAILABLE;
	}
}
