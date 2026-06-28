#ifndef __SPI_H
#define __SPI_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define SPI_INSTANCE_MAX    3

/* ── State ── */
typedef enum
{
    SPI_STATE_IDLE = 0,   /* not initialised */
    SPI_STATE_READY,      /* ready for transfer */
    SPI_STATE_BUSY,       /* DMA in progress */
    SPI_STATE_ERROR       /* hardware error, needs reset */
} SPIState_t;

/* ── Configuration (caller fills in HAL init struct) ── */
typedef struct {
    SPI_InitTypeDef  spi_init;        /* Mode, CPOL/CPHA, NSS, BaudRate, … */
    uint8_t          remap_flag;      /* 0 = default pins, 1 = remapped */
} SPIConfig_t;

/* ── Forward declaration ── */
typedef struct SPIInstance SPIInstance_t;

/* ── SPI peripheral instance ── */
struct SPIInstance {
    SPI_TypeDef         *Instance;    /* SPI1 / SPI2 / SPI3 */
    SPI_HandleTypeDef    hspi;        /* HAL handle */
    SPIState_t           state;       /* IDLE / READY / BUSY / ERROR */

    /* DMA handles — configured once at init */
    DMA_HandleTypeDef    hdma_tx;
    DMA_HandleTypeDef    hdma_rx;

    /* ── Hooks (injected by upper layer, e.g. Flash driver) ── */
    void (*on_transfer_start)(SPIInstance_t *spi);  /* called before DMA starts → CS low */
    void (*on_transfer_end)(SPIInstance_t *spi);    /* called after DMA finishes → CS high */
    void (*on_dma_done)(SPIInstance_t *spi);         /* DMA complete → caller consumes rx_data */
    void (*on_error)(SPIInstance_t *spi, uint32_t error_flags);
};

/*──────────────────────────────────────────────────────
 * Public API
 *──────────────────────────────────────────────────────*/

/* Init */
HAL_StatusTypeDef SPI_Init(SPIInstance_t        *spi,
                           SPI_TypeDef          *instance,
                           const SPIConfig_t    *config);

/* Speed */
HAL_StatusTypeDef SPI_SetSpeed(SPIInstance_t *spi, uint32_t prescaler);

/* ── Path 1: polling (commands, small data) ── */

/* TxRx single byte */
uint8_t SPI_ReadWriteByte(SPIInstance_t *spi, uint8_t tx_data);

/* Tx only (discard received byte), e.g. Write Enable 0x06 */
HAL_StatusTypeDef SPI_Transmit(SPIInstance_t *spi, uint8_t *data, uint16_t size);

/* TxRx batch — synchronous, returns after all bytes transferred */
HAL_StatusTypeDef SPI_TransmitReceive(SPIInstance_t *spi,
                                      uint8_t *tx_data, uint8_t *rx_data,
                                      uint16_t size);

/* ── Path 2: DMA (bulk data — page / sector) ── */

/* Start asynchronous TxRx DMA transfer, returns immediately.
 * Caller must NOT touch tx_data / rx_data until on_dma_done fires. */
HAL_StatusTypeDef SPI_TransmitReceive_DMA(SPIInstance_t *spi,
                                          uint8_t *tx_data, uint8_t *rx_data,
                                          uint16_t size);

#endif /* __SPI_H */
