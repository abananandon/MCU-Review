#ifndef __SYS_H
#define __SYS_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

void stm32_system_clock_init(uint32_t plln);

#endif