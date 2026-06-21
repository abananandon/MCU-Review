#include "i2c_sw.h"
#include "delay.h"

/**
 * @brief  Set SDA pin level
 * @param  pin_state  0=low, non-zero=high
 * @return none
 */
static void i2c_sda_set(uint8_t pin_state)
{
    if (pin_state) {
        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
    }
}

/**
 * @brief  Set SCL pin level
 * @param  pin_state  0=low, non-zero=high
 * @return none
 */
static void i2c_scl_set(uint8_t pin_state)
{
    if (pin_state) {
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
    }
}

/**
 * @brief  Read current SDA pin level
 * @param  none
 * @return 0=low, 1=high
 */
static uint8_t i2c_sda_read(void)
{
    return HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN);
}

/**
 * @brief  Initialize software I2C GPIO (SDA=OD, SCL=PP, both pulled-up)
 * @param  none
 * @return none
 */
void I2C_Init(void)
{
    I2C_SDA_CLK_ENABLE();
    I2C_SCL_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = I2C_SDA_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SDA_PORT, &gpio_init);

    gpio_init.Pin = I2C_SCL_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(I2C_SCL_PORT, &gpio_init);

    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Generate I2C START condition (SDA high→low while SCL high)
 * @param  none
 * @return none
 */
void I2C_Start(void)
{
    i2c_sda_set(1);
    i2c_scl_set(1);
    I2C_DELAY_US();
    i2c_sda_set(0);
    I2C_DELAY_US();
    i2c_scl_set(0);
}

/**
 * @brief  Generate I2C STOP condition (SDA low→high while SCL high)
 * @param  none
 * @return none
 */
void I2C_Stop(void)
{
    i2c_sda_set(0);
    i2c_scl_set(1);
    I2C_DELAY_US();
    i2c_sda_set(1);
    I2C_DELAY_US();
}

/**
 * @brief  Send ACK (master pulls SDA low during 9th clock)
 * @param  none
 * @return none
 */
void I2C_Ack(void)
{
    i2c_scl_set(0);
    i2c_sda_set(0);
    I2C_DELAY_US();
    i2c_scl_set(1);
    I2C_DELAY_US();
    i2c_scl_set(0);
    i2c_sda_set(1);
    I2C_DELAY_US();
}

/**
 * @brief  Send NACK (master leaves SDA high during 9th clock)
 * @param  none
 * @return none
 */
void I2C_NAck(void)
{
    i2c_scl_set(0);
    i2c_sda_set(1);
    I2C_DELAY_US();
    i2c_scl_set(1);
    I2C_DELAY_US();
    i2c_scl_set(0);
    I2C_DELAY_US();
}

/**
 * @brief  Send one byte and read slave ACK
 * @param  data  byte to send (MSB first)
 * @return 0=ACK received, 1=NACK received
 */
uint8_t I2C_SendByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        i2c_scl_set(0);
        i2c_sda_set((data << i) & 0x80);
        I2C_DELAY_US();
        i2c_scl_set(1);
        I2C_DELAY_US();
    }
    i2c_scl_set(0);
    i2c_sda_set(1);
    I2C_DELAY_US();
    i2c_scl_set(1);
    I2C_DELAY_US();
    uint8_t ack = i2c_sda_read();
    i2c_scl_set(0);

    return ack;
}

/**
 * @brief  Receive one byte from slave (MSB first)
 * @param  none
 * @return received byte
 */
uint8_t I2C_ReceiveData(void)
{
    uint8_t data = 0;
    i2c_scl_set(0);
    i2c_sda_set(1);

    for (uint8_t i = 0; i < 8 ; i++) {
        data <<= 1;
        i2c_scl_set(1);
        I2C_DELAY_US();
        if (i2c_sda_read()) {
            data |= 0x01;
        }
        i2c_scl_set(0);
        I2C_DELAY_US();
    }

    return data;
}

/**
 * @brief  Release SDA, pulse SCL and read slave ACK (standalone)
 * @param  none
 * @return 0=ACK received, 1=NACK received
 */
uint8_t I2C_WaitAck(void)
{
    i2c_scl_set(0);
    i2c_sda_set(1);
    I2C_DELAY_US();
    i2c_scl_set(1);
    I2C_DELAY_US();
    uint8_t ack = i2c_sda_read();
    i2c_scl_set(0);

    return ack;
}

/**
 * @brief  Write register/memory via I2C (addr + reg + data[])
 * @param  dev_addr  7-bit I2C device address
 * @param  reg_addr  register / memory address
 * @param  buf       data to write
 * @param  len       number of bytes
 * @return 0=success, 1=device NACK, 2=register NACK, 3=data NACK
 */
uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr,
     uint8_t *buf, uint16_t len)
{
    I2C_Start();

    if (I2C_SendByte(dev_addr << 1)) {
        I2C_Stop();
        return 1;
    }

    if (I2C_SendByte(reg_addr)) {
        I2C_Stop();
        return 2;
    }

    for (uint16_t i = 0; i < len; i++) {
        if (I2C_SendByte(buf[i])) {
            I2C_Stop();
            return 3;
        }
    }

    I2C_Stop();
    return 0;
}

/**
 * @brief  Read register/memory via I2C (addr + reg + repeated-start + data[])
 * @param  dev_addr  7-bit I2C device address
 * @param  reg_addr  register / memory address
 * @param  buf       output buffer
 * @param  len       number of bytes to read
 * @return 0=success, 1=device NACK, 2=register NACK, 3=read-addr NACK
 */
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr,
     uint8_t *buf, uint16_t len)
{
    I2C_Start();

    if (I2C_SendByte(dev_addr << 1)) {
        I2C_Stop();
        return 1;
    }

    if (I2C_SendByte(reg_addr)) {
        I2C_Stop();
        return 2;
    }

    I2C_Start();

    if (I2C_SendByte((dev_addr << 1) | 0x01)) {
        I2C_Stop();
        return 3;
    }

    for (uint16_t i = 0; i < len; i++) {
        buf[i] = I2C_ReceiveData();

        if (i == len - 1) {
            I2C_NAck();
        } else {
            I2C_Ack();
        }
    }

    I2C_Stop();
    return 0;
}
