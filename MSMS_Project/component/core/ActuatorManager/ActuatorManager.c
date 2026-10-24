#include "ActuatorManager.h"
#include "DataManager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ActuatorManager";
static bool s_initialized = false;

typedef struct {
  bool is_configured;
  bool level;
} actuator_pin_state_t;

static actuator_pin_state_t s_pin_states[GPIO_NUM_MAX];

static bool is_actuator_pin_allowed(int gpio_num) {
  if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
    return false;
  }
#if defined(CONFIG_IDF_TARGET_ESP32)
  if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio_num)) {
    return false;
  }
#endif
  /* Board-level whitelist: only designated actuator pins are allowed */
  if (gpio_num == PIN_ACTUATOR_IO1_P1 ||
      gpio_num == PIN_ACTUATOR_IO3_P2 ||
      gpio_num == PIN_ACTUATOR_IO1_P3) {
    return true;
  }
  return false;
}

system_err_t ActuatorManager_Init(void) {
  if (s_initialized) {
    return MRS_OK;
  }
  memset(s_pin_states, 0, sizeof(s_pin_states));
  s_initialized = true;
  ESP_LOGI(TAG, "ActuatorManager initialized with board pin whitelist");
  return MRS_OK;
}

system_err_t ActuatorManager_Deinit(void) {
  if (!s_initialized) {
    return MRS_OK;
  }

  /* Drive all configured actuators to safe level (OFF / LOW) */
  for (int pin = 0; pin < GPIO_NUM_MAX; pin++) {
    if (s_pin_states[pin].is_configured) {
      (void)gpio_set_level((gpio_num_t)pin, 0);
      s_pin_states[pin].level = false;
      s_pin_states[pin].is_configured = false;
    }
  }

  s_initialized = false;
  ESP_LOGI(TAG, "ActuatorManager deinitialized (all actuators reset to safe level)");
  return MRS_OK;
}

system_err_t ActuatorManager_SetLevel(int gpio_num, bool enabled) {
  if (!s_initialized) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!is_actuator_pin_allowed(gpio_num)) {
    ESP_LOGE(TAG, "GPIO %d is not a whitelisted/valid output actuator pin", gpio_num);
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (!s_pin_states[gpio_num].is_configured) {
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t cfg_result = gpio_config(&config);
    if (cfg_result != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure GPIO %d as output: %s", gpio_num,
               esp_err_to_name(cfg_result));
      return MRS_ERR_DRIVERS_INIT_FAILED;
    }
    s_pin_states[gpio_num].is_configured = true;
  }

  esp_err_t set_result = gpio_set_level((gpio_num_t)gpio_num, enabled ? 1 : 0);
  if (set_result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set actuator GPIO %d to %d: %s", gpio_num,
             (int)enabled, esp_err_to_name(set_result));
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  s_pin_states[gpio_num].level = enabled;
  return MRS_OK;
}

system_err_t ActuatorManager_GetLevel(int gpio_num, bool *out_enabled) {
  if (!s_initialized) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!is_actuator_pin_allowed(gpio_num) || out_enabled == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (s_pin_states[gpio_num].is_configured) {
    *out_enabled = s_pin_states[gpio_num].level;
  } else {
    *out_enabled = (gpio_get_level((gpio_num_t)gpio_num) != 0);
  }
  return MRS_OK;
}
