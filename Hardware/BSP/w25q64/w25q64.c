/**
 * @file    w25q64.c
 * @brief   W25Q64 SPI NOR Flash driver (8 MB / 64 Mbit)
 *
 * Supports: Read (polling + DMA), Page Program, Sector / Block / Chip Erase.
 *
 * Wiring:  SPI1  SCK=PA5  MISO=PA6  MOSI=PA7   CS=PA2 (software NSS)
 *          Mode 0 (CPOL=0, CPHA=0), 36 MHz SCK
 */

#include "w25q64.h"
#include "spi.h"
#include <string.h>

/*──────────────────────────────────────────────────────
 * Module statics
 *──────────────────────────────────────────────────────*/
SPIInstance_t   g_w25q_spi;                                /**< SPI instance     */
static uint8_t         g_tx_buf[W25Q_DMA_BUF_SIZE + 4];    /**< CMD+ADDR+dummy  */
static uint8_t         g_rx_buf[W25Q_DMA_BUF_SIZE + 4];    /**< garbage + data  */

/** @brief DMA callback state: caller's destination buffer */
static uint8_t        *g_dma_user_buf;
/** @brief DMA callback state: caller's expected byte count */
static uint32_t        g_dma_user_len;

/*──────────────────────────────────────────────────────
 * Hardware configuration (Mode 0, 36 MHz, software NSS)
 *──────────────────────────────────────────────────────*/
static SPIConfig_t g_w25q_spi_config = {
    .spi_init.Mode               = SPI_MODE_MASTER,
    .spi_init.Direction          = SPI_DIRECTION_2LINES,
    .spi_init.DataSize           = SPI_DATASIZE_8BIT,
    .spi_init.CLKPolarity        = SPI_POLARITY_LOW,
    .spi_init.CLKPhase           = SPI_PHASE_1EDGE,
    .spi_init.NSS                = SPI_NSS_SOFT,
    .spi_init.BaudRatePrescaler  = SPI_BAUDRATEPRESCALER_2,
    .spi_init.FirstBit           = SPI_FIRSTBIT_MSB,
    .spi_init.TIMode             = SPI_TIMODE_DISABLE,
    .spi_init.CRCCalculation     = SPI_CRCCALCULATION_DISABLE,
    .spi_init.CRCPolynomial      = 7,
    .remap_flag                  = 0
};

/*──────────────────────────────────────────────────────
 * CS helpers (private)
 *──────────────────────────────────────────────────────*/

/** @brief Pull chip-select low */
static void W25Q_CS_Low(void)
{
    HAL_GPIO_WritePin(W25Q_NSS_PORT, W25Q_NSS_PIN, GPIO_PIN_RESET);
}

/** @brief Pull chip-select high (idle) */
static void W25Q_CS_High(void)
{
    HAL_GPIO_WritePin(W25Q_NSS_PORT, W25Q_NSS_PIN, GPIO_PIN_SET);
}

/** @brief Hook wrapper: matches SPIInstance_t callback signature */
static void W25Q_CS_Low_Hook(SPIInstance_t *spi)
{
    (void)spi;
    W25Q_CS_Low();
}

/** @brief Hook wrapper: matches SPIInstance_t callback signature */
static void W25Q_CS_High_Hook(SPIInstance_t *spi)
{
    (void)spi;
    W25Q_CS_High();
}

/*──────────────────────────────────────────────────────
 * Low-level SPI helpers
 *──────────────────────────────────────────────────────*/

/** @brief Send one byte over SPI, return received byte */
static uint8_t W25Q_SendByte(uint8_t byte)
{
    return SPI_ReadWriteByte(&g_w25q_spi, byte);
}

/** @brief Send dummy byte (0xFF) to clock out one data byte */
static uint8_t W25Q_SendDummy(void)
{
    return W25Q_SendByte(W25Q_SPI_DUMMY);
}

/** @brief Send a command byte */
static void W25Q_SendCmd(W25Q_Cmd_t cmd)
{
    W25Q_SendByte((uint8_t)cmd);
}

/** @brief Send 3-byte address (MSB first: A23-A16, A15-A8, A7-A0) */
static void W25Q_SendAddr(uint32_t addr)
{
    W25Q_SendByte((uint8_t)(addr >> 16));   /* A23-A16 */
    W25Q_SendByte((uint8_t)(addr >> 8));    /* A15-A8  */
    W25Q_SendByte((uint8_t)(addr));         /* A7-A0   */
}

