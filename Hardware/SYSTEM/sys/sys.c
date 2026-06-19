#include "sys.h"

/*──────────────────────────────────────────────────────────────────────
 * SysTick ISR
 *
 * HAL_Init() and HAL_RCC_ClockConfig() both call HAL_InitTick() which
 * enables the SysTick interrupt via CMSIS SysTick_Config().  Without
 * this handler the CPU vectors into Default_Handler (infinite loop)
 * ~1 ms after boot.
 *
 * When USE_RTOS is later set to 1 and the HAL timebase is moved to a
 * hardware timer, this handler should be replaced by the RTOS tick ISR.
 *──────────────────────────────────────────────────────────────────────*/
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void stm32_system_clock_init(uint32_t plln)
{
    HAL_StatusTypeDef ret;
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init.HSEState = RCC_HSE_ON;
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;
    rcc_osc_init.PLL.PLLMUL = plln;

    ret = HAL_RCC_OscConfig(&rcc_osc_init);

    if (ret != HAL_OK) {
        /* ── HSE failed → fall back to HSI (8 MHz), stay alive ── */
        rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        rcc_osc_init.HSIState = RCC_HSI_ON;
        rcc_osc_init.HSEState = RCC_HSE_OFF;
        rcc_osc_init.PLL.PLLState = RCC_PLL_NONE;  /* skip PLL */

        ret = HAL_RCC_OscConfig(&rcc_osc_init);
        if (ret != HAL_OK) {
            while (1);  /* HSI also failed — hardware fault, trap */
        }

        rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    } else {
        /* HSE + PLL OK → use PLL output */
        rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    }

    rcc_clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                              RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_SYSCLK_DIV2;
    rcc_clk_init.APB2CLKDivider = RCC_SYSCLK_DIV1;

    uint32_t flash_latency = (plln <= RCC_PLL_MUL6) ? FLASH_LATENCY_1
                                                     : FLASH_LATENCY_2;

    ret = HAL_RCC_ClockConfig(&rcc_clk_init, flash_latency);
    if (ret != HAL_OK) {
        while (1);
    }

    SystemCoreClockUpdate();
}



