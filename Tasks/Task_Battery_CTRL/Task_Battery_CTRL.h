/*
 * Task_Battery_CTRL.h
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#ifndef TASK_BATTERY_CTRL_TASK_BATTERY_CTRL_H_
#define TASK_BATTERY_CTRL_TASK_BATTERY_CTRL_H_


#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_battery_ctrl_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_battery_ctrl_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_battery_ctrl_NAME          "Task_battery_ctrl"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_battery_ctrl_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_battery_ctrl_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_battery_ctrl_IsRunning(void);

#endif /* TASK_BATTERY_CTRL_TASK_BATTERY_CTRL_H_ */
