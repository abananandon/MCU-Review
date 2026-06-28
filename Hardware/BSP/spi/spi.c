/**
 * @file    spi.c
 * @brief   STM32F103 SPI driver — polling + DMA, hook-based CS control
 */

#include "spi.h"
#include <string.h>
#include <stddef.h>

/*──────────────────────────────────────────────────────
 * Pin map — default row [0] / remapped row [1]
 *──────────────────────────────────────────────────────*/
typedef struct {
    GPIO_TypeDef   *miso_port;
    uint32_t        miso_pin;
    GPIO_TypeDef   *mosi_port;
    uint32_t        mosi_pin;
    GPIO_TypeDef   *clk_port;
    uint32_t        clk_pin;
    GPIO_TypeDef   *nss_port;
    uint32_t        nss_pin;
    uint8_t         remap_flag;
} SPIPinMap_t;

static const SPIPinMap_t g_spi_pin_map[SPI_INSTANCE_MAX][2] = {
    /* SPI1 */ {
        {GPIOA, GPIO_PIN_6,  GPIOA, GPIO_PIN_7,  GPIOA, GPIO_PIN_5,  GPIOA, GPIO_PIN_4,  0},
        {GPIOB, GPIO_PIN_4,  GPIOB, GPIO_PIN_5,  GPIOB, GPIO_PIN_3,  GPIOA, GPIO_PIN_15, 1}
    },
    /* SPI2 */ {
        {GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_15, GPIOB, GPIO_PIN_13, GPIOB, GPIO_PIN_12, 0},
        {0}
    },
    /* SPI3 */ {
        {GPIOB, GPIO_PIN_4,  GPIOB, GPIO_PIN_5,  GPIOB, GPIO_PIN_3,  GPIOA, GPIO_PIN_15, 0},
        {0}
    }
};

/** @brief Global instance table — ISR dispatches via g_spi_table[idx] */
static SPIInstance_t *g_spi_table[SPI_INSTANCE_MAX];

/**
 * @brief  Map SPI_TypeDef pointer to zero-based index
 * @param  instance  SPI1 / SPI2 / SPI3
 * @retval 0..2 on success, -1 on NULL or unknown
 */
static int spi_get_index(SPI_TypeDef *instance)
{
    if (instance == NULL) return -1;
    if (instance == SPI1) return 0;
    if (instance == SPI2) return 1;
    if (instance == SPI3) return 2;
    return -1;
}

/**
 * @brief  Get NVIC IRQ number for a given SPI peripheral
 * @param  instance  SPI1 / SPI2 / SPI3
 * @retval IRQn_Type on success, (IRQn_Type)-1 on NULL / unknown
 */
static IRQn_Type spi_get_irq(SPI_TypeDef *instance)
{
    if (instance == NULL) return (IRQn_Type)-1;
    if (instance == SPI1) return SPI1_IRQn;
    if (instance == SPI2) return SPI2_IRQn;
    if (instance == SPI3) return SPI3_IRQn;
    return (IRQn_Type)-1;
}

/*──────────────────────────────────────────────────────
 * DMA channel assignment (chosen once at init)
 *──────────────────────────────────────────────────────*/
typedef struct {
    DMA_Channel_TypeDef *rx_channel;   /**< e.g. DMA1_Channel2 */
    DMA_Channel_TypeDef *tx_channel;   /**< e.g. DMA1_Channel3 */
    IRQn_Type            rx_irq;
    IRQn_Type            tx_irq;
    uint32_t             bus_clock;    /**< 0 = DMA1, 1 = DMA2 */
} SPIDmaChannel_t;

static const SPIDmaChannel_t g_spi_dma[SPI_INSTANCE_MAX] = {
    /* SPI1 */ {
        DMA1_Channel2, DMA1_Channel3,
        DMA1_Channel2_IRQn, DMA1_Channel3_IRQn,
        0  /* DMA1 */
    },
    /* SPI2 */ {
        DMA1_Channel4, DMA1_Channel5,
        DMA1_Channel4_IRQn, DMA1_Channel5_IRQn,
        0  /* DMA1 */
    },
    /* SPI3 */ {
        DMA2_Channel1, DMA2_Channel2,
        DMA2_Channel1_IRQn, DMA2_Channel2_IRQn,
        1  /* DMA2 */
    }
};

