/*
 * Task_pmw3901.h
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#ifndef TASK_PMW3901_TASK_PMW3901_H_
#define TASK_PMW3901_TASK_PMW3901_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_pmw3901_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_pmw3901_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_pmw3901_NAME          "Task_pmw3901"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_pmw3901_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_pmw3901_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_pmw3901_IsRunning(void);


#endif /* TASK_PMW3901_TASK_PMW3901_H_ */
