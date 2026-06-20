#ifndef __LED_H
#define __LED_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define LED_NUM 2

typedef enum {
    LED_ACTIVE_LOW = 0,
    LED_ACTIVE_HIGH
} LedActiveLevel_t;

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON
} LedState_t;

typedef struct {
    GPIO_TypeDef        *port;
    uint32_t            pin;
    LedActiveLevel_t    active_level;
    LedState_t          state;
    GPIO_InitTypeDef    gpio;
} Led_t;

HAL_StatusTypeDef Led_Hardware_Init(void);
HAL_StatusTypeDef Led_On(uint8_t index);
HAL_StatusTypeDef Led_Off(uint8_t index);
HAL_StatusTypeDef Led_Toggle(uint8_t index);

#endif