/*──────────────────────────────────────────────────────
 * DMA callback — copies received data to user buffer
 *──────────────────────────────────────────────────────*/

/**
 * @brief  DMA completion callback — copies payload to caller's buffer.
 *
 * rx_buf layout after a read transfer:
 *   [0..3]  = garbage (command + address overhead)
 *   [4..N]  = actual Flash data
 *
 * Only the payload region is memcpy'd to g_dma_user_buf.
 */
static void W25Q_DmaDone(SPIInstance_t *spi)
{
    (void)spi;

    if (g_dma_user_buf != NULL && g_dma_user_len > 0) {
        memcpy(g_dma_user_buf, &g_rx_buf[4], g_dma_user_len);
    }

    /* Reset tracking */
    g_dma_user_buf = NULL;
    g_dma_user_len = 0;
}

/*──────────────────────────────────────────────────────
 * Public API — Init
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Initialise the W25Q64 Flash.
 *
 * Configures CS GPIO (PA2, push-pull, idle high), initialises SPI1 in
 * Mode 0 at 36 MHz, injects CS hooks, and reads JEDEC ID as a sanity check.
 *
 * @retval HAL_OK on success, HAL_ERROR if SPI init fails or JEDEC ID mismatch
 */
HAL_StatusTypeDef W25Q_Init(void)
{
    HAL_StatusTypeDef ret;

    /* ── CS GPIO — push-pull, idle high ── */
    GPIO_InitTypeDef cs = {
        .Pin   = W25Q_NSS_PIN,
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_HIGH
    };
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_Init(W25Q_NSS_PORT, &cs);
    W25Q_CS_High();     /* ensure idle high */

    /* ── SPI init ── */
    ret = SPI_Init(&g_w25q_spi, W25Q_SPI_INSTANCE, &g_w25q_spi_config);
    if (ret != HAL_OK) return ret;

    /* ── Inject hooks ── */
    g_w25q_spi.on_transfer_start = W25Q_CS_Low_Hook;
    g_w25q_spi.on_transfer_end   = W25Q_CS_High_Hook;

    /* ── Warm-up: first SPI transfer after init may be corrupted ── */
    W25Q_CS_Low();
    SPI_ReadWriteByte(&g_w25q_spi, 0xFF);
    W25Q_CS_High();

    /* ── Verify chip is present (JEDEC ID check) ── */
    uint8_t id[3];
    W25Q_ReadJEDECID(id);

    if (id[0] != W25Q64_MANUFACTURER_EF
        && id[0] != W25Q64_MANUFACTURER_52) {
        return HAL_ERROR;   /* communication failure or wrong chip */
    }

    return HAL_OK;
}

/*──────────────────────────────────────────────────────
 * Public API — Control
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Read Status Register-1 (BUSY, WEL, BP bits).
 * @retval SR1 byte value
 */
uint8_t W25Q_ReadStatus1(void)
{
    uint8_t sr = 0;
    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_RDSR1);          /* 0x05 */
    sr = W25Q_SendDummy();
    W25Q_CS_High();
    return sr;
}

/**
 * @brief  Read Status Register-2 (QE, SRP1, CMP, LB bits).
 * @retval SR2 byte value
 */
uint8_t W25Q_ReadStatus2(void)
{
    uint8_t sr = 0;
    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_RDSR2);          /* 0x35 */
    sr = W25Q_SendDummy();
    W25Q_CS_High();
    return sr;
}

/**
 * @brief  Poll BUSY bit in SR1 until cleared or timeout (~16.7 M iterations).
 *
 * Typical wait times:
 *   - Page Program:  ~3 ms
 *   - Sector Erase:  ~45 ms
 *   - Block Erase 32K: ~400 ms
 *   - Block Erase 64K: ~1 s
 *   - Chip Erase:      ~80 s
 */
void W25Q_WaitBusy(void)
{
    uint32_t timeout = 0xFFFFFF;
    while ((W25Q_ReadStatus1() & W25Q_SR1_BUSY) && --timeout) {
        /* spin */
    }
}

/**
 * @brief  Send Write Enable (0x06) — must precede any Program / Erase command.
 */
void W25Q_WriteEnable(void)
{
    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_WREN);           /* 0x06 */
    W25Q_CS_High();
}

/**
 * @brief  Send Write Disable (0x04) — clears the WEL latch.
 */
void W25Q_WriteDisable(void)
{
    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_WRDI);           /* 0x04 */
    W25Q_CS_High();
}

