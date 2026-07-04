/*
 * Task_Telemetry.h
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#ifndef TASK_TELEMETRY_TASK_TELEMETRY_H_
#define TASK_TELEMETRY_TASK_TELEMETRY_H_


#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_telemetry_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_telemetry_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_telemetry_NAME          "Task_telemetry"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_telemetry_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_telemetry_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_telemetry_IsRunning(void);

/**
 * @brief Обработчик прерывания EXTI по пину DIO0 LoRa-модуля.
 * Вызывается из HAL_GPIO_EXTI_Callback (см. Task_HC_SR04.c). Сам разбирается,
 * относится ли пин к LoRa, и будит задачу телеметрии через LoRa_handleDio0IRQ.
 * @param GPIO_Pin Пин, вызвавший прерывание.
 */
void Task_telemetry_HandleExti(uint16_t GPIO_Pin);

#endif /* TASK_TELEMETRY_TASK_TELEMETRY_H_ */
