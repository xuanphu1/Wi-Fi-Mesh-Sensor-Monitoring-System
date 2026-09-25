#include "ip5306.h"
#include "esp_log.h"
#include "i2cdev.h"
#include "PinManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "IP5306_DRIVER";
static i2c_dev_t s_ip5306_dev = {0};
static bool s_ip5306_initialized = false;
static uint8_t s_last_valid_soc = 0;

esp_err_t ip5306_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    if (dev == NULL) return ESP_ERR_INVALID_ARG;

    dev->port = port;
    dev->addr = IP5306_I2C_ADDR;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = IP5306_DEFAULT_I2C_FREQ_HZ;
    dev->cfg.clk_flags = 0;
    dev->cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    dev->cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    dev->timeout_ticks = I2CDEV_MAX_STRETCH_TIME;
#endif

    return i2c_dev_create_mutex(dev);
}

esp_err_t ip5306_free_desc(i2c_dev_t *dev)
{
    if (dev == NULL) return ESP_ERR_INVALID_ARG;
    return i2c_dev_delete_mutex(dev);
}

i2c_dev_t *ip5306_get_dev(void)
{
    return &s_ip5306_dev;
}

esp_err_t ip5306_init(gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    if (sda_pin == GPIO_NUM_NC || sda_pin <= 0) {
        sda_pin = PIN_I2C_SDA;
    }
    if (scl_pin == GPIO_NUM_NC || scl_pin <= 0) {
        scl_pin = PIN_I2C_SCL;
    }

    // Ensure common I2C bus driver is up
    esp_err_t err = i2cInitDevCommon();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2cInitDevCommon failed: %s", esp_err_to_name(err));
        return err;
    }

    if (s_ip5306_dev.mutex != NULL) {
        i2c_dev_delete_mutex(&s_ip5306_dev);
        s_ip5306_dev.mutex = NULL;
    }

    err = ip5306_init_desc(&s_ip5306_dev, PIN_I2C_PORT_NUM, sda_pin, scl_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ip5306_init_desc failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ip5306_initialized = true;
    ESP_LOGI(TAG, "IP5306 (i2cdev standardized) initialized on port %d (SDA: GPIO %d, SCL: GPIO %d, Addr: 0x%02X)",
             PIN_I2C_PORT_NUM, sda_pin, scl_pin, IP5306_I2C_ADDR);
    return ESP_OK;
}

// Low-level register read: Write RegAddr -> STOP -> Read Byte -> STOP via i2cdev
esp_err_t ip5306_read_reg(uint8_t reg, uint8_t *val)
{
    if (val == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_ip5306_initialized || s_ip5306_dev.mutex == NULL) {
        esp_err_t init_err = ip5306_init(PIN_I2C_SDA, PIN_I2C_SCL);
        if (init_err != ESP_OK) return init_err;
    }

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        I2C_DEV_TAKE_MUTEX(&s_ip5306_dev);
        // Step 1: Write register address with STOP
        ret = i2c_dev_write(&s_ip5306_dev, NULL, 0, &reg, 1);
        if (ret == ESP_OK) {
            // Step 2: Read data byte from IP5306 with STOP
            ret = i2c_dev_read(&s_ip5306_dev, NULL, 0, val, 1);
        }
        I2C_DEV_GIVE_MUTEX(&s_ip5306_dev);

        if (ret == ESP_OK) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return ret;
}

// Low-level register write: Write RegAddr & Val -> STOP -> Readback Verify via i2cdev
esp_err_t ip5306_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_ip5306_initialized || s_ip5306_dev.mutex == NULL) {
        esp_err_t init_err = ip5306_init(PIN_I2C_SDA, PIN_I2C_SCL);
        if (init_err != ESP_OK) return init_err;
    }

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        I2C_DEV_TAKE_MUTEX(&s_ip5306_dev);
        ret = i2c_dev_write_reg(&s_ip5306_dev, reg, &val, 1);
        I2C_DEV_GIVE_MUTEX(&s_ip5306_dev);

        if (ret == ESP_OK) {
            uint8_t readback = 0;
            if (ip5306_read_reg(reg, &readback) == ESP_OK) {
                ESP_LOGI(TAG, "Write Reg 0x%02X -> Set 0x%02X | Verified Readback = 0x%02X %s",
                         reg, val, readback, (readback == val) ? "[MATCH]" : "[MISMATCH]");
            }
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "Failed to write Reg 0x%02X", reg);
    return ret;
}

