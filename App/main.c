#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "led.h"
#include "delay.h"
#include "sys.h"
#include "retarget.h"
#include "24c02.h"

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
    printf("%u errors\r\n", fail);
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
