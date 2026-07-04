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

#include <stdint.h>

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

	// TODO: добавить поля, когда будут реализованы Task_MPU6050 / Task_ESC_CTRL / Task_Battery_CTRL
} TelemetryFrame_t;
#pragma pack(pop)

void Telemetry_SetHeight(float height_agl_m);
void Telemetry_SetBaro(float temperature_c, float pressure_mmhg, float altitude_m, float relative_height_m);

/**
 * @brief Забирает потокобезопасную копию текущего снимка телеметрии.
 * @param out Куда скопировать кадр. timestamp_ms проставляется на момент вызова.
 */
void Telemetry_GetFrame(TelemetryFrame_t *out);

#endif /* TASK_TELEMETRY_TELEMETRY_DATA_H_ */