/**
 * @brief  Low-level hardware init: GPIO, clocks, AFIO remap, NVIC, DMA handles
 * @param  spi         Pointer to SPI instance
 * @param  remap_flag  0 = default pins, 1 = remapped pins
 * @retval HAL_OK on success, HAL_ERROR otherwise
 */
static HAL_StatusTypeDef spi_hardware_init(SPIInstance_t *spi, uint8_t remap_flag)
{
    if (spi == NULL) return HAL_ERROR;

    GPIO_InitTypeDef gpio = {0};
    const SPIPinMap_t *pin;
    int idx = spi_get_index(spi->Instance);
    if (idx < 0) return HAL_ERROR;

    pin = &g_spi_pin_map[idx][remap_flag ? 1 : 0];
    if (pin->miso_port == NULL) return HAL_ERROR;   /* invalid remap */

    /* ── SPI peripheral clock ── */
    if      (spi->Instance == SPI1) __HAL_RCC_SPI1_CLK_ENABLE();
    else if (spi->Instance == SPI2) __HAL_RCC_SPI2_CLK_ENABLE();
    else if (spi->Instance == SPI3) __HAL_RCC_SPI3_CLK_ENABLE();

    /* ── AFIO remap (SPI1 only) ── */
    if (remap_flag) {
        __HAL_RCC_AFIO_CLK_ENABLE();
        if (spi->Instance == SPI1) __HAL_AFIO_REMAP_SPI1_ENABLE();
    }

    /* ── GPIO clocks ── */
    if (pin->miso_port == GPIOA || pin->mosi_port == GPIOA ||
        pin->clk_port  == GPIOA || pin->nss_port  == GPIOA)
        __HAL_RCC_GPIOA_CLK_ENABLE();
    if (pin->miso_port == GPIOB || pin->mosi_port == GPIOB ||
        pin->clk_port  == GPIOB || pin->nss_port  == GPIOB)
        __HAL_RCC_GPIOB_CLK_ENABLE();
    if (pin->miso_port == GPIOC || pin->mosi_port == GPIOC ||
        pin->clk_port  == GPIOC || pin->nss_port  == GPIOC)
        __HAL_RCC_GPIOC_CLK_ENABLE();
    if (pin->miso_port == GPIOD || pin->mosi_port == GPIOD ||
        pin->clk_port  == GPIOD || pin->nss_port  == GPIOD)
        __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── SCK ── */
    gpio.Pin   = pin->clk_pin;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(pin->clk_port, &gpio);

    /* ── NSS — software: GPIO output; hardware: AF push-pull ── */
    gpio.Pin = pin->nss_pin;
    if (spi->hspi.Init.NSS == SPI_NSS_SOFT) {
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
    } else {
        gpio.Mode = GPIO_MODE_AF_PP;
    }
    HAL_GPIO_Init(pin->nss_port, &gpio);

    /* ── MISO — master: AF input, slave: AF push-pull ── */
    gpio.Pin = pin->miso_pin;
    if (spi->hspi.Init.Mode == SPI_MODE_MASTER) {
        gpio.Mode = GPIO_MODE_AF_INPUT;
    } else {
        gpio.Mode = GPIO_MODE_AF_PP;
    }
    HAL_GPIO_Init(pin->miso_port, &gpio);

    /* ── MOSI — master: AF push-pull, slave: AF input ── */
    gpio.Pin = pin->mosi_pin;
    if (spi->hspi.Init.Mode == SPI_MODE_MASTER) {
        gpio.Mode = GPIO_MODE_AF_PP;
    } else {
        gpio.Mode = GPIO_MODE_AF_INPUT;
    }
    HAL_GPIO_Init(pin->mosi_port, &gpio);

    /* ── NVIC: SPI peripheral interrupts (error / flags) ── */
    HAL_NVIC_SetPriority(spi_get_irq(spi->Instance), 1, 0);
    HAL_NVIC_EnableIRQ(spi_get_irq(spi->Instance));

    /* ── DMA clock ── */
    const SPIDmaChannel_t *dma = &g_spi_dma[idx];
    if (dma->bus_clock == 0) {
        __HAL_RCC_DMA1_CLK_ENABLE();
    } else {
        __HAL_RCC_DMA2_CLK_ENABLE();
    }

    /* ── DMA NVIC ── */
    HAL_NVIC_SetPriority(dma->rx_irq, 1, 1);
    HAL_NVIC_SetPriority(dma->tx_irq, 1, 1);
    HAL_NVIC_EnableIRQ(dma->rx_irq);
    HAL_NVIC_EnableIRQ(dma->tx_irq);

    /* ── DMA handles — static config, buffer pointers set at transfer time ── */
    spi->hdma_tx.Instance                 = dma->tx_channel;
    spi->hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    spi->hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    spi->hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;
    spi->hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi->hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    spi->hdma_tx.Init.Mode                = DMA_NORMAL;
    spi->hdma_tx.Init.Priority            = DMA_PRIORITY_HIGH;

    spi->hdma_rx.Instance                 = dma->rx_channel;
    spi->hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    spi->hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    spi->hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
    spi->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi->hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    spi->hdma_rx.Init.Mode                = DMA_NORMAL;
    spi->hdma_rx.Init.Priority            = DMA_PRIORITY_HIGH;

    HAL_DMA_Init(&spi->hdma_tx);
    HAL_DMA_Init(&spi->hdma_rx);

    /* ── Link DMA handles to SPI handle ── */
    __HAL_LINKDMA(&spi->hspi, hdmatx, spi->hdma_tx);
    __HAL_LINKDMA(&spi->hspi, hdmarx, spi->hdma_rx);

    /* ── Enable SPI peripheral ── */
    __HAL_SPI_ENABLE(&spi->hspi);

    return HAL_OK;
}

