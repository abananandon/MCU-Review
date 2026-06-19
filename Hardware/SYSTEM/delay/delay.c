#include "delay.h"

static uint32_t us_period;

void delay_init(uint32_t sys_clk_freq)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    us_period = sys_clk_freq / 1000000;
}

void delay_us(uint32_t time_us)
{
    uint32_t delay_us_begin_val;
    uint32_t delay_us_tick;

    delay_us_begin_val = DWT->CYCCNT;
    delay_us_tick = time_us * us_period;

    while ((DWT->CYCCNT - delay_us_begin_val) < delay_us_tick) {
        __NOP();
    }
}

void delay_ms(uint32_t time_ms)
{
    for (uint32_t i = 0; i < time_ms; i++) {
        delay_us(1000);
    }
}