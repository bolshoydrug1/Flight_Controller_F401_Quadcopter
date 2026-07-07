/*
 * Telemetry_Data.h
 *
 *  Общий потокобезопасный "снимок" телеметрии. Задачи с датчиками (bmp280,
 *  hc_sr04, в будущем mpu6050/esc_ctrl/battery_ctrl) кладут сюда последние
 *  измеренные значения через сеттеры, а Task_Telemetry читает готовый кадр
 *  через Telemetry_GetFrame() и отправляет его по LoRa.
 *
 *  Created on: Jul 4, 2026
 *      Author: konstantin1
 */

#ifndef TASK_TELEMETRY_TELEMETRY_DATA_H_
#define TASK_TELEMETRY_TELEMETRY_DATA_H_

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 * Общий формат пакета поверх LoRa:
 *
 *   [ PREAMBLE (2 байта) ][ TYPE (1 байт) ][ payload, зависит от TYPE ][ TAIL (2 байта) ]
 *
 * PREAMBLE и TAIL - фиксированные байты-маркеры начала/конца пакета.
 * TYPE идёт сразу после преамбулы и определяет, как разбирать payload и
 * какой у пакета полный размер (у каждого типа он фиксирован).
 * ------------------------------------------------------------------- */

#define PACKET_PREAMBLE_BYTE0		0xAAU
#define PACKET_PREAMBLE_BYTE1		0xAAU
#define PACKET_PREAMBLE_LEN		2U

#define PACKET_TAIL_BYTE0		0x55U
#define PACKET_TAIL_BYTE1		0x55U
#define PACKET_TAIL_LEN			2U

/* Типы пакетов (значение байта TYPE, сразу после преамбулы) */
#define PACKET_TYPE_TELEMETRY		0x01U

//Взлёт на высоту X
#define PACKET_TYPE_SET_HIGHT		0x02U
//Посадка

//Аварийное отключение

//Движение вправо\влево\вверх\вниз\вперёд\назад

//Поворот по часовой\против часовой стрелки


#pragma pack(push, 1)
typedef struct {
	uint8_t preamble[PACKET_PREAMBLE_LEN];
	uint8_t type;
} PacketHeader_t;

typedef struct {
	uint8_t marker[PACKET_TAIL_LEN];
} PacketTail_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint32_t timestamp_ms;

	// HC-SR04
	float height_agl_m;

	// BMP280
	float temperature_c;
	float pressure_mmhg;
	float altitude_m;
	float relative_height_m;

	//Battery
	uint8_t bat_procent;

	//GPS ( × 10^7)
	int32_t width_gps;
	int32_t longitude_gps;
	int16_t angle_from_north;

	//Errors
	uint8_t error;

} TelemetryFrame_t;

/* Пакет типа PACKET_TYPE_TELEMETRY целиком, как он уходит в эфир. */
typedef struct {
	PacketHeader_t header;
	TelemetryFrame_t payload;
	PacketTail_t tail;
} TelemetryPacket_t;
#pragma pack(pop)

/**
 * @brief Разобранный входящий пакет: тип и указатель на его payload внутри
 * исходного буфера (без преамбулы/типа/хвоста).
 */
typedef struct {
	uint8_t type;
	const void *payload;
	uint16_t payload_len;
} PacketView_t;

void Telemetry_SetHeight(float height_agl_m);
void Telemetry_SetBaro(float temperature_c, float pressure_mmhg, float altitude_m, float relative_height_m);

/**
 * @brief Забирает потокобезопасную копию текущего снимка телеметрии.
 * @param out Куда скопировать кадр. timestamp_ms проставляется на момент вызова.
 */
void Telemetry_GetFrame(TelemetryFrame_t *out);

/**
 * @brief Собирает готовый к передаче по LoRa пакет телеметрии: преамбула +
 * PACKET_TYPE_TELEMETRY + текущий снимок телеметрии + хвост.
 */
void Telemetry_BuildPacket(TelemetryPacket_t *out);

/**
 * @brief Разбирает сырой буфер, принятый по LoRa (LoRa_receive): проверяет
 * преамбулу и хвост, по байту TYPE определяет ожидаемую длину пакета и
 * отдаёт вызывающему код тип + указатель на payload для дальнейшей
 * расшифровки (см. PACKET_TYPE_*).
 * @param buf Сырые байты, принятые LoRa_receive.
 * @param len Длина буфера (LoRa_receive возвращает фактическую длину пакета).
 * @param out Куда записать разобранный пакет.
 * @return true, если преамбула/тип/длина/хвост прошли проверку.
 */
bool Packet_Parse(const uint8_t *buf, uint16_t len, PacketView_t *out);

#endif /* TASK_TELEMETRY_TELEMETRY_DATA_H_ */
