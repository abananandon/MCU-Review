#include "i2c_sw.h"
#include "delay.h"

static void i2c_sda_set(uint8_t pin_state)
{
    if (pin_state) {
        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
    }
}

static void i2c_scl_set(uint8_t pin_state)
{
    if (pin_state) {
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
    }
}

static uint8_t i2c_sda_read(void)
{
    return HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN);
}

void I2C_Init(void)
{
    I2C_SDA_CLK_ENABLE();
    I2C_SCL_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = I2C_SDA_PIN | I2C_SCL_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SDA_PORT, &gpio_init);

    gpio_init.Pin = I2C_SCL_PIN;
    HAL_GPIO_Init(I2C_SCL_PORT, &gpio_init);

    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
}

void I2C_Start(void)
{
    i2c_sda_set(1);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    i2c_sda_set(0);
    I2C_DELAY_US(5);
    i2c_scl_set(0);
}

void I2C_Stop(void)
{
    i2c_sda_set(0);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    i2c_sda_set(1);
    I2C_DELAY_US(5);
}

void I2C_Ack(void)
{
    i2c_sda_set(0);
    I2C_DELAY_US(2);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    i2c_scl_set(0);
    i2c_sda_set(1);
    I2C_DELAY_US(2);
}

void I2C_NAck(void)
{
    i2c_sda_set(1);
    I2C_DELAY_US(2);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    i2c_scl_set(0);
    I2C_DELAY_US(2);
}

uint8_t I2C_SendByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        i2c_scl_set(0);
        i2c_sda_set((data << i) & 0x80);
        I2C_DELAY_US(5);
        i2c_scl_set(1);
        I2C_DELAY_US(5);
    }

    i2c_sda_set(1);
    I2C_DELAY_US(2);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    uint8_t ack = i2c_sda_read();
    i2c_scl_set(0);

    return ack;
}

uint8_t I2C_ReceiveData(void)
{
    uint8_t data = 0;
    i2c_sda_set(1);

    for (uint8_t i = 0; i < 8 ; i++) {
        data <<= 1;
        i2c_scl_set(1);
        I2C_DELAY_US(5);
        if (i2c_sda_read()) {
            data |= 0x01;
        }
        i2c_scl_set(0);
        I2C_DELAY_US(2);
    }

    return data;
}

uint8_t I2C_WaitAck(void)
{
    i2c_sda_set(1);
    I2C_DELAY_US(2);
    i2c_scl_set(1);
    I2C_DELAY_US(5);
    uint8_t ack = i2c_sda_read();
    i2c_scl_set(0);

    return ack;
}

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