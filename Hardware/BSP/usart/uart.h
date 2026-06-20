#ifndef __UART_H
#define __UART_H

#include "stm32f1xx_hal.h"
#include "ring_buffer.h"
#include <stdint.h>

/* ── Buffer sizes (must be power of 2 for & (size-1) indexing) ── */
#define UART_RX_BUF_SIZE    256
#define UART_TX_BUF_SIZE    256

/* ── UART instance count ── */
#define UART_INSTANCE_MAX   5

typedef enum {
    UART_STATE_IDLE = 0,
    UART_STATE_READY,
    UART_STATE_BUSY,
    UART_STATE_ERROR
} UartState_t;

typedef struct UartInstance UartInstance_t;

struct UartInstance {
    USART_TypeDef       *Instance;
    UART_HandleTypeDef   huart;

    /* Receive */
    RingBuffer_t         rx_rb;
    uint8_t              rx_buf[UART_RX_BUF_SIZE];
    volatile uint8_t     rx_flag;        /* IDLE interrupt → set; main loop polls & clears */

    /* Transmit */
    RingBuffer_t         tx_rb;
    uint8_t              tx_buf[UART_TX_BUF_SIZE];
    volatile uint8_t     tx_flag;        /* TC interrupt → set */

    UartState_t          state;

    /* Callbacks */
    void (*on_frame_received)(UartInstance_t *uart);
    void (*on_frame_sent)(UartInstance_t *uart);
    void (*on_error)(UartInstance_t *uart, uint32_t error_flags);
};

typedef struct {
    UART_InitTypeDef    uart_init;
    uint32_t            remap_flag;      /* 0=default pins, 1=remapped pins */
} UartConfig_t;

HAL_StatusTypeDef Uart_Init(UartInstance_t *uart, USART_TypeDef *instance,
                             const UartConfig_t *config);
void Uart_Send(UartInstance_t *uart, uint8_t *data, uint16_t len);
uint16_t Uart_Read(UartInstance_t *uart, uint8_t *buf, uint16_t len);
void Uart_PollEvents(UartInstance_t *uart);

#endif
