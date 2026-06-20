#include "uart.h"
#include <string.h>

typedef struct {
    GPIO_TypeDef *tx_port;
    uint32_t      tx_pin;
    GPIO_TypeDef *rx_port;
    uint32_t      rx_pin;
    uint8_t       remap_flag;       /* copy of config→remap_flag for consistency */
} UartPinMap_t;

static const UartPinMap_t g_uart_pin_map[UART_INSTANCE_MAX][2] = {
    /* USART1 */ {
        {GPIOA, GPIO_PIN_9,  GPIOA, GPIO_PIN_10, 0},     /* remap_flag=0 */
        {GPIOB, GPIO_PIN_6,  GPIOB, GPIO_PIN_7,  1}      /* remap_flag=1 */
    },
    /* USART2 */ {
        {GPIOA, GPIO_PIN_2,  GPIOA, GPIO_PIN_3,  0},
        {GPIOD, GPIO_PIN_5,  GPIOD, GPIO_PIN_6,  1}
    },
    /* USART3 */ {
        {GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_11, 0},
        {GPIOC, GPIO_PIN_10, GPIOC, GPIO_PIN_11, 1}
    },
    /* UART4 */ {
        {GPIOC, GPIO_PIN_10, GPIOC, GPIO_PIN_11, 0},
        {0}                                              /* no remap */
    },
    /* UART5 */ {
        {GPIOC, GPIO_PIN_12, GPIOD, GPIO_PIN_2,  0},
        {0}                                              /* no remap */
    }
};

/*──────────────────────────────────────────────────────────────────────
 * Global instance table — ISR uses uart_index() to locate the active
 * UartInstance for a given USARTx interrupt.
 *──────────────────────────────────────────────────────────────────────*/
static UartInstance_t *g_uart_table[UART_INSTANCE_MAX];

/*──────────────────────────────────────────────────────────────────────
 * Helpers
 *──────────────────────────────────────────────────────────────────────*/

static int uart_get_index(USART_TypeDef *instance)
{
    if      (instance == USART1) return 0;
    else if (instance == USART2) return 1;
    else if (instance == USART3) return 2;
    else if (instance == UART4)  return 3;
    else if (instance == UART5)  return 4;
    return -1;
}

static IRQn_Type uart_get_irq(USART_TypeDef *instance)
{
    if      (instance == USART1) return USART1_IRQn;
    else if (instance == USART2) return USART2_IRQn;
    else if (instance == USART3) return USART3_IRQn;
    else if (instance == UART4)  return UART4_IRQn;
    else if (instance == UART5)  return UART5_IRQn;
    return (IRQn_Type)-1;
}

/*──────────────────────────────────────────────────────────────────────
 * hw_init — GPIO, AFIO remap, NVIC
 *──────────────────────────────────────────────────────────────────────*/
static HAL_StatusTypeDef uart_hw_init(UartInstance_t *uart, uint32_t remap_flag)
{
    GPIO_InitTypeDef gpio = {0};
    const UartPinMap_t *pin;
    int idx;

    idx = uart_get_index(uart->Instance);
    if (idx < 0) {
        return HAL_ERROR;
    }

    pin = &g_uart_pin_map[idx][remap_flag ? 1 : 0];
    if (pin->tx_port == NULL) {
        /* Invalid remap selection (e.g. UART4 remap_flag=1) */
        return HAL_ERROR;
    }

    /* ── UART peripheral clock ── */
    /* Using the __HAL_RCC_xxx_CLK_ENABLE() macros avoids writing a big
       switch.  Alternatively we could write `RCC->APBxENR |= mask`. */
    if      (uart->Instance == USART1) __HAL_RCC_USART1_CLK_ENABLE();
    else if (uart->Instance == USART2) __HAL_RCC_USART2_CLK_ENABLE();
    else if (uart->Instance == USART3) __HAL_RCC_USART3_CLK_ENABLE();
    else if (uart->Instance == UART4)  __HAL_RCC_UART4_CLK_ENABLE();
    else if (uart->Instance == UART5)  __HAL_RCC_UART5_CLK_ENABLE();

    /* ── AFIO remap (USART1/2/3 only) ── */
    if (remap_flag) {
        __HAL_RCC_AFIO_CLK_ENABLE();
        if      (uart->Instance == USART1) __HAL_AFIO_REMAP_USART1_ENABLE();
        else if (uart->Instance == USART2) __HAL_AFIO_REMAP_USART2_ENABLE();
        else if (uart->Instance == USART3) __HAL_AFIO_REMAP_USART3_ENABLE();
    }

    /* ── GPIO clocks (TX / RX ports may be the same or different) ── */
    if (pin->tx_port == GPIOA || pin->rx_port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    if (pin->tx_port == GPIOB || pin->rx_port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    if (pin->tx_port == GPIOC || pin->rx_port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    if (pin->tx_port == GPIOD || pin->rx_port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── TX pin: alternate-function push-pull ── */
    gpio.Pin   = pin->tx_pin;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(pin->tx_port, &gpio);

    /* ── RX pin: input floating ── */
    gpio.Pin   = pin->rx_pin;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(pin->rx_port, &gpio);

    /* ── NVIC: priority 1, enable ── */
    HAL_NVIC_SetPriority(uart_get_irq(uart->Instance), 1, 0);
    HAL_NVIC_EnableIRQ(uart_get_irq(uart->Instance));

    return HAL_OK;
}

static void uart_irq_handler(UartInstance_t *uart)
{
    uint8_t tmp;

    if (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_ORE) != RESET) {
        __HAL_UART_CLEAR_OREFLAG(&uart->huart);
        (void)uart->Instance->SR;
        tmp = (uint8_t)uart->Instance->DR;
        RingBuffer_Push(&uart->rx_rb, tmp);
    }

    while (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_RXNE) != RESET) {
        tmp = uart->Instance->DR;
        RingBuffer_Push(&uart->rx_rb, tmp);
    }

    if (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_IDLE) != RESET) {
        uart->rx_flag = 1;
        __HAL_UART_CLEAR_IDLEFLAG(&uart->huart);
    }

    if (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_TXE) != RESET) {
        if (!RingBuffer_IsEmpty(&uart->tx_rb)) {
            uart->Instance->DR =  RingBuffer_Pop(&uart->tx_rb);

            if (RingBuffer_IsEmpty(&uart->tx_rb)) {
                __HAL_UART_DISABLE_IT(&uart->huart, UART_IT_TXE);
                __HAL_UART_ENABLE_IT(&uart->huart, UART_IT_TC);
            }
        }
        else {
            __HAL_UART_ENABLE_IT(&uart->huart, UART_IT_TC);
            __HAL_UART_DISABLE_IT(&uart->huart, UART_IT_TXE);
        }
    }

    if (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_TC) != RESET) {
        uart->tx_flag = 1;

        __HAL_UART_DISABLE_IT(&uart->huart, UART_IT_TC);
        __HAL_UART_CLEAR_FLAG(&uart->huart, UART_FLAG_TC);
    }
}