/*──────────────────────────────────────────────────────
 * Public API — Init & Configuration
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Initialize an SPI peripheral instance.
 *
 * Configures GPIO pins, clocks, AFIO remap (SPI1 only), NVIC priorities,
 * DMA channels, and HAL SPI settings.  Registers the instance for ISR
 * dispatch via g_spi_table.
 *
 * @param  spi       Pointer to caller-allocated SPIInstance_t
 * @param  instance  SPI1 / SPI2 / SPI3
 * @param  config    HAL Init struct + remap flag
 * @retval HAL_OK on success, HAL_ERROR on null parameter or hardware failure
 */
HAL_StatusTypeDef SPI_Init(SPIInstance_t          *spi,
                           SPI_TypeDef            *instance,
                           const SPIConfig_t      *config)
{
    HAL_StatusTypeDef ret;
    int idx;

    if (spi == NULL || instance == NULL || config == NULL) return HAL_ERROR;

    idx = spi_get_index(instance);
    if (idx < 0) return HAL_ERROR;

    memset(spi, 0, sizeof(*spi));

    /* ── Bind hardware ── */
    spi->Instance       = instance;
    spi->hspi.Instance  = instance;
    spi->hspi.Init      = config->spi_init;
    spi->state          = SPI_STATE_IDLE;

    /* ── GPIO + clocks + remap + DMA + NVIC ── */
    ret = spi_hardware_init(spi, config->remap_flag);
    if (ret != HAL_OK) return ret;

    /* ── HAL configures SPI registers (BR, CPOL/CPHA, CRC, …) ── */
    ret = HAL_SPI_Init(&spi->hspi);
    if (ret != HAL_OK) return ret;

    /* ── Register instance ── */
    g_spi_table[idx] = spi;

    spi->state = SPI_STATE_READY;
    return HAL_OK;
}

/**
 * @brief  Change SPI baud-rate prescaler at runtime.
 *
 * The SPI peripheral is briefly disabled while CR1 BR[2:0] bits are updated.
 *
 * @param  spi        Pointer to SPI instance
 * @param  prescaler  HAL prescaler value, e.g. SPI_BAUDRATEPRESCALER_2
 * @retval HAL_OK on success, HAL_ERROR if spi is NULL
 */
HAL_StatusTypeDef SPI_SetSpeed(SPIInstance_t *spi, uint32_t prescaler)
{
    if (spi == NULL) return HAL_ERROR;

    assert_param(IS_SPI_BAUDRATE_PRESCALER(prescaler));

    __HAL_SPI_DISABLE(&spi->hspi);
    /* HAL prescaler values already hold BR[2:0] at bits 5:3 */
    spi->hspi.Instance->CR1 = (spi->hspi.Instance->CR1 & ~(0x07U << 3U))
                            | (prescaler & (0x07U << 3U));
    __HAL_SPI_ENABLE(&spi->hspi);

    return HAL_OK;
}

