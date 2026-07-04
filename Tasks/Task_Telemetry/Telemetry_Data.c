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

void Telemetry_GetFrame(TelemetryFrame_t *out)
{
	ensure_mutex();

	if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TELEMETRY_LOCK_TIMEOUT_MS)) == pdTRUE) {
		*out = s_frame;
		xSemaphoreGive(s_mutex);
	}

	out->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