/**
 * @brief  Software reset the device.
 *
 * Sequence: Enable Reset (0x66) → Reset (0x99) → wait ~30 µs.
 */
void W25Q_Reset(void)
{
    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_RSTEN);          /* 0x66 */
    W25Q_CS_High();

    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_RST);            /* 0x99 */
    W25Q_CS_High();

    W25Q_WaitBusy();                       /* ~30 µs reset time */
}

/*──────────────────────────────────────────────────────
 * Public API — Identification
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Read JEDEC Manufacturer & Device ID (0x9F).
 *
 * Returns three bytes:
 *   id[0] = Manufacturer ID (0xEF for Winbond)
 *   id[1] = Memory Type    (0x40 for W25Q64)
 *   id[2] = Capacity       (0x17 for W25Q64)
 *
 * @param  id  Output buffer (at least 3 bytes)
 */
void W25Q_ReadJEDECID(uint8_t *id)
{
    if (id == NULL) return;

    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_RDID);           /* 0x9F */
    id[0] = W25Q_SendDummy();              /* Manufacturer ID   */
    id[1] = W25Q_SendDummy();              /* Memory Type       */
    id[2] = W25Q_SendDummy();              /* Capacity          */
    W25Q_CS_High();
}

/*──────────────────────────────────────────────────────
 * Public API — Read (polling)
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Read data from Flash — polling mode.
 *
 * Sends Read Data command (0x03) + 3-byte address, then clocks out
 * len bytes using dummy bytes on MOSI.
 *
 * @param  addr  Start address (0 … 8M-1)
 * @param  buf   Destination buffer
 * @param  len   Number of bytes to read
 * @retval HAL_OK on success, HAL_ERROR on null buffer or zero length
 */
HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0) return HAL_ERROR;

    W25Q_CS_Low();
    W25Q_SendCmd(W25Q_CMD_READ);           /* 0x03 */
    W25Q_SendAddr(addr);

    for (uint32_t i = 0; i < len; i++) {
        buf[i] = W25Q_SendDummy();         /* each dummy byte clocks out 1 data byte */
    }

    W25Q_CS_High();
    return HAL_OK;
}

/*──────────────────────────────────────────────────────
 * Public API — Read (DMA, asynchronous)
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Read data from Flash using DMA — non-blocking.
 *
 * Builds a TX frame of [CMD, ADDR[23:16], ADDR[15:8], ADDR[7:0],
 * dummy × len] and launches SPI_TransmitReceive_DMA().  The full-duplex
 * nature of SPI means rx_buf receives [garbage × 4, data × len].
 *
 * When the DMA finishes, W25Q_DmaDone() copies rx_buf[4..] to the
 * caller's buffer.
 *
 * @note  Caller flow:
 *   1. (optional) Set g_w25q_spi.on_dma_done = your_callback;
 *   2. W25Q_Read_DMA(addr, buf, len);
 *   3. Wait for state == SPI_STATE_READY or use the callback.
 *
 * @param  addr  Start address
 * @param  buf   Destination buffer (must remain valid until DMA completes)
 * @param  len   Number of bytes (max W25Q_DMA_BUF_SIZE = 4096)
 * @retval HAL_OK on success, HAL_ERROR on invalid parameters
 */
HAL_StatusTypeDef W25Q_Read_DMA(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0) return HAL_ERROR;
    if (len > W25Q_DMA_BUF_SIZE) return HAL_ERROR;

    /* Store user buffer info for the DMA callback */
    g_dma_user_buf = buf;
    g_dma_user_len = len;

    /* ── Build TX buffer ──
     * [CMD, ADDR[23:16], ADDR[15:8], ADDR[7:0], DUMMY × len]    */
    g_tx_buf[0] = W25Q_CMD_READ;           /* 0x03 */
    g_tx_buf[1] = (uint8_t)(addr >> 16);
    g_tx_buf[2] = (uint8_t)(addr >> 8);
    g_tx_buf[3] = (uint8_t)(addr);
    memset(&g_tx_buf[4], W25Q_SPI_DUMMY, len);

    /* ── Set DMA completion hook ── */
    g_w25q_spi.on_dma_done = W25Q_DmaDone;

    /* ── Start DMA TxRx ──
     * Total transfer = 4 (cmd+addr) + len (data).
     * rx_buf[0..3] = garbage, rx_buf[4..] = flash payload.  */
    return SPI_TransmitReceive_DMA(&g_w25q_spi,
                                   g_tx_buf, g_rx_buf,
                                   (uint16_t)(4 + len));
}

