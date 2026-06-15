/*
 * Task_MPU6050.h
 *
 *  Created on: Jun 15, 2026
 *      Author: konstantin1
 */

#ifndef TASK_mpu6050_TASK_mpu6050_H_
#define TASK_mpu6050_TASK_mpu6050_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// Конфигурация задачи (можно менять под свои нужды)
#define TASK_mpu6050_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)
#define TASK_mpu6050_PRIORITY      (tskIDLE_PRIORITY + 4)
#define TASK_mpu6050_NAME          "Task_mpu6050"

/**
 * @brief Запускает задачу.
 * Если задача уже запущена, функция ничего не делает.
 */
void Task_mpu6050_Start(void);

/**
 * @brief Останавливает (удаляет) задачу.
 */
void Task_mpu6050_Stop(void);

/**
 * @brief Проверяет статус задачи.
 * @return true, если задача запущена и выполняется.
 * @return false, если задача остановлена или удалена.
 */
bool Task_mpu6050_IsRunning(void);

#endif /* TASK_mpu6050_TASK_mpu6050_H_ */