esp_err_t ip5306_i2c_scan(i2c_scan_result_t *scan_res)
{
    if (scan_res == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_ip5306_initialized || s_ip5306_dev.mutex == NULL) {
        ip5306_init(PIN_I2C_SDA, PIN_I2C_SCL);
    }

    scan_res->count = 0;
    ESP_LOGI(TAG, "=== Scanning I2C Bus via i2cdev (Port %d) ===", s_ip5306_dev.port);
    i2c_dev_t probe_dev = s_ip5306_dev;

    for (uint8_t addr = 1; addr < 127; addr++) {
        probe_dev.addr = addr;
        if (i2c_dev_probe(&probe_dev, I2C_DEV_WRITE) == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 0x%02X %s", addr, (addr == IP5306_I2C_ADDR) ? "[IP5306]" : "");
            if (scan_res->count < 16) {
                scan_res->addresses[scan_res->count++] = addr;
            }
        }
    }
    ESP_LOGI(TAG, "I2C Scan Complete: Found %d device(s)", scan_res->count);
    return ESP_OK;
}


esp_err_t ip5306_get_status(ip5306_status_t *status)
{
    if (status == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t ctl0 = 0, ctl1 = 0, ctl2 = 0, read0 = 0, read1 = 0, read4 = 0;

    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) {
        status->i2c_online = false;
        return err;
    }

    ip5306_read_reg(IP5306_REG_SYS_CTL1, &ctl1);
    ip5306_read_reg(IP5306_REG_SYS_CTL2, &ctl2);
    ip5306_read_reg(IP5306_REG_READ0, &read0);
    ip5306_read_reg(IP5306_REG_READ1, &read1);
    ip5306_read_reg(IP5306_REG_READ4, &read4);

    status->i2c_online = true;
    status->raw_sys_ctl0 = ctl0;
    status->raw_sys_ctl1 = ctl1;
    status->raw_sys_ctl2 = ctl2;
    status->raw_read0 = read0;
    status->raw_read1 = read1;
    status->raw_soc_reg = read4;

    status->vin_plugged = (read0 & IP5306_READ0_VIN_PLUGGED) ? true : false;
    status->boost_output_on = (read0 & IP5306_READ0_BOOST_ON) ? true : false;
    status->is_charging = (read1 & IP5306_READ1_CHARGING) ? true : false;
    status->is_full = (read1 & IP5306_READ1_CHARGE_FULL) ? true : false;

    // Parse SOC percentage from READ1 (0x71) and READ4 (0x78)
    if (status->is_full) {
        status->soc_percent = 100;
        s_last_valid_soc = 100;
    } else {
        // Count active LED bits in 0x78 (READ4)
        uint8_t count = 0;
        for (int i = 0; i < 8; i++) {
            if (read4 & (1 << i)) count++;
        }

        if (count >= 4) {
            status->soc_percent = 100;
            s_last_valid_soc = 100;
        } else if (count == 3) {
            status->soc_percent = 75;
            s_last_valid_soc = 75;
        } else if (count == 2) {
            status->soc_percent = 50;
            s_last_valid_soc = 50;
        } else if (count == 1) {
            status->soc_percent = 25;
            s_last_valid_soc = 25;
        } else {
            // count == 0
            if (s_last_valid_soc > 0) {
                status->soc_percent = s_last_valid_soc;
            } else if (status->is_charging || status->vin_plugged) {
                status->soc_percent = 25; // Charging active
            } else {
                status->soc_percent = 0;
            }
        }
    }


    // SYS_CTL0 (0x00) parse
    status->charger_enabled = (ctl0 & IP5306_SYS_CTL0_CHARGER_EN) ? true : false;
    status->boost_output_on = (ctl0 & IP5306_SYS_CTL0_BOOST_EN) ? true : false;
    status->flashlight_on = (ctl0 & IP5306_SYS_CTL0_FLASHLIGHT_EN) ? true : false;
    status->light_load_shutdown_en = (ctl0 & IP5306_SYS_CTL0_LIGHT_LOAD_SHUTDOWN) ? true : false;
    
    // In IP5306 Datasheet: Bit 1 is 1 -> Long Press (Giữ nút), Bit 1 is 0 -> Double Click
    status->button_off_mode = (ctl0 & IP5306_SYS_CTL0_BOOST_OFF_BTN_FUNC) ? 0 : 1;
    status->key_shutdown_enabled = (ctl0 & IP5306_SYS_CTL0_KEY_SHUTDOWN_EN) ? true : false;

    // SYS_CTL1 (0x01) parse
    status->boost_low_bat_2_85v = (ctl1 & IP5306_SYS_CTL1_BOOST_LOW_BAT_SHUTDOWN) ? true : false;
    status->charge_cutoff_volt = (ctl1 & IP5306_SYS_CTL1_VOLTAGE_MASK) >> 5;
    status->light_load_time_64s = (ctl1 & IP5306_SYS_CTL1_LIGHT_LOAD_TIME_64S) ? true : false;
    status->button_long_press_3s = (ctl1 & IP5306_SYS_CTL1_LONG_PRESS_3S) ? true : false;

    // SYS_CTL2 (0x02) parse
    status->cc_charge_current = (ctl2 & IP5306_SYS_CTL2_CC_CURRENT_MASK) >> 2;
    status->charge_stop_current = (ctl2 & IP5306_SYS_CTL2_CUTOFF_CURRENT_MASK);

    return ESP_OK;
}

