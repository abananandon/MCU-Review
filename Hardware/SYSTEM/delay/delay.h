#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

void delay_init(uint32_t sys_clk_freq);
void delay_us(uint32_t time_us);
void delay_ms(uint32_t time_ms);

#endif