/*──────────────────────────────────────────────────────
 * Public API — Write / Erase
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Program up to 256 bytes into one page (Page Program 0x02).
 *
 * Write Enable is sent automatically.  The caller must ensure the
 * write does NOT cross a page boundary (addr & ~0xFF must remain constant).
 *
 * @param  addr  Destination address
 * @param  data  Source data buffer
 * @param  len   Number of bytes (1 … 256)
 * @retval HAL_OK on success, HAL_ERROR on invalid parameter
 */
HAL_StatusTypeDef W25Q_WritePage(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0 || len > W25Q64_PAGE_SIZE) return HAL_ERROR;

    W25Q_WriteEnable();
    W25Q_CS_Low();

    W25Q_SendCmd(W25Q_CMD_PP);             /* 0x02 */
    W25Q_SendAddr(addr);

    for (uint16_t i = 0; i < len; i++) {
        W25Q_SendByte(data[i]);
    }

    W25Q_CS_High();
    W25Q_WaitBusy();                       /* tPP ≤ 3 ms */

    return HAL_OK;
}

/**
 * @brief  Erase a 4 KB sector (Sector Erase 0x20).
 *
 * @param  addr  Sector-aligned address (addr & 0xFFF == 0)
 * @retval HAL_OK on success, HAL_ERROR on misaligned address
 */
HAL_StatusTypeDef W25Q_EraseSector(uint32_t addr)
{
    if (addr & (W25Q64_SECTOR_SIZE - 1)) return HAL_ERROR;   /* alignment check */

    W25Q_WriteEnable();
    W25Q_CS_Low();

    W25Q_SendCmd(W25Q_CMD_SE);             /* 0x20 */
    W25Q_SendAddr(addr);

    W25Q_CS_High();
    W25Q_WaitBusy();                       /* tSE ≤ 45 ms */

    return HAL_OK;
}

/**
 * @brief  Erase a 32 KB block (Block Erase 0x52).
 *
 * @param  addr  32-KB-aligned address (addr & 0x7FFF == 0)
 * @retval HAL_OK on success, HAL_ERROR on misaligned address
 */
HAL_StatusTypeDef W25Q_EraseBlock32(uint32_t addr)
{
    if (addr & (W25Q64_BLOCK32_SIZE - 1)) return HAL_ERROR;

    W25Q_WriteEnable();
    W25Q_CS_Low();

    W25Q_SendCmd(W25Q_CMD_BE32);          /* 0x52 */
    W25Q_SendAddr(addr);

    W25Q_CS_High();
    W25Q_WaitBusy();                       /* tBE ≤ 400 ms */

    return HAL_OK;
}

/**
 * @brief  Erase a 64 KB block (Block Erase 0xD8).
 *
 * @param  addr  64-KB-aligned address (addr & 0xFFFF == 0)
 * @retval HAL_OK on success, HAL_ERROR on misaligned address
 */
HAL_StatusTypeDef W25Q_EraseBlock64(uint32_t addr)
{
    if (addr & (W25Q64_BLOCK64_SIZE - 1)) return HAL_ERROR;

    W25Q_WriteEnable();
    W25Q_CS_Low();

    W25Q_SendCmd(W25Q_CMD_BE64);          /* 0xD8 */
    W25Q_SendAddr(addr);

    W25Q_CS_High();
    W25Q_WaitBusy();                       /* tBE ≤ 1 s */

    return HAL_OK;
}

/**
 * @brief  Erase the entire chip (Chip Erase 0xC7).
 *
 * @warning Can take up to ~80 seconds.  Use only when full-chip erase
 *          is genuinely needed.
 *
 * @retval HAL_OK on success
 */
HAL_StatusTypeDef W25Q_EraseChip(void)
{
    W25Q_WriteEnable();
    W25Q_CS_Low();

    W25Q_SendCmd(W25Q_CMD_CE);            /* 0xC7 */

    W25Q_CS_High();
    W25Q_WaitBusy();                       /* tCE ≤ 80 s */

    return HAL_OK;
}

/*──────────────────────────────────────────────────────
 * Public API — Capacity
 *──────────────────────────────────────────────────────*/

/**
 * @brief  Return total capacity in bytes.
 * @retval 8 * 1024 * 1024 (8 MB)
 */
uint32_t W25Q_GetCapacity(void)
{
    return W25Q64_CHIP_SIZE;               /* 8 MB */
}