esp_err_t ip5306_set_light_load_shutdown(bool enable)
{
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (enable) {
        ctl0 |= IP5306_SYS_CTL0_LIGHT_LOAD_SHUTDOWN;
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_LIGHT_LOAD_SHUTDOWN;
    }
    ESP_LOGI(TAG, "Setting Light Load Auto Shutdown to: %s", enable ? "ENABLED" : "DISABLED");
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_boost_enable(bool enable)
{
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (enable) {
        ctl0 |= IP5306_SYS_CTL0_BOOST_EN;
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_BOOST_EN;
    }
    ESP_LOGI(TAG, "Setting Boost 5V output to: %s", enable ? "ENABLED" : "DISABLED");
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_charger_enable(bool enable)
{
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (enable) {
        ctl0 |= IP5306_SYS_CTL0_CHARGER_EN;
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_CHARGER_EN;
    }
    ESP_LOGI(TAG, "Setting Charger Logic to: %s", enable ? "ENABLED" : "DISABLED");
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_flashlight(bool enable)
{
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (enable) {
        ctl0 |= IP5306_SYS_CTL0_FLASHLIGHT_EN;
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_FLASHLIGHT_EN;
    }
    ESP_LOGI(TAG, "Setting Flashlight to: %s", enable ? "ON" : "OFF");
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_key_shutdown_enable(bool enable)
{
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (enable) {
        ctl0 |= IP5306_SYS_CTL0_KEY_SHUTDOWN_EN;
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_KEY_SHUTDOWN_EN;
    }
    ESP_LOGI(TAG, "Setting Button Shutdown permission to: %s", enable ? "ALLOWED" : "DISALLOWED");
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_button_off_mode(uint8_t mode)
{
    // In IP5306 Datasheet: mode 0 (Long press / Giữ nút) -> Bit 1 = 1, mode 1 (Double click) -> Bit 1 = 0
    uint8_t ctl0 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL0, &ctl0);
    if (err != ESP_OK) return err;

    if (mode == 0) {
        ctl0 |= IP5306_SYS_CTL0_BOOST_OFF_BTN_FUNC;  // Bit 1 = 1 for Long Press
    } else {
        ctl0 &= ~IP5306_SYS_CTL0_BOOST_OFF_BTN_FUNC; // Bit 1 = 0 for Double Click
    }
    ESP_LOGI(TAG, "Setting Power Button Off mode: %s (SYS_CTL0=0x%02X)",
             (mode == 0) ? "Long Press (Giữ nút)" : "Double Click (Nhấn 2 nhịp)", ctl0);
    return ip5306_write_reg(IP5306_REG_SYS_CTL0, ctl0);
}

esp_err_t ip5306_set_charge_stop_voltage(uint8_t volt_code)
{
    if (volt_code > 3) return ESP_ERR_INVALID_ARG;
    uint8_t ctl1 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL1, &ctl1);
    if (err != ESP_OK) return err;

    ctl1 &= ~IP5306_SYS_CTL1_VOLTAGE_MASK;
    ctl1 |= (volt_code << 5);

    const char *v_str[] = {"4.2V", "4.3V", "4.35V", "4.4V"};
    ESP_LOGI(TAG, "Setting Charge Cutoff Voltage to: %s", v_str[volt_code]);
    return ip5306_write_reg(IP5306_REG_SYS_CTL1, ctl1);
}

esp_err_t ip5306_set_long_press_time(bool is_3s)
{
    uint8_t ctl1 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL1, &ctl1);
    if (err != ESP_OK) return err;

    if (is_3s) {
        ctl1 |= IP5306_SYS_CTL1_LONG_PRESS_3S;
    } else {
        ctl1 &= ~IP5306_SYS_CTL1_LONG_PRESS_3S;
    }
    ESP_LOGI(TAG, "Setting Button Long Press Duration to: %s", is_3s ? "3 Seconds" : "2 Seconds");
    return ip5306_write_reg(IP5306_REG_SYS_CTL1, ctl1);
}

esp_err_t ip5306_set_light_load_time(bool is_64s)
{
    uint8_t ctl1 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL1, &ctl1);
    if (err != ESP_OK) return err;

    if (is_64s) {
        ctl1 |= IP5306_SYS_CTL1_LIGHT_LOAD_TIME_64S;
    } else {
        ctl1 &= ~IP5306_SYS_CTL1_LIGHT_LOAD_TIME_64S;
    }
    ESP_LOGI(TAG, "Setting Light Load Auto Shutdown Timer to: %s", is_64s ? "64 Seconds" : "32 Seconds");
    return ip5306_write_reg(IP5306_REG_SYS_CTL1, ctl1);
}

esp_err_t ip5306_set_low_bat_shutdown_volt(bool is_2_85v)
{
    uint8_t ctl1 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL1, &ctl1);
    if (err != ESP_OK) return err;

    if (is_2_85v) {
        ctl1 |= IP5306_SYS_CTL1_BOOST_LOW_BAT_SHUTDOWN;
    } else {
        ctl1 &= ~IP5306_SYS_CTL1_BOOST_LOW_BAT_SHUTDOWN;
    }
    ESP_LOGI(TAG, "Setting Boost Low Battery Shutdown Threshold to: %s", is_2_85v ? "2.85V" : "3.0V");
    return ip5306_write_reg(IP5306_REG_SYS_CTL1, ctl1);
}

