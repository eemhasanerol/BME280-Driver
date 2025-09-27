/*
 * main_example.c
 *
 * Example usage of BME280 driver
 *
 * Tested on: STM32F407 (custom drivers for RCC, GPIO, I2C, SysTick)
 */

#include "stm32f407xx.h"
#include "RCC.h"
#include "GPIO.h"
#include "I2C.h"
#include "Systick.h"
#include "bme280.h"
#include <stdio.h>

/* -------- System clock (Hz) -------- */
uint32_t SystemCoreClock = 16000000U;  // HSI default 16 MHz

/* -------- I2C handle -------- */
static I2C_HandleTypeDef_t hi2c1;

/* -------- SysTick handle -------- */
static SYSTICK_HandleTypeDef_t hsystick;

/* -------- I2C wrapper functions -------- */
static int32_t platform_i2c_read(uint8_t dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (I2C_MemRead(&hi2c1, dev, reg, buf, (uint8_t)len) != STATUS_OK) {
        return BME280_E_COMM;
    }
    return BME280_OK;
}

static int32_t platform_i2c_write(uint8_t dev, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    if (I2C_MemWrite(&hi2c1, dev, reg, buf, (uint8_t)len) != STATUS_OK) {
        return BME280_E_COMM;
    }
    return BME280_OK;
}

/* -------- Delay wrapper -------- */
static void platform_delay_ms(uint32_t ms)
{
    SysTick_Delay_ms(ms);
}

/* -------- Init functions -------- */
static void I2C1_InitPins(void)
{
    RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef_t I2C_Pins = {0};
    I2C_Pins.pinNumber  = (GPIO_PIN_6 | GPIO_PIN_7);  // PB6 = SCL, PB7 = SDA
    I2C_Pins.Mode       = GPIO_MODE_AF;
    I2C_Pins.Otype      = GPIO_OTYPE_OD;
    I2C_Pins.PuPd       = GPIO_PULLUP;
    I2C_Pins.Speed      = GPIO_SPEED_HIGH;
    I2C_Pins.Alternate  = GPIO_AF4_I2C1;              // AF4 = I2C1

    GPIO_Init(GPIOB, &I2C_Pins);
}

static void I2C1_InitPeripheral(void)
{
    RCC_I2C1_CLK_ENABLE();

    hi2c1.inst                 = I2C1;
    hi2c1.config.ack           = I2C_ACK_ENABLE;
    hi2c1.dev_addr             = 0x00;                /* master own addr */
    hi2c1.config.fm_duty       = I2C_FM_DUTY_2;
    hi2c1.config.scl_speed_hz  = I2C_SPEED_STANDARD;  /* 100 kHz */
    hi2c1.timeout_ms           = 1000;

    I2C_Init(&hi2c1);
}

static void SysTick_InitConfig(void)
{
    hsystick.tick_hz       = 1000U;                 // 1 ms tick
    hsystick.clksource     = SYSTICK_CLKSRC_AHB;    // AHB clock
    hsystick.use_interrupt = 1U;                    // enable IRQ
    hsystick.nvic_priority = 0xF;                   // lowest priority

    SysTick_Init(&hsystick);
}

/* -------- Main -------- */
int main(void)
{
    /* Init I2C1 */
    I2C1_InitPins();
    I2C1_InitPeripheral();

    /* Init SysTick */
    SysTick_InitConfig();

    /* BME280 handle + default config */
    bme280_dev_t bme = {0};

    bme.dev_addr = BME280_I2C_ADDR_SDO_LOW; /* 0x76 */
    bme.osr_t    = BME280_OSR_T_2X;
    bme.osr_p    = BME280_OSR_P_4X;
    bme.osr_h    = BME280_OSR_H_1X;
    bme.filter   = BME280_FILTER_4;
    bme.standby  = BME280_STBY_1000_MS;
    bme.mode     = BME280_MODE_NORMAL;

    /* Assign hooks */
    bme.i2c_read  = platform_i2c_read;
    bme.i2c_write = platform_i2c_write;
    bme.delay_ms  = SysTick_Delay_ms;

    /* Small delay before init */
    SysTick_Delay_ms(10);

    /* Init (ID check + reset + config + calibration load) */
    if (bme280_init(&bme) != BME280_OK) {
        while (1);  /* error */
    }

    bme280_data_t data;
    while (1)
    {
        if (bme280_read_all(&bme, &data) == BME280_OK) {
            printf("Temp: %.2f °C | Pressure: %.2f Pa | Humidity: %.2f %%RH\r\n",
                   data.temperature_c,
                   data.pressure_pa,
                   data.humidity_rh);
        }

        platform_delay_ms(1000);
    }
}
