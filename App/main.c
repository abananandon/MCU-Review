#include <stdint.h>
#include <stdio.h>
#include "stm32f1xx_hal.h"
#include "led.h"
#include "delay.h"
#include "sys.h"
#include "retarget.h"

int main(void)
{
    HAL_Init();
    stm32_system_clock_init(RCC_PLL_MUL9);
    delay_init(SystemCoreClock);
    Led_Hardware_Init();
    printf_init();

    printf("HelloWorld! \r\n");
    printf("Init... \r\n");
    while (1) {
        Led_On(0);
        printf("LED%d: On\r\n", 0);
        delay_ms(500);
        Led_Off(0);
        printf("LED%d: Off\r\n", 0);
        delay_ms(500);
    }

    return 0;
}