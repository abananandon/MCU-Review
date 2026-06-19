#include "led.h"

static HAL_StatusTypeDef led_clock_enable(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static Led_t g_led[LED_NUM] = {
    [0] = {
        .port = GPIOA,
        .pin = GPIO_PIN_8,
        .active_level = LED_ACTIVE_LOW,
        .state = LED_STATE_OFF,
        .gpio = {0}
    },
    [1] = {
        .port = GPIOD,
        .pin = GPIO_PIN_2,
        .active_level = LED_ACTIVE_LOW,
        .state = LED_STATE_OFF,
        .gpio = {0}
    }
};

HAL_StatusTypeDef led_on(uint8_t index)
{
    if (index < 0 || index > LED_NUM) {
        return HAL_ERROR;
    }

    if (g_led[index].active_level == LED_ACTIVE_LOW) {
        HAL_GPIO_WritePin(g_led[index].port, g_led[index].pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(g_led[index].port, g_led[index].pin, GPIO_PIN_SET);
    }
    g_led[index].state = LED_STATE_ON;

    return HAL_OK;
}

HAL_StatusTypeDef led_off(uint8_t index)
{
    if (index < 0 || index > LED_NUM) {
        return HAL_ERROR;
    }

    if (g_led[index].active_level == LED_ACTIVE_LOW) {
        HAL_GPIO_WritePin(g_led[index].port, g_led[index].pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(g_led[index].port, g_led[index].pin, GPIO_PIN_RESET);
    }
    g_led[index].state = LED_STATE_OFF;

    return HAL_OK;
}

HAL_StatusTypeDef led_toggle(uint8_t index)
{
    if (index < 0 || index > LED_NUM) {
        return HAL_ERROR;
    }

    if (g_led[index].state == LED_STATE_OFF) {
        return led_on(index);
    } else {
        return led_off(index);
    }
}

HAL_StatusTypeDef led_hardware_init(void)
{
    HAL_StatusTypeDef ret = HAL_ERROR;

    for (uint8_t i = 0; i < LED_NUM; i++) {
        ret = led_clock_enable(g_led[i].port);
        if (ret != HAL_OK) {
            return HAL_ERROR;
        }

        g_led[i].gpio.Pin = g_led[i].pin;
        g_led[i].gpio.Mode = GPIO_MODE_OUTPUT_PP;
        g_led[i].gpio.Pull = GPIO_PULLDOWN;
        g_led[i].gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(g_led[i].port, &g_led[i].gpio);

        if (g_led[i].state  == LED_STATE_OFF) {
            ret = led_off(i);
        } else {
            ret = led_on(i);
        }
        if (ret != HAL_OK) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

