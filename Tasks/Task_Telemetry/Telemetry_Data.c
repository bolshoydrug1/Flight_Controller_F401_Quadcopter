/*
 * Telemetry_Data.c
 *
 *  Created on: Jul 4, 2026
 *      Author: konstantin1
 */

#include "Telemetry_Data.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TELEMETRY_LOCK_TIMEOUT_MS	5U

static TelemetryFrame_t s_frame;
static SemaphoreHandle_t s_mutex = NULL;

static void ensure_mutex(void)
{
	if (s_mutex == NULL) {
		taskENTER_CRITICAL();
		if (s_mutex == NULL) {
			s_mutex = xSemaphoreCreateMutex();
		}
		taskEXIT_CRITICAL();
	}
}

void Telemetry_SetHeight(float height_agl_m)
{
	ensure_mutex();

	if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TELEMETRY_LOCK_TIMEOUT_MS)) == pdTRUE) {
		s_frame.height_agl_m = height_agl_m;
		xSemaphoreGive(s_mutex);
	}
}

void Telemetry_SetBaro(float temperature_c, float pressure_mmhg, float altitude_m, float relative_height_m)
{
	ensure_mutex();

	if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TELEMETRY_LOCK_TIMEOUT_MS)) == pdTRUE) {
		s_frame.temperature_c = temperature_c;
		s_frame.pressure_mmhg = pressure_mmhg;
		s_frame.altitude_m = altitude_m;
		s_frame.relative_height_m = relative_height_m;
		xSemaphoreGive(s_mutex);
	}
}

void Telemetry_SetBattery(uint8_t bat_procent)
{
	ensure_mutex();

	if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TELEMETRY_LOCK_TIMEOUT_MS)) == pdTRUE) {
		s_frame.bat_procent = bat_procent;
		xSemaphoreGive(s_mutex);
	}
}

void Telemetry_GetFrame(TelemetryFrame_t *out)
{
	ensure_mutex();

	if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TELEMETRY_LOCK_TIMEOUT_MS)) == pdTRUE) {
		*out = s_frame;
		xSemaphoreGive(s_mutex);
	}

	out->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void Telemetry_BuildPacket(TelemetryPacket_t *out)
{
	out->header.preamble[0] = PACKET_PREAMBLE_BYTE0;
	out->header.preamble[1] = PACKET_PREAMBLE_BYTE1;
	out->header.type = PACKET_TYPE_TELEMETRY;

	Telemetry_GetFrame(&out->payload);

	out->tail.marker[0] = PACKET_TAIL_BYTE0;
	out->tail.marker[1] = PACKET_TAIL_BYTE1;
}

bool Packet_Parse(const uint8_t *buf, uint16_t len, PacketView_t *out)
{
	if (buf == NULL || out == NULL) {
		return false;
	}

	if (len < sizeof(PacketHeader_t) + PACKET_TAIL_LEN) {
		return false;
	}

	if (buf[0] != PACKET_PREAMBLE_BYTE0 || buf[1] != PACKET_PREAMBLE_BYTE1) {
		return false;
	}

	uint8_t type = buf[PACKET_PREAMBLE_LEN];
	uint16_t expected_len;

	switch (type) {
	case PACKET_TYPE_TELEMETRY:
		expected_len = (uint16_t)sizeof(TelemetryPacket_t);
		break;

	default:
		return false;
	}

	if (len != expected_len) {
		return false;
	}

	const uint8_t *tail = buf + len - PACKET_TAIL_LEN;
	if (tail[0] != PACKET_TAIL_BYTE0 || tail[1] != PACKET_TAIL_BYTE1) {
		return false;
	}

	out->type = type;
	out->payload = buf + sizeof(PacketHeader_t);
	out->payload_len = (uint16_t)(expected_len - sizeof(PacketHeader_t) - PACKET_TAIL_LEN);
	return true;
}
