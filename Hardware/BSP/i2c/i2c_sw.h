#ifndef __I2C_H
#define __I2C_H

#include "stm32f1xx_hal.h"
#include "delay.h"

#define I2C_SDA_PORT    GPIOC
#define I2C_SDA_PIN     GPIO_PIN_11
#define I2C_SDA_CLK_ENABLE()    do {__HAL_RCC_GPIOC_CLK_ENABLE();} while(0)

#define I2C_SCL_PORT    GPIOC
#define I2C_SCL_PIN     GPIO_PIN_12
#define I2C_SCL_CLK_ENABLE()    do {__HAL_RCC_GPIOC_CLK_ENABLE();} while(0)

#define I2C_DELAY_US()     delay_us(2)

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Ack(void);
void I2C_NAck(void);
uint8_t I2C_SendByte(uint8_t data);
uint8_t I2C_ReceiveData(void);
uint8_t I2C_WaitAck(void);

uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr,
     uint8_t *buf, uint16_t len);
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr,
     uint8_t *buf, uint16_t len);

#endif