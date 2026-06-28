#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* ── Hardware wiring ── */
#define W25Q_SPI_INSTANCE   SPI1
#define W25Q_NSS_PORT       GPIOA
#define W25Q_NSS_PIN        GPIO_PIN_2

/* ── Geometry ── */
#define W25Q64_SECTOR_SIZE    4096    /* 4 KB  — smallest erasable unit */
#define W25Q64_BLOCK32_SIZE   (32 * 1024)
#define W25Q64_BLOCK64_SIZE   (64 * 1024)
#define W25Q64_PAGE_SIZE      256     /* page-program limit */
#define W25Q64_CHIP_SIZE      (8 * 1024 * 1024)   /* 8 MB = 64 Mbit */

/* ── DMA buffer size (must cover one sector for erase-then-write) ── */
#define W25Q_DMA_BUF_SIZE     4096

/* ── Dummy byte for read (MOSI is ignored during read-out) ── */
#define W25Q_SPI_DUMMY        0xFF

/*──────────────────────────────────────────────────────
 * Commands (only the ones actually used)
 *──────────────────────────────────────────────────────*/
typedef enum {
    W25Q_CMD_WREN      = 0x06,   /* Write Enable                    */
    W25Q_CMD_WRDI      = 0x04,   /* Write Disable                   */
    W25Q_CMD_RDSR1     = 0x05,   /* Read Status Register-1          */
    W25Q_CMD_RDSR2     = 0x35,   /* Read Status Register-2          */
    W25Q_CMD_WRSR      = 0x01,   /* Write Status Register           */
    W25Q_CMD_READ      = 0x03,   /* Read Data                       */
    W25Q_CMD_PP        = 0x02,   /* Page Program  (≤ 256 B)         */
    W25Q_CMD_SE        = 0x20,   /* Sector Erase   (4 KB)           */
    W25Q_CMD_BE32      = 0x52,   /* Block Erase    (32 KB)          */
    W25Q_CMD_BE64      = 0xD8,   /* Block Erase    (64 KB)          */
    W25Q_CMD_CE        = 0xC7,   /* Chip Erase                      */
    W25Q_CMD_RDID      = 0x9F,   /* Read JEDEC ID                   */
    W25Q_CMD_RSTEN     = 0x66,   /* Enable Reset                    */
    W25Q_CMD_RST       = 0x99    /* Reset Device                    */
} W25Q_Cmd_t;

/* ── Status Register-1 bits ── */
#define W25Q_SR1_BUSY        0x01    /* 1 = write/erase in progress */
#define W25Q_SR1_WEL         0x02    /* 1 = write latch enabled     */

/* ── JEDEC ID expected values ──
 * Some clones report manufacturer 0x52 instead of Winbond 0xEF */
#define W25Q64_MANUFACTURER_EF  0xEF
#define W25Q64_MANUFACTURER_52  0x52
#define W25Q64_MANUFACTURER_ID  0xEF    /* kept for source compatibility */
#define W25Q64_CAPACITY_ID      0x17    /* 2^23 = 8 MB */
#define W25Q64_REMS_DEVICE_ID   0x16

/*──────────────────────────────────────────────────────
 * Public API
 *──────────────────────────────────────────────────────*/

/* Init — configures CS GPIO + SPI, reads JEDEC ID to verify */
HAL_StatusTypeDef W25Q_Init(void);

/* ── Control ── */
uint8_t  W25Q_ReadStatus1(void);
uint8_t  W25Q_ReadStatus2(void);
void     W25Q_WaitBusy(void);              /* poll BUSY until cleared */
void     W25Q_WriteEnable(void);
void     W25Q_WriteDisable(void);
void     W25Q_Reset(void);

/* ── Identification ── */
void     W25Q_ReadJEDECID(uint8_t *id);    /* out: 3 bytes [Mfr, Type, Cap] */

/* ── Read (polling) ── */
HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/* ── Read (DMA) — asynchronous, caller sets on_dma_done hook ── */
HAL_StatusTypeDef W25Q_Read_DMA(uint32_t addr, uint8_t *buf, uint32_t len);

/* ── Write / Erase ── */
HAL_StatusTypeDef W25Q_WritePage(uint32_t addr, uint8_t *data,
                                 uint16_t len);       /* len ≤ 256, no page crossing */
HAL_StatusTypeDef W25Q_EraseSector(uint32_t addr);    /*  4 KB — addr must be 4-KB aligned */
HAL_StatusTypeDef W25Q_EraseBlock32(uint32_t addr);   /* 32 KB — addr must be 32-KB aligned */
HAL_StatusTypeDef W25Q_EraseBlock64(uint32_t addr);   /* 64 KB — addr must be 64-KB aligned */
HAL_StatusTypeDef W25Q_EraseChip(void);               /* ~80 s — use with caution */

/* ── Read Size ── */
uint32_t W25Q_GetCapacity(void);

#endif /* __W25Q64_H */
