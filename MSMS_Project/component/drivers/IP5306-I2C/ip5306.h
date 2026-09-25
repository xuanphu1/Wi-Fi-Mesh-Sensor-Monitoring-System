#ifndef IP5306_H
#define IP5306_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "i2cdev.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default IP5306 I2C 7-bit Address
#define IP5306_I2C_ADDR             0x75

// Default I2C GPIO Configuration
#define IP5306_DEFAULT_SDA_PIN      GPIO_NUM_21
#define IP5306_DEFAULT_SCL_PIN      GPIO_NUM_22
#define IP5306_DEFAULT_I2C_PORT     I2C_NUM_0
#define IP5306_DEFAULT_I2C_FREQ_HZ  100000

// Registers
#define IP5306_REG_SYS_CTL0         0x00
#define IP5306_REG_SYS_CTL1         0x01
#define IP5306_REG_SYS_CTL2         0x02
#define IP5306_REG_READ0            0x70
#define IP5306_REG_READ1            0x71
#define IP5306_REG_READ2            0x72
#define IP5306_REG_READ4            0x78  // SOC / Power LED status

// Bit definitions for SYS_CTL0 (0x00)
#define IP5306_SYS_CTL0_CHARGER_EN           (1 << 5)  // 1: Enable charging, 0: Disable
#define IP5306_SYS_CTL0_BOOST_EN             (1 << 4)  // 1: Enable boost 5V output, 0: Disable
#define IP5306_SYS_CTL0_FLASHLIGHT_EN        (1 << 3)  // 1: Flashlight ON, 0: OFF
#define IP5306_SYS_CTL0_LIGHT_LOAD_SHUTDOWN  (1 << 2)  // 1: Auto shutdown on light load (<45mA), 0: Continuous 5V (Keep-alive)
#define IP5306_SYS_CTL0_BOOST_OFF_BTN_FUNC   (1 << 1)  // 1: Long press (Giữ nút) to off, 0: Double click to off
#define IP5306_SYS_CTL0_KEY_SHUTDOWN_EN      (1 << 0)  // 1: Allow button to turn off boost, 0: Disable button shutdown

// Bit definitions for SYS_CTL1 (0x01)
#define IP5306_SYS_CTL1_BOOST_LOW_BAT_SHUTDOWN (1 << 7) // 0: 3.0V, 1: 2.85V cutoff
#define IP5306_SYS_CTL1_VOLTAGE_MASK           (3 << 5)  // Charge stop voltage bits
#define IP5306_SYS_CTL1_VOLTAGE_4_2V           (0 << 5)
#define IP5306_SYS_CTL1_VOLTAGE_4_3V           (1 << 5)
#define IP5306_SYS_CTL1_VOLTAGE_4_35V          (2 << 5)
#define IP5306_SYS_CTL1_VOLTAGE_4_4V           (3 << 5)
#define IP5306_SYS_CTL1_SHORT_PRESS_FLASHLIGHT (1 << 3)  // 1: Short press button toggles flashlight
#define IP5306_SYS_CTL1_LIGHT_LOAD_TIME_64S    (1 << 2)  // 1: 64s auto shutdown time, 0: 32s
#define IP5306_SYS_CTL1_DOUBLE_CLICK_FLASHLIGHT (1 << 1) // 1: Double click button toggles flashlight
#define IP5306_SYS_CTL1_LONG_PRESS_3S          (1 << 0)  // 1: Long press 3s, 0: Long press 2s

// Bit definitions for SYS_CTL2 (0x02)
#define IP5306_SYS_CTL2_CC_CURRENT_MASK        (7 << 2)  // Constant Current Charge setting
#define IP5306_SYS_CTL2_CUTOFF_CURRENT_MASK    (3 << 0)  // Charge stop current setting

// Bit definitions for READ0 (0x70)
#define IP5306_READ0_VIN_PLUGGED              (1 << 3)  // 1: Charge input plugged in (5V present)
#define IP5306_READ0_BOOST_ON                 (1 << 2)  // 1: Boost 5V output active

// Bit definitions for READ1 (0x71)
#define IP5306_READ1_CHARGING                 (1 << 5)  // 1: Charging active
#define IP5306_READ1_CHARGE_FULL              (1 << 3)  // 1: Battery fully charged

// Structure to hold complete status telemetry
typedef struct {
    bool i2c_online;
    uint8_t soc_percent;           // 0%, 25%, 50%, 75%, 100%
    uint8_t raw_soc_reg;           // Raw 0x78 register
    bool vin_plugged;              // Is 5V USB charger plugged in
    bool is_charging;              // Is charging battery
    bool is_full;                  // Is battery full
    bool boost_output_on;          // Is 5V output active
    bool light_load_shutdown_en;   // Is auto shutdown enabled
    bool charger_enabled;          // Is charging logic enabled
    bool flashlight_on;            // Is auxiliary LED / flashlight ON
    bool key_shutdown_enabled;     // Is button shutdown allowed
    uint8_t button_off_mode;       // 0: Long press (Giữ nút), 1: Double click (Nhấn 2 nhịp)
    bool button_long_press_3s;     // true: 3s, false: 2s
    bool light_load_time_64s;      // true: 64s, false: 32s
    bool boost_low_bat_2_85v;      // true: 2.85V shutdown, false: 3.0V shutdown
    uint8_t charge_cutoff_volt;    // 0: 4.2V, 1: 4.3V, 2: 4.35V, 3: 4.4V
    uint8_t cc_charge_current;     // 0: 0.5A, 1: 1.0A, 2: 1.5A, 3: 2.0A, 4: 2.1A, 5: 2.2A, 6: 2.4A
    uint8_t charge_stop_current;   // 0: 200mA, 1: 400mA, 2: 500mA, 3: 600mA
    uint8_t raw_sys_ctl0;
    uint8_t raw_sys_ctl1;
    uint8_t raw_sys_ctl2;
    uint8_t raw_read0;
    uint8_t raw_read1;
} ip5306_status_t;

// I2C Scan Result structure
typedef struct {
    uint8_t count;
    uint8_t addresses[16];
} i2c_scan_result_t;

// Function prototypes (Standard i2cdev & convenience APIs)
esp_err_t ip5306_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);
esp_err_t ip5306_free_desc(i2c_dev_t *dev);
i2c_dev_t *ip5306_get_dev(void);
esp_err_t ip5306_init(gpio_num_t sda_pin, gpio_num_t scl_pin);
esp_err_t ip5306_i2c_scan(i2c_scan_result_t *scan_res);
esp_err_t ip5306_read_reg(uint8_t reg, uint8_t *val);
esp_err_t ip5306_write_reg(uint8_t reg, uint8_t val);
esp_err_t ip5306_get_status(ip5306_status_t *status);

// Complete Config & Control API
esp_err_t ip5306_set_light_load_shutdown(bool enable);
esp_err_t ip5306_set_boost_enable(bool enable);
esp_err_t ip5306_set_charger_enable(bool enable);
esp_err_t ip5306_set_flashlight(bool enable);
esp_err_t ip5306_set_key_shutdown_enable(bool enable);
esp_err_t ip5306_set_button_off_mode(uint8_t mode);          // 0: Long press (Giữ nút), 1: Double click
esp_err_t ip5306_set_long_press_time(bool is_3s);            // false: 2s, true: 3s
esp_err_t ip5306_set_light_load_time(bool is_64s);           // false: 32s, true: 64s
esp_err_t ip5306_set_low_bat_shutdown_volt(bool is_2_85v);   // false: 3.0V, true: 2.85V
esp_err_t ip5306_set_charge_stop_voltage(uint8_t volt_code); // 0: 4.2V, 1: 4.3V, 2: 4.35V, 3: 4.4V
esp_err_t ip5306_set_cc_charge_current(uint8_t code);        // 0..6 (0.5A .. 2.4A)
esp_err_t ip5306_set_charge_stop_current(uint8_t code);      // 0..3 (200mA .. 600mA)
esp_err_t ip5306_soft_power_off(void);

// NVS Persistent Storage functions
esp_err_t ip5306_save_nvs_config(const ip5306_status_t *status);
esp_err_t ip5306_load_and_apply_nvs_config(void);

#ifdef __cplusplus
}
#endif

#endif // IP5306_H
