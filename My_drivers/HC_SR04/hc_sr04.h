/*
 * hc_sr04.h
 *
 *  Created on: Jun 18, 2026
 *      Author: konstantin1
 */

#ifndef HC_SR04_HC_SR04_H_
#define HC_SR04_HC_SR04_H_

#include <stdint.h>
#include "main.h"

#define HC_SR04_ECHO_IRQ_PRIORITY   10
#define HC_SR04_TIM_PERIOD_MAX      65535U
#define HC_SR04_TIM_TICK_US         1U

// Структура объекта датчика
// Структура датчика
typedef struct {
    GPIO_TypeDef *trig_port;
    uint16_t      trig_pin;
    GPIO_TypeDef *echo_port;
    uint16_t      echo_pin;
    TIM_HandleTypeDef*	htim;

    uint64_t      pulse_start_us;
    uint64_t      pulse_end_us;
    uint32_t      pulse_us;           // таймаут ожидания эхо (мкс)

    float      distance_mm;          // последнее измерение
} hc_sr04_t;

// Инициализация таймера (1 мкс) и прерываний echo
HAL_StatusTypeDef hc_sr04_init(hc_sr04_t *dev);

// Запуск нового измерения (вызывается из задачи)
void hc_sr04_trigger(hc_sr04_t *dev);

// Получить расстояние (мм)
float hc_sr04_get_distance(hc_sr04_t *dev, float temp);

void echo_start_it(hc_sr04_t *dev);

void echo_end_it(hc_sr04_t *dev);

#endif /* HC_SR04_HC_SR04_H_ */
