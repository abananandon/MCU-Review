#include "24c02.h"
#include <stdint.h>

/**
 * @brief  Initialize 24C02 (delegates to I2C bus init)
 * @param  none
 * @return none
 */
void AT24C02_Init(void)
{
    I2C_Init();
}

/**
 * @brief  Probe device presence via I2C ACK (non-destructive, no data written)
 * @param  dev_addr  7-bit I2C device address (e.g. 0x50 for A2=A1=A0=0)
 * @return 0=device present, 1=no ACK
 */
uint8_t AT24C02_Check(uint8_t dev_addr)
{
    uint8_t ack;

    I2C_Start();
    ack = I2C_SendByte(dev_addr << 1);
    I2C_Stop();

    return ack ? 1 : 0;
}

/**
 * @brief  Write data to 24C02 with page-boundary splitting and write-cycle polling
 * @param  dev_addr  7-bit I2C device address
 * @param  mem_addr  EEPROM start address (0–255 for 24C02)
 * @param  pbuf      pointer to write data
 * @param  datalen   number of bytes to write
 * @return 0=success, 1=I2C NACK, 2=write-cycle timeout
 */
uint8_t AT24C02_Write(uint8_t dev_addr, uint8_t mem_addr,
                       uint8_t *pbuf, uint16_t datalen)
{
    uint16_t written = 0;
    uint16_t chunk;
    uint16_t page_remain;
    uint32_t timeout;

    if (pbuf == NULL || datalen == 0) {
        return 0;
    }

    while (written < datalen) {
        page_remain = AT24C02_PAGE_SIZE - (mem_addr & (AT24C02_PAGE_SIZE - 1));
        chunk = datalen - written;
        if (chunk > page_remain) {
            chunk = page_remain;
        }

        if (I2C_WriteReg(dev_addr, mem_addr, pbuf + written, chunk)) {
            return 1;
        }

        timeout = AT24C02_TWR_MAX;
        while (timeout) {
            I2C_Start();
            if (!I2C_SendByte(dev_addr << 1)) {
                I2C_Stop();
                break;
            }
            I2C_Stop();
            delay_us(1);
            timeout--;
        }

        if (timeout == 0) {
            return 2;
        }

        written  += chunk;
        mem_addr += chunk;
    }

    return 0;
}

/**
 * @brief  Read sequential data from 24C02
 * @param  dev_addr  7-bit I2C device address
 * @param  mem_addr  EEPROM start address (0–255 for 24C02)
 * @param  pbuf      output buffer
 * @param  datalen   number of bytes to read
 * @return 0=success, non-zero=I2C NACK
 */
uint8_t AT24C02_Read(uint8_t dev_addr, uint8_t mem_addr,
                      uint8_t *pbuf, uint16_t datalen)
{
    return I2C_ReadReg(dev_addr, mem_addr, pbuf, datalen);
}
