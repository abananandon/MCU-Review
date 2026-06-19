#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "led.h"
#include "delay.h"
#include "sys.h"

int main(void)
{
    HAL_Init();
    stm32_system_clock_init(RCC_PLL_MUL9);
    delay_init(SystemCoreClock);
    led_hardware_init();

    while (1) {
        led_on(0);
        delay_ms(500);
        led_off(0);
        delay_ms(500);
    }

    return 0;
}