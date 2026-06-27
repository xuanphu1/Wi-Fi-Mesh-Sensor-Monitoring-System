#include "ButtonManager.h"
#include "sdkconfig.h"

#if CONFIG_BUTTON_MODE_DIGITAL
#include "driver/gpio.h"
#else
#include "esp_adc/adc_oneshot.h"
#endif

/**
 * @file ButtonManager.c
 * @brief Target-specific button input: ESP32 uses GPIO keys, ESP32-C3/C6 uses ADC ladder.
 */

#if CONFIG_BUTTON_MODE_DIGITAL

#define BUTTON_GPIO_DOWN ((gpio_num_t)CONFIG_BUTTON_DIGITAL_DOWN_GPIO)
#define BUTTON_GPIO_SEL ((gpio_num_t)CONFIG_BUTTON_DIGITAL_SEL_GPIO)
#define BUTTON_GPIO_BACK ((gpio_num_t)CONFIG_BUTTON_DIGITAL_BACK_GPIO)
#define BUTTON_GPIO_ACTIVE_LEVEL CONFIG_BUTTON_DIGITAL_ACTIVE_LEVEL
#define BUTTON_GPIO_DEBOUNCE_MS CONFIG_BUTTON_DIGITAL_DEBOUNCE_MS

static button_type_t s_last_gpio_sample = BTN_NONE;
static button_type_t s_last_gpio_stable = BTN_NONE;
static TickType_t s_gpio_sample_tick = 0;

static const char *button_to_name(button_type_t btn) {
  switch (btn) {
  case BTN_UP:
    return "BTN_UP";
  case BTN_DOWN:
    return "BTN_DOWN";
  case BTN_SEL:
    return "BTN_SEL";
  case BTN_BACK:
    return "BTN_BACK";
  default:
    return "BTN_NONE";
  }
}

static bool gpio_button_pressed(gpio_num_t gpio) {
  return gpio_get_level(gpio) == BUTTON_GPIO_ACTIVE_LEVEL;
}

static button_type_t read_gpio_button_raw(void) {
  if (gpio_button_pressed(BUTTON_GPIO_DOWN)) {
    return BTN_DOWN;
  }
  if (gpio_button_pressed(BUTTON_GPIO_SEL)) {
    return BTN_SEL;
  }
  if (gpio_button_pressed(BUTTON_GPIO_BACK)) {
    return BTN_BACK;
  }
  return BTN_NONE;
}

void ButtonManagerInit(void) {
  gpio_config_t input_conf = {
      .pin_bit_mask = (1ULL << BUTTON_GPIO_DOWN) | (1ULL << BUTTON_GPIO_SEL) |
                      (1ULL << BUTTON_GPIO_BACK),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t err = gpio_config(&input_conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_BUTTON_MANAGER, "GPIO button init failed: %s", esp_err_to_name(err));
    return;
  }

  s_last_gpio_sample = BTN_NONE;
  s_last_gpio_stable = BTN_NONE;
  s_gpio_sample_tick = xTaskGetTickCount();

  ESP_LOGI(TAG_BUTTON_MANAGER,
           "GPIO buttons: DOWN=GPIO%d SEL=GPIO%d BACK=GPIO%d active=%d",
           (int)BUTTON_GPIO_DOWN, (int)BUTTON_GPIO_SEL,
           (int)BUTTON_GPIO_BACK, BUTTON_GPIO_ACTIVE_LEVEL);
}

button_type_t ReadButtonStatus(void) {
  button_type_t raw = read_gpio_button_raw();
  TickType_t now = xTaskGetTickCount();

  if (raw != s_last_gpio_sample) {
    s_last_gpio_sample = raw;
    s_gpio_sample_tick = now;
    return BTN_NONE;
  }

  if ((now - s_gpio_sample_tick) < pdMS_TO_TICKS(BUTTON_GPIO_DEBOUNCE_MS)) {
    return BTN_NONE;
  }

  button_type_t result = BTN_NONE;
  if (s_last_gpio_stable == BTN_NONE && raw != BTN_NONE) {
    result = raw;
    ESP_LOGD(TAG_BUTTON_MANAGER, "Button pressed: %s", button_to_name(result));
  }

  s_last_gpio_stable = raw;
  return result;
}

#else

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_channel_t s_adc_channel;

static adc_atten_t atten_from_db(int db) {
  if (db <= 0) {
    return ADC_ATTEN_DB_0;
  }
  if (db <= 2) {
    return ADC_ATTEN_DB_2_5;
  }
  if (db <= 6) {
    return ADC_ATTEN_DB_6;
  }
  return ADC_ATTEN_DB_12;
}

typedef enum {
  ZONE_NONE = 0,
  ZONE_DOWN,
  ZONE_SEL,
  ZONE_BACK,
} btn_zone_t;

static btn_zone_t zone_from_raw(int v) {
  if (v >= CONFIG_BUTTON_ANALOG_DOWN_MIN && v <= CONFIG_BUTTON_ANALOG_DOWN_MAX) {
    return ZONE_DOWN;
  }
  if (v >= CONFIG_BUTTON_ANALOG_SEL_MIN && v <= CONFIG_BUTTON_ANALOG_SEL_MAX) {
    return ZONE_SEL;
  }
  if (v >= CONFIG_BUTTON_ANALOG_BACK_MIN && v <= CONFIG_BUTTON_ANALOG_BACK_MAX) {
    return ZONE_BACK;
  }
  return ZONE_NONE;
}

