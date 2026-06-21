#ifndef __24C02_H
#define __24C02_H

#include "i2c_sw.h"
#include <stdint.h>

#define AT24C02_SIZE        256
#define AT24C02_PAGE_SIZE   8
#define AT24C02_ADDR        0x50
#define AT24C02_TWR_MAX     5000    /* max write cycle (us) */

void AT24C02_Init(void);
uint8_t AT24C02_Check(uint8_t dev_addr);
uint8_t AT24C02_Write(uint8_t dev_addr, uint8_t mem_addr,
    uint8_t *pbuf, uint16_t datalen);
uint8_t AT24C02_Read(uint8_t dev_addr, uint8_t mem_addr,
    uint8_t *pbuf, uint16_t datalen);

#endif