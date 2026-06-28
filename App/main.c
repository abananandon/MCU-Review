#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "led.h"
#include "delay.h"
#include "sys.h"
#include "retarget.h"
#include "24c02.h"
#include "w25q64.h"
#include "spi.h"

/* DMA poll: test code needs visibility of SPI state */
extern SPIInstance_t g_w25q_spi;

static void w25q64_test(void)
{
    uint8_t id[3];
    uint8_t wr_buf[256], rd_buf[256];
    uint32_t fail = 0;

    printf("\r\n===== W25Q64 SPI Flash Test =====\r\n");

    /* ── 1. Init ── */
    if (W25Q_Init() != HAL_OK) {
        printf("W25Q64 Init FAIL — check wiring & SPI mode\r\n");
        return;
    }
    printf("W25Q64 Init OK\r\n");

    /* ── 2. Read JEDEC ID ── */
    W25Q_ReadJEDECID(id);
    printf("JEDEC ID: MF=0x%02X  Type=0x%02X  Cap=0x%02X",
           id[0], id[1], id[2]);
    if (id[0] == W25Q64_MANUFACTURER_ID) {
        printf("  (W25Q64 recognized)\r\n");
    } else {
        printf("  UNKNOWN CHIP!\r\n");
    }

    /* ── 3. Status register ── */
    uint8_t sr1 = W25Q_ReadStatus1();
    printf("Status1: 0x%02X (BUSY=%d WEL=%d)\r\n",
           sr1, !!(sr1 & W25Q_SR1_BUSY), !!(sr1 & W25Q_SR1_WEL));

    /* ── 4. Erase sector 0 ── */
    printf("Erasing sector 0 (4 KB)... ");
    if (W25Q_EraseSector(0) != HAL_OK) {
        printf("FAIL\r\n");
        return;
    }
    printf("OK\r\n");

    /* ── 5. Verify erased → all 0xFF ── */
    printf("Verify erased... ");
    W25Q_Read(0, rd_buf, sizeof(rd_buf));
    for (uint16_t i = 0; i < sizeof(rd_buf); i++) {
        if (rd_buf[i] != 0xFF) { fail++; break; }
    }
    printf("%s\r\n", fail ? "FAIL" : "OK");
    fail = 0;

    /* ── 6. Prepare test pattern ── */
    for (uint16_t i = 0; i < sizeof(wr_buf); i++) {
        wr_buf[i] = (uint8_t)i;   /* 0x00, 0x01, ..., 0xFF */
    }

    /* ── 7. Write page 0 ── */
    printf("Writing page 0 (256 B)... ");
    if (W25Q_WritePage(0, wr_buf, sizeof(wr_buf)) != HAL_OK) {
        printf("FAIL\r\n");
        return;
    }
    printf("OK\r\n");

    /* ── 8. Read back & verify ── */
    printf("Read back verify... ");
    memset(rd_buf, 0, sizeof(rd_buf));
    W25Q_Read(0, rd_buf, sizeof(rd_buf));
    for (uint16_t i = 0; i < sizeof(wr_buf); i++) {
        if (rd_buf[i] != wr_buf[i]) {
            if (fail < 5) {
                printf("  [%u] w=0x%02X r=0x%02X\r\n", i, wr_buf[i], rd_buf[i]);
            }
            fail++;
        }
    }
    printf("%lu errors\r\n", (unsigned long)fail);

    /* ── 9. DMA read test ── */
    fail = 0;
    memset(rd_buf, 0, sizeof(rd_buf));

    printf("DMA read (256 B)... ");
    if (W25Q_Read_DMA(0, rd_buf, sizeof(rd_buf)) != HAL_OK) {
        printf("FAIL (launch)\r\n");
    } else {
        /* Wait for DMA completion — state returns to READY */
        uint32_t timeout = 0xFFFFFF;
        while (g_w25q_spi.state == SPI_STATE_BUSY && --timeout) {}
        if (timeout == 0) {
            printf("FAIL (timeout)\r\n");
        } else {
            for (uint16_t i = 0; i < sizeof(wr_buf); i++) {
                if (rd_buf[i] != wr_buf[i]) fail++;
            }
            printf("%lu errors\r\n", (unsigned long)fail);
        }
    }

    printf("===== W25Q64 Test Complete =====\r\n");
}

static void test_single_byte(void)
{
    uint8_t wdata, rdata;
    uint8_t fail = 0;

    printf("\r\n--- Single Byte Test ---\r\n");
    for (wdata = 0; wdata < 16; wdata++) {
        AT24C02_Write(AT24C02_ADDR, 0x10, &wdata, 1);
        AT24C02_Read(AT24C02_ADDR, 0x10, &rdata, 1);

        if (wdata == rdata) {
            printf("  0x%02X OK\r\n", wdata);
        } else {
            printf("  0x%02X FAIL (read 0x%02X)\r\n", wdata, rdata);
            fail++;
        }
    }
    printf("%lu errors\r\n", (unsigned long)fail);
}

static void test_multi_byte(void)
{
    uint8_t wbuf[32], rbuf[32];
    uint16_t fail = 0;
    uint8_t i;

    for (i = 0; i < sizeof(wbuf); i++) {
        wbuf[i] = 0xA0 + i;
    }

    printf("\r\n--- Multi-Byte Test (32 B) ---\r\n");
    AT24C02_Write(AT24C02_ADDR, 0x20, wbuf, sizeof(wbuf));
    memset(rbuf, 0, sizeof(rbuf));
    AT24C02_Read(AT24C02_ADDR, 0x20, rbuf, sizeof(rbuf));

    for (i = 0; i < sizeof(wbuf); i++) {
        if (wbuf[i] != rbuf[i]) {
            printf("  0x%02X: write=0x%02X read=0x%02X FAIL\r\n",
                   0x20 + i, wbuf[i], rbuf[i]);
            fail++;
        }
    }

    if (fail == 0) {
        printf("  all 32 bytes OK\r\n");
    } else {
        printf("  %u errors\r\n", fail);
    }
}

static void test_rolling(void)
{
    static uint8_t seq;
    uint8_t wdata, rdata;

    wdata = seq++;
    AT24C02_Write(AT24C02_ADDR, 0x00, &wdata, 1);
    AT24C02_Read(AT24C02_ADDR, 0x00, &rdata, 1);
    printf("addr=0x00 write=0x%02X read=0x%02X %s\r\n",
           wdata, rdata, (wdata == rdata) ? "OK" : "FAIL");
}

int main(void)
{
    HAL_Init();
    stm32_system_clock_init(RCC_PLL_MUL9);
    delay_init(SystemCoreClock);
    Led_Hardware_Init();
    printf_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("24C02 I2C Test\r\n");

    /* ── W25Q64 SPI Flash test ── */
    w25q64_test();

    I2C_Init();

    printf("Scanning...\r\n");
    for (uint8_t addr = 0; addr < 0x80; addr++) {
        I2C_Start();
        if (!I2C_SendByte(addr << 1)) {
            printf("  device at 0x%02X\r\n", addr);
        }
        I2C_Stop();
    }

    if (AT24C02_Check(AT24C02_ADDR)) {
        printf("24C02 not found!\r\n");
        while (1) { Led_Toggle(0); delay_ms(200); }
        return 1;
    }

    printf("24C02 OK\r\n");

    test_single_byte();
    test_multi_byte();

    while (1) {
        test_rolling();
        Led_Toggle(0);
        delay_ms(500);
    }

    return 0;
}
