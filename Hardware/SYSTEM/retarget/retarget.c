#include <stdint.h>
#include "uart.h"
#include "stm32f1xx_hal.h"

static UartInstance_t g_debug_uart;
static UartConfig_t g_debug_uart_config = {
    .uart_init.BaudRate = 115200,
    .uart_init.WordLength = UART_WORDLENGTH_8B,
    .uart_init.StopBits = UART_STOPBITS_1,
    .uart_init.Parity = UART_PARITY_NONE,
    .uart_init.Mode = UART_MODE_TX_RX,
    .uart_init.HwFlowCtl = UART_HWCONTROL_NONE,
    .uart_init.OverSampling = UART_OVERSAMPLING_16,
    .remap_flag = 0
};

void printf_init(void)
{
    Uart_Init(&g_debug_uart, USART1, &g_debug_uart_config);
}

int _write(int fd, char *ptr, int len)
{
    Uart_Send(&g_debug_uart, (uint8_t *)ptr, len);
    while (!g_debug_uart.tx_flag);       // 等发送完成
    g_debug_uart.tx_flag = 0;
    return len;
}
