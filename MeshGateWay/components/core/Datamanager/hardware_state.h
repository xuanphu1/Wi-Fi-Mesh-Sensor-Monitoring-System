#ifndef HARDWARE_STATE_H
#define HARDWARE_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "i2cdev.h"
#include "ds3231.h"
#include "sdkconfig.h"

// ===== HW Config =====
#ifdef CONFIG_DS3231_I2C_PORT
#define DS3231_I2C_PORT       CONFIG_DS3231_I2C_PORT
#else
#define DS3231_I2C_PORT       0
#endif

#ifdef CONFIG_DS3231_SDA_GPIO
#define DS3231_SDA_GPIO       CONFIG_DS3231_SDA_GPIO
#else
#define DS3231_SDA_GPIO       21
#endif

#ifdef CONFIG_DS3231_SCL_GPIO
#define DS3231_SCL_GPIO       CONFIG_DS3231_SCL_GPIO
#else
#define DS3231_SCL_GPIO       22
#endif

// GPIO34 on ESP32 => ADC1 channel 6
#define BAT_ADC_CHANNEL       ADC1_CHANNEL_6

// Voltage divider: R_TOP (pin -> ADC), R_BOTTOM (ADC -> GND)
// Dùng 2 điện trở 20k - 20k
#define BAT_R_TOP_OHMS        20000
#define BAT_R_BOTTOM_OHMS     20000

// ADC mV calibration scale (integer ratio).
// Thực tế: Vbat=4.02V, divider 20k/20k => Vadc_expected=4.02/2=2.01V
// Log ADC: Vadc_measured ≈ 2.05V => scale ~= 2.01/2.05 ≈ 0.9805
#define BAT_ADC_MV_SCALE_NUM  201
#define BAT_ADC_MV_SCALE_DEN  205

// Battery voltage range (mV) before divider
#define BAT_VMIN_MV           3300
#define BAT_VMAX_MV           4200

#define BAT_ADC_ATTEN         ADC_ATTEN_DB_11
#define BAT_ADC_WIDTH         ADC_WIDTH_BIT_12

typedef struct
{
    // Runtime sensor/io state
    i2c_dev_t rtc_dev;
    bool rtc_ready;
    esp_adc_cal_characteristics_t adc_chars;
    bool adc_ready;
} dm_hw_t;

#endif // HARDWARE_STATE_H