static button_type_t zone_to_button(btn_zone_t z) {
  switch (z) {
  case ZONE_DOWN:
    return BTN_DOWN;
  case ZONE_SEL:
    return BTN_SEL;
  case ZONE_BACK:
    return BTN_BACK;
  default:
    return BTN_NONE;
  }
}

static int adc_read_averaged(void) {
  if (s_adc == NULL) {
    return -1;
  }
  const int n = 8;
  int sum = 0;
  for (int i = 0; i < n; i++) {
    int raw = 0;
    esp_err_t err = adc_oneshot_read(s_adc, s_adc_channel, &raw);
    if (err != ESP_OK) {
      ESP_LOGW(TAG_BUTTON_MANAGER, "adc_oneshot_read: %s", esp_err_to_name(err));
      return -1;
    }
    sum += raw;
  }
  return sum / n;
}

static int s_last_adc_raw = -1;
static btn_zone_t s_last_zone = ZONE_NONE;

static btn_zone_t read_current_zone(void) {
  int r = adc_read_averaged();
  s_last_adc_raw = r;
  if (r < 0) {
    return ZONE_NONE;
  }
  return zone_from_raw(r);
}

void ButtonManagerInit(void) {
  if (CONFIG_BUTTON_ANALOG_DOWN_MIN > CONFIG_BUTTON_ANALOG_DOWN_MAX ||
      CONFIG_BUTTON_ANALOG_SEL_MIN > CONFIG_BUTTON_ANALOG_SEL_MAX ||
      CONFIG_BUTTON_ANALOG_BACK_MIN > CONFIG_BUTTON_ANALOG_BACK_MAX) {
    ESP_LOGW(TAG_BUTTON_MANAGER,
             "An ADC range has min > max; fix Button Manager in menuconfig");
  }

  adc_unit_t unit_id;
  adc_channel_t ch;
  esp_err_t err =
      adc_oneshot_io_to_channel((int)CONFIG_BUTTON_ANALOG_GPIO, &unit_id, &ch);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_BUTTON_MANAGER,
             "GPIO %d cannot be mapped to ADC channel: %s",
             CONFIG_BUTTON_ANALOG_GPIO, esp_err_to_name(err));
    s_adc = NULL;
    return;
  }
  s_adc_channel = ch;

  adc_oneshot_unit_init_cfg_t ucfg = {
      .unit_id = unit_id,
  };
  err = adc_oneshot_new_unit(&ucfg, &s_adc);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_BUTTON_MANAGER, "adc_oneshot_new_unit: %s", esp_err_to_name(err));
    s_adc = NULL;
    return;
  }

  adc_oneshot_chan_cfg_t ccfg = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = atten_from_db(CONFIG_BUTTON_ANALOG_ATTEN_DB),
  };
  err = adc_oneshot_config_channel(s_adc, s_adc_channel, &ccfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_BUTTON_MANAGER, "adc_oneshot_config_channel: %s",
             esp_err_to_name(err));
    adc_oneshot_del_unit(s_adc);
    s_adc = NULL;
    return;
  }

  ESP_LOGI(TAG_BUTTON_MANAGER,
           "ADC buttons: GPIO %d, unit %d ch %d, atten ~%d dB | "
           "DOWN[%d-%d] SEL[%d-%d] BACK[%d-%d]",
           CONFIG_BUTTON_ANALOG_GPIO, (int)unit_id, (int)s_adc_channel,
           CONFIG_BUTTON_ANALOG_ATTEN_DB, CONFIG_BUTTON_ANALOG_DOWN_MIN,
           CONFIG_BUTTON_ANALOG_DOWN_MAX, CONFIG_BUTTON_ANALOG_SEL_MIN,
           CONFIG_BUTTON_ANALOG_SEL_MAX, CONFIG_BUTTON_ANALOG_BACK_MIN,
           CONFIG_BUTTON_ANALOG_BACK_MAX);
}

button_type_t ReadButtonStatus(void) {
  if (s_adc == NULL) {
    ESP_LOGW(TAG_BUTTON_MANAGER, "ADC not initialized");
    return BTN_NONE;
  }

  btn_zone_t z = read_current_zone();
  static TickType_t s_last_status_log_tick = 0;
  TickType_t now = xTaskGetTickCount();
  if (z != s_last_zone ||
      (now - s_last_status_log_tick) >= pdMS_TO_TICKS(500)) {
    s_last_status_log_tick = now;
  }

  button_type_t result = BTN_NONE;
  if (s_last_zone == ZONE_NONE && z != ZONE_NONE) {
    result = zone_to_button(z);
    ESP_LOGD(TAG_BUTTON_MANAGER, "Button pressed: raw=%d zone=%d", s_last_adc_raw,
             (int)z);
  }

  s_last_zone = z;
  return result;
}

#endif
