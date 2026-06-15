/*
 * Task_esc_ctrl.h
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#ifndef TASK_esc_ctrl_TASK_esc_ctrl_H_
#define TASK_esc_ctrl_TASK_esc_ctrl_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_esc_ctrl_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_esc_ctrl_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_esc_ctrl_NAME          "Task_esc_ctrl"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_esc_ctrl_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_esc_ctrl_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_esc_ctrl_IsRunning(void);


#endif /* TASK_esc_ctrl_TASK_esc_ctrl_H_ */
