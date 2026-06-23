/*
 * hc_sro4.c
 *
 *  Created on: Jun 18, 2026
 *      Author: konstantin1
 */
#include "hc_sr04.h"
#include "tim.h"

static uint32_t hc_sr04_get_tim_clk(TIM_TypeDef *instance)
{
    uint32_t tim_clk;

    if (instance == TIM1 || instance == TIM9 ||
        instance == TIM10 || instance == TIM11)
    {
        tim_clk = HAL_RCC_GetPCLK2Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
        {
            tim_clk *= 2U;
        }
    }
    else
    {
        tim_clk = HAL_RCC_GetPCLK1Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        {
            tim_clk *= 2U;
        }
    }

    return tim_clk;
}

static HAL_StatusTypeDef hc_sr04_tim_init(hc_sr04_t *dev)
{
    uint32_t tim_clk = hc_sr04_get_tim_clk(dev->htim->Instance);
    uint32_t prescaler = (tim_clk / 1000000U) - 1U;

    dev->htim->Init.Prescaler = prescaler;
    dev->htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    dev->htim->Init.Period = HC_SR04_TIM_PERIOD_MAX;
    dev->htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    dev->htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(dev->htim) != HAL_OK)
    {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_COUNTER(dev->htim, 0);
    __HAL_TIM_DISABLE(dev->htim);

    return HAL_OK;
}

static IRQn_Type hc_sr04_get_exti_irq(uint16_t pin)
{
    switch (pin)
    {
        case GPIO_PIN_0:  return EXTI0_IRQn;
        case GPIO_PIN_1:  return EXTI1_IRQn;
        case GPIO_PIN_2:  return EXTI2_IRQn;
        case GPIO_PIN_3:  return EXTI3_IRQn;
        case GPIO_PIN_4:  return EXTI4_IRQn;
        case GPIO_PIN_5:
        case GPIO_PIN_6:
        case GPIO_PIN_7:
        case GPIO_PIN_8:
        case GPIO_PIN_9:  return EXTI9_5_IRQn;
        default:          return EXTI15_10_IRQn;
    }
}

static void hc_sr04_echo_irq_init(hc_sr04_t *dev)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    IRQn_Type irq = hc_sr04_get_exti_irq(dev->echo_pin);

    GPIO_InitStruct.Pin = dev->echo_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(dev->echo_port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(irq, HC_SR04_ECHO_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(irq);
}

HAL_StatusTypeDef hc_sr04_init(hc_sr04_t *dev)
{
    if (dev == NULL || dev->htim == NULL)
    {
        return HAL_ERROR;
    }

    if (hc_sr04_tim_init(dev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    hc_sr04_echo_irq_init(dev);

    dev->pulse_start_us = 0;
    dev->pulse_end_us = 0;
    dev->pulse_us = 0;
    dev->distance_mm = 0;

    return HAL_OK;
}

// Запуск нового измерения (вызывается из задачи)
void hc_sr04_trigger(hc_sr04_t *dev)
{
	HAL_TIM_Base_Start(dev->htim);  // Запускаем таймер один раз

	HAL_GPIO_WritePin(dev->trig_port, dev->trig_pin, GPIO_PIN_SET);
    // Задержка 10 микросекунд с использованием таймера
    uint32_t start = __HAL_TIM_GET_COUNTER(dev->htim);
    while ((__HAL_TIM_GET_COUNTER(dev->htim) - start) < 10U) {
        // Активное ожидание (busy-wait)
        __NOP();  // можно добавить для пустой инструкции
    }
	HAL_GPIO_WritePin(dev->trig_port, dev->trig_pin, GPIO_PIN_RESET);
}

// Получить расстояние (мм)
float hc_sr04_get_distance(hc_sr04_t *dev, float temp)
{
	//TODO: сделать подстройку температуры с датчика BMP280
	float speed_of_sound = 331.5 + 0.6 * temp; // м/с
	float speed_mm_per_us = speed_of_sound * 1000.0 / 1000000.0; // мм/мкс

	dev->pulse_us = dev->pulse_end_us - dev->pulse_start_us;
	dev->distance_mm = (speed_mm_per_us * dev->pulse_us) / 2.0;
	HAL_TIM_Base_Stop(dev->htim);
	__HAL_TIM_SET_COUNTER(dev->htim, 0);
	return dev->distance_mm;
}


void echo_start_it(hc_sr04_t *dev)
{
	dev->pulse_start_us = __HAL_TIM_GET_COUNTER(dev->htim);
}

void echo_end_it(hc_sr04_t *dev)
{
	dev->pulse_end_us = __HAL_TIM_GET_COUNTER(dev->htim);
}
