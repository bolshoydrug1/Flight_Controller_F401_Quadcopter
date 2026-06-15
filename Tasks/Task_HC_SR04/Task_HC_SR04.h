/*
 * Task_HC_SR04.h
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#ifndef TASK_hc_sr04_TASK_hc_sr04_H_
#define TASK_hc_sr04_TASK_hc_sr04_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_hc_sr04_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_hc_sr04_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_hc_sr04_NAME          "Task_hc_sr04"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_hc_sr04_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_hc_sr04_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_hc_sr04_IsRunning(void);



#endif /* TASK_HC_SR04_TASK_HC_SR04_H_ */