/*──────────────────────────────────────────────────────────────────────
 * Public API
 *──────────────────────────────────────────────────────────────────────*/

HAL_StatusTypeDef Uart_Init(UartInstance_t *uart, USART_TypeDef *instance,
                             const UartConfig_t *config)
{
    HAL_StatusTypeDef ret;
    int idx;

    if (uart == NULL || instance == NULL || config == NULL) {
        return HAL_ERROR;
    }

    idx = uart_get_index(instance);
    if (idx < 0) {
        return HAL_ERROR;
    }

    /* ── Zero the entire instance to start clean ── */
    memset(uart, 0, sizeof(*uart));

    /* ── Bind hardware ── */
    uart->Instance        = instance;
    uart->huart.Instance  = instance;
    uart->huart.Init      = config->uart_init;
    uart->state           = UART_STATE_IDLE;

    /* ── GPIO + AFIO remap + NVIC ── */
    ret = uart_hw_init(uart, config->remap_flag);
    if (ret != HAL_OK) {
        return ret;
    }

    /* ── HAL handles baud rate, stop bits, word length etc. ──
     *     HAL_UART_MspInit() weak default is a no-op, which is fine —
     *     we already configured GPIO/NVIC in uart_hw_init().          */
    ret = HAL_UART_Init(&uart->huart);
    if (ret != HAL_OK) {
        return ret;
    }

    /* ── Register instance for ISR lookup ── */
    g_uart_table[idx] = uart;

    /* ── Init ring buffers ── */
    RingBuffer_Init(&uart->rx_rb, uart->rx_buf, UART_RX_BUF_SIZE);
    RingBuffer_Init(&uart->tx_rb, uart->tx_buf, UART_TX_BUF_SIZE);

    /* ── Enable RXNE + IDLE interrupts (register level) ── */
    uart->Instance->CR1 |= USART_CR1_RXNEIE | USART_CR1_IDLEIE;

    return HAL_OK;
}

void Uart_Send(UartInstance_t *uart, uint8_t *data, uint16_t len)
{
    if (uart == NULL || data == NULL) {
        return;
    }

    uart->tx_flag = 0;
    for (uint16_t i = 0; i < len; i++) {
        RingBuffer_Push(&uart->tx_rb, data[i]);
    }
    __HAL_UART_ENABLE_IT(&uart->huart, UART_IT_TXE);
}

uint16_t Uart_Read(UartInstance_t *uart, uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;

    if (uart == NULL || buf == NULL) {
        return 0;
    }

    while (!RingBuffer_IsEmpty(&uart->rx_rb) && i < len) {
        buf[i++] = RingBuffer_Pop(&uart->rx_rb);
    }

    if (RingBuffer_IsEmpty(&uart->rx_rb)) {
        uart->rx_flag = 0;
    }

    return i;
}

void Uart_PollEvents(UartInstance_t *uart)
{
    if (uart == NULL) {
        return;
    }

    if (uart->rx_flag) {
        if (RingBuffer_IsEmpty(&uart->rx_rb)) {
            uart->rx_flag = 0;
        } else if (uart->on_frame_received) {
            uart->on_frame_received(uart);
        }
    }

    if (uart->tx_flag) {
        uart->tx_flag = 0;
        if (uart->on_frame_sent) {
            uart->on_frame_sent(uart);
        }
    }
}

void USART1_IRQHandler(void) { uart_irq_handler(g_uart_table[0]); }
void USART2_IRQHandler(void) { uart_irq_handler(g_uart_table[1]); }
void USART3_IRQHandler(void) { uart_irq_handler(g_uart_table[2]); }
void UART4_IRQHandler(void)  { uart_irq_handler(g_uart_table[3]); }
void UART5_IRQHandler(void)  { uart_irq_handler(g_uart_table[4]); }