esp_err_t ip5306_set_cc_charge_current(uint8_t code)
{
    if (code > 6) return ESP_ERR_INVALID_ARG;
    uint8_t ctl2 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL2, &ctl2);
    if (err != ESP_OK) return err;

    ctl2 &= ~IP5306_SYS_CTL2_CC_CURRENT_MASK;
    ctl2 |= (code << 2);
    ESP_LOGI(TAG, "Setting CC Charge Current setting code to: %d", code);
    return ip5306_write_reg(IP5306_REG_SYS_CTL2, ctl2);
}

esp_err_t ip5306_set_charge_stop_current(uint8_t code)
{
    if (code > 3) return ESP_ERR_INVALID_ARG;
    uint8_t ctl2 = 0;
    esp_err_t err = ip5306_read_reg(IP5306_REG_SYS_CTL2, &ctl2);
    if (err != ESP_OK) return err;

    ctl2 &= ~IP5306_SYS_CTL2_CUTOFF_CURRENT_MASK;
    ctl2 |= code;
    ESP_LOGI(TAG, "Setting Charge Stop Current setting code to: %d", code);
    return ip5306_write_reg(IP5306_REG_SYS_CTL2, ctl2);
}

esp_err_t ip5306_soft_power_off(void)
{
    ESP_LOGW(TAG, "Soft Power Off requested! Disabling boost output...");
    return ip5306_set_boost_enable(false);
}