/*──────────────────────────────────────────────────────
 * Public API — Path 1: polling (synchronous)
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Transmit one byte and simultaneously receive one byte (polling).
 *
 * Blocks until the byte exchange completes.  Sets state to BUSY during
 * the transfer and restores it to READY on return.
 *
 * @param  spi      Pointer to SPI instance
 * @param  tx_data  Byte to send
 * @retval Received byte on success, 0xFF if spi is NULL or not READY
 */
uint8_t SPI_ReadWriteByte(SPIInstance_t *spi, uint8_t tx_data)
{
    if (spi == NULL || spi->state != SPI_STATE_READY) return 0xFF;

    spi->state = SPI_STATE_BUSY;

    uint8_t rx_data = 0;
    HAL_SPI_TransmitReceive(&spi->hspi, &tx_data, &rx_data, 1, 100);

    spi->state = SPI_STATE_READY;
    return rx_data;
}

/**
 * @brief  Transmit-only (discard received bytes) — polling.
 *
 * Typical use: Write Enable (0x06) where the response is irrelevant.
 *
 * @param  spi   Pointer to SPI instance
 * @param  data  Transmit buffer
 * @param  size  Number of bytes
 * @retval HAL_OK on success, HAL_ERROR on null pointer or not READY
 */
HAL_StatusTypeDef SPI_Transmit(SPIInstance_t *spi, uint8_t *data, uint16_t size)
{
    if (spi == NULL || data == NULL || spi->state != SPI_STATE_READY)
        return HAL_ERROR;

    spi->state = SPI_STATE_BUSY;

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(&spi->hspi, data, size, 100);

    spi->state = SPI_STATE_READY;
    return ret;
}

/**
 * @brief  Transmit and receive a block of bytes — polling.
 *
 * Data is exchanged in-place: tx_data[i] is sent while rx_data[i] is received.
 *
 * @param  spi      Pointer to SPI instance
 * @param  tx_data  Transmit buffer
 * @param  rx_data  Receive buffer
 * @param  size     Number of bytes
 * @retval HAL_OK on success, HAL_ERROR on null pointer or not READY
 */
HAL_StatusTypeDef SPI_TransmitReceive(SPIInstance_t *spi,
                                      uint8_t *tx_data, uint8_t *rx_data,
                                      uint16_t size)
{
    if (spi == NULL || tx_data == NULL || rx_data == NULL
                     || spi->state != SPI_STATE_READY)
        return HAL_ERROR;

    spi->state = SPI_STATE_BUSY;

    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&spi->hspi,
                              tx_data, rx_data, size, 500);

    spi->state = SPI_STATE_READY;
    return ret;
}

/*──────────────────────────────────────────────────────
 * Public API — Path 2: DMA (asynchronous)
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Start an asynchronous SPI TxRx transfer using DMA.
 *
 * Returns immediately after starting the DMA engine.  The caller must
 * NOT modify tx_data / rx_data until the transfer completes.
 *
 * Completion flow:
 *   1. on_transfer_start() hook is called   → upper layer pulls CS low
 *   2. DMA transfer runs in background
 *   3. HAL_SPI_TxRxCpltCallback() fires     → calls on_transfer_end() + on_dma_done()
 *
 * @param  spi      Pointer to SPI instance
 * @param  tx_data  Transmit buffer (must remain valid until DMA completes)
 * @param  rx_data  Receive buffer  (must remain valid until DMA completes)
 * @param  size     Number of bytes to exchange
 * @retval HAL_OK on success, HAL_ERROR on null pointer, not READY, or
 *         DMA start failure (in which case on_transfer_end is called internally)
 */
