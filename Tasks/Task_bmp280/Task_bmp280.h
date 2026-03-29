/*
 * Task_bmp280.h
 *
 *  Created on: Mar 29, 2026
 *      Author: konstantin1
 */

#ifndef TASK_BMP280_TASK_BMP280_H_
#define TASK_BMP280_TASK_BMP280_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_bmp280_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_bmp280_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_bmp280_NAME          "Task_bmp280"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_bmp280_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_bmp280_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_bmp280_IsRunning(void);


#endif /* TASK_BMP280_TASK_BMP280_H_ */