// NVS Persistent Storage functions
esp_err_t ip5306_save_nvs_config(const ip5306_status_t *status)
{
    if (status == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ip5306_cfg", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for write: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u8(my_handle, "light_load_off", status->light_load_shutdown_en ? 1 : 0);
    nvs_set_u8(my_handle, "btn_off_mode", status->button_off_mode);
    nvs_set_u8(my_handle, "long_press_3s", status->button_long_press_3s ? 1 : 0);
    nvs_set_u8(my_handle, "light_load_64s", status->light_load_time_64s ? 1 : 0);
    nvs_set_u8(my_handle, "low_bat_285v", status->boost_low_bat_2_85v ? 1 : 0);
    nvs_set_u8(my_handle, "cutoff_volt", status->charge_cutoff_volt);
    nvs_set_u8(my_handle, "cc_current", status->cc_charge_current);
    nvs_set_u8(my_handle, "stop_current", status->charge_stop_current);
    nvs_set_u8(my_handle, "key_shut_en", status->key_shutdown_enabled ? 1 : 0);

    err = nvs_commit(my_handle);
    nvs_close(my_handle);
    ESP_LOGI(TAG, "SUCCESS: Saved IP5306 configuration to ESP32 NVS Flash memory");
    return err;
}

esp_err_t ip5306_load_and_apply_nvs_config(void)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ip5306_cfg", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No NVS saved config found. Applying default keep-alive settings...");
        ip5306_set_light_load_shutdown(false);
        return ESP_OK;
    }

    uint8_t light_load_off = 0, btn_off_mode = 0, long_press_3s = 0, light_load_64s = 0;
    uint8_t low_bat_285v = 0, cutoff_volt = 0, cc_current = 4, stop_current = 0, key_shut_en = 1;

    nvs_get_u8(my_handle, "light_load_off", &light_load_off);
    nvs_get_u8(my_handle, "btn_off_mode", &btn_off_mode);
    nvs_get_u8(my_handle, "long_press_3s", &long_press_3s);
    nvs_get_u8(my_handle, "light_load_64s", &light_load_64s);
    nvs_get_u8(my_handle, "low_bat_285v", &low_bat_285v);
    nvs_get_u8(my_handle, "cutoff_volt", &cutoff_volt);
    nvs_get_u8(my_handle, "cc_current", &cc_current);
    nvs_get_u8(my_handle, "stop_current", &stop_current);
    nvs_get_u8(my_handle, "key_shut_en", &key_shut_en);
    nvs_close(my_handle);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Restoring Saved IP5306 Settings From NVS Flash:");
    ESP_LOGI(TAG, "   BtnOffMode=%s | CutoffVolt=%d | CCCurrent=%d",
             (btn_off_mode == 0) ? "Long Press" : "Double Click", cutoff_volt, cc_current);
    ESP_LOGI(TAG, "==================================================");

    ip5306_set_light_load_shutdown(light_load_off == 1);
    ip5306_set_button_off_mode(btn_off_mode);
    ip5306_set_long_press_time(long_press_3s == 1);
    ip5306_set_light_load_time(light_load_64s == 1);
    ip5306_set_low_bat_shutdown_volt(low_bat_285v == 1);
    ip5306_set_charge_stop_voltage(cutoff_volt);
    ip5306_set_cc_charge_current(cc_current);
    ip5306_set_charge_stop_current(stop_current);
    ip5306_set_key_shutdown_enable(key_shut_en == 1);

    return ESP_OK;
}