HAL_StatusTypeDef SPI_TransmitReceive_DMA(SPIInstance_t *spi,
                                          uint8_t *tx_data, uint8_t *rx_data,
                                          uint16_t size)
{
    if (spi == NULL || tx_data == NULL || rx_data == NULL
                     || spi->state != SPI_STATE_READY)
        return HAL_ERROR;

    spi->state = SPI_STATE_BUSY;

    /* ── Call hook: upper layer pulls CS low ── */
    if (spi->on_transfer_start) {
        spi->on_transfer_start(spi);
    }

    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(&spi->hspi,
                              tx_data, rx_data, size);

    if (ret != HAL_OK) {
        /* DMA start failed → pull CS high, restore state */
        if (spi->on_transfer_end) spi->on_transfer_end(spi);
        spi->state = SPI_STATE_READY;
    }

    return ret;
}

/*──────────────────────────────────────────────────────
 * HAL overrides — DMA completion callbacks
 *──────────────────────────────────────────────────────*/

/**
 * @brief  HAL SPI TxRx DMA transfer complete callback (weak override).
 *
 * Called from DMA IRQ context when both TX and RX DMA streams have
 * finished.  Restores CS via on_transfer_end() hook and notifies the
 * upper layer via on_dma_done().
 *
 * @param  hspi  HAL SPI handle (must be embedded in an SPIInstance_t)
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    /* Reverse-lookup: hspi is embedded in SPIInstance */
    SPIInstance_t *spi = (SPIInstance_t *)((char *)hspi
                         - offsetof(SPIInstance_t, hspi));

    /* ── Call hook: upper layer pulls CS high ── */
    if (spi->on_transfer_end) {
        spi->on_transfer_end(spi);
    }

    spi->state = SPI_STATE_READY;

    /* ── Call hook: notify upper layer that data is ready ── */
    if (spi->on_dma_done) {
        spi->on_dma_done(spi);
    }
}

/**
 * @brief  HAL SPI error callback (weak override).
 *
 * Sets state to ERROR, calls on_transfer_end (CS high) and on_error hooks.
 *
 * @param  hspi  HAL SPI handle
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    SPIInstance_t *spi = (SPIInstance_t *)((char *)hspi
                         - offsetof(SPIInstance_t, hspi));

    uint32_t err = HAL_SPI_GetError(hspi);

    spi->state = SPI_STATE_ERROR;

    if (spi->on_transfer_end) spi->on_transfer_end(spi);
    if (spi->on_error)        spi->on_error(spi, err);
}

/*──────────────────────────────────────────────────────
 * ISR entries — SPI peripheral interrupts
 *──────────────────────────────────────────────────────*/

void SPI1_IRQHandler(void)
{
    if (g_spi_table[0])
        HAL_SPI_IRQHandler(&g_spi_table[0]->hspi);
}

void SPI2_IRQHandler(void)
{
    if (g_spi_table[1])
        HAL_SPI_IRQHandler(&g_spi_table[1]->hspi);
}

void SPI3_IRQHandler(void)
{
    if (g_spi_table[2])
        HAL_SPI_IRQHandler(&g_spi_table[2]->hspi);
}

/*──────────────────────────────────────────────────────
 * ISR entries — DMA channels
 *──────────────────────────────────────────────────────*/

void DMA1_Channel2_IRQHandler(void)         /**< SPI1_RX */
{
    if (g_spi_table[0])
        HAL_DMA_IRQHandler(&g_spi_table[0]->hdma_rx);
}

void DMA1_Channel3_IRQHandler(void)         /**< SPI1_TX */
{
    if (g_spi_table[0])
        HAL_DMA_IRQHandler(&g_spi_table[0]->hdma_tx);
}

void DMA1_Channel4_IRQHandler(void)         /**< SPI2_RX */
{
    if (g_spi_table[1])
        HAL_DMA_IRQHandler(&g_spi_table[1]->hdma_rx);
}

void DMA1_Channel5_IRQHandler(void)         /**< SPI2_TX */
{
    if (g_spi_table[1])
        HAL_DMA_IRQHandler(&g_spi_table[1]->hdma_tx);
}

void DMA2_Channel1_IRQHandler(void)         /**< SPI3_RX */
{
    if (g_spi_table[2])
        HAL_DMA_IRQHandler(&g_spi_table[2]->hdma_rx);
}

void DMA2_Channel2_IRQHandler(void)         /**< SPI3_TX */
{
    if (g_spi_table[2])
        HAL_DMA_IRQHandler(&g_spi_table[2]->hdma_tx);
}
