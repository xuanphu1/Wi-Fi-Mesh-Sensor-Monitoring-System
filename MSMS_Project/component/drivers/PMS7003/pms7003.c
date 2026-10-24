#include "pms7003.h"
#include <inttypes.h>
#include <string.h>
#include "PinManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

__attribute__((unused)) static const char *TAG = "PMS7003";

static uart_port_t s_pms_uart_num = UART_NUM_MAX;
static SemaphoreHandle_t s_pms_mutex = NULL;

esp_err_t pms7003_initUart(uart_port_t uart_num, int tx_pin, int rx_pin) {
  if (uart_num >= UART_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  s_pms_uart_num = uart_num;

  if (s_pms_mutex == NULL) {
    s_pms_mutex = xSemaphoreCreateMutex();
    if (s_pms_mutex == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }

  if (!uart_is_driver_installed(uart_num)) {
    ESP_LOGE(__func__, "UART port %d driver not installed by UartResourceManager", (int)uart_num);
    return ESP_ERR_INVALID_STATE;
  }
  (void)tx_pin;
  (void)rx_pin;

  ESP_LOGI(__func__, "PMS7003 UART port %d bound successfully.", (int)uart_num);
  return ESP_OK;
}

esp_err_t pms7003_deinitUart(void) {
  if (s_pms_mutex != NULL) {
    SemaphoreHandle_t mutex = s_pms_mutex;
    s_pms_mutex = NULL;
    vSemaphoreDelete(mutex);
  }
  s_pms_uart_num = UART_NUM_MAX;
  return ESP_OK;
}

esp_err_t pms7003_activeMode(void) {
  if (s_pms_uart_num == UART_NUM_MAX) {
    return ESP_ERROR_PMS7003_SET_ACTIVE_MODE_FAILED;
  }
  int len = uart_write_bytes(s_pms_uart_num, pms7003_commandActiveMode,
                             sizeof(pms7003_commandActiveMode));
  if (len < 0) {
    ESP_LOGE(__func__, "Set active mode failed.");
    return ESP_ERROR_PMS7003_SET_ACTIVE_MODE_FAILED;
  }
  ESP_LOGI(__func__, "Set active mode successfully.");
  return ESP_OK;
}

esp_err_t pms7003_readData(const int pms_modeAmbience, uint32_t *pm1_0,
                           uint32_t *pm2_5, uint32_t *pm10) {
  if (pm1_0 == NULL || pm2_5 == NULL || pm10 == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_pms_uart_num == UART_NUM_MAX || s_pms_mutex == NULL) {
    return ESP_ERROR_PMS7003_READ_DATA_FAILED;
  }

  if (xSemaphoreTake(s_pms_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    ESP_LOGE(__func__, "Mutex take timeout");
    return ESP_ERROR_PMS7003_READ_DATA_FAILED;
  }

  uint8_t rawData[128] = {0};
  int read_len = uart_read_bytes(s_pms_uart_num, rawData, sizeof(rawData), pdMS_TO_TICKS(200));
  if (read_len < 32) {
    ESP_LOGD(__func__, "Insufficient bytes read: %d", read_len);
    xSemaphoreGive(s_pms_mutex);
    *pm1_0 = PMS_ERROR_INVALID_VALUE;
    *pm2_5 = PMS_ERROR_INVALID_VALUE;
    *pm10 = PMS_ERROR_INVALID_VALUE;
    return ESP_ERROR_PMS7003_READ_DATA_FAILED;
  }

  bool valid_frame = false;
  size_t max_search = (read_len >= 32) ? (size_t)(read_len - 32) : 0;

  for (size_t i = 0; i <= max_search; i++) {
    if (rawData[i] == START_CHARACTER_1 && rawData[i + 1] == START_CHARACTER_2) {
      uint16_t frame_len = ((uint16_t)rawData[i + 2] << 8) | rawData[i + 3];
      if (frame_len < 28) {
        continue;
      }

      // Checksum validation: sum bytes i through i+29
      uint16_t checksum = 0;
      for (size_t k = 0; k < 30; k++) {
        checksum += rawData[i + k];
      }
      uint16_t expected_checksum = ((uint16_t)rawData[i + 30] << 8) | rawData[i + 31];

      if (checksum != expected_checksum) {
        ESP_LOGD(__func__, "Checksum mismatch: calc=0x%04x, exp=0x%04x", checksum, expected_checksum);
        continue;
      }

      size_t startByte = i + ((pms_modeAmbience == indoor) ? 4 : 10);
      *pm1_0 = ((uint32_t)rawData[startByte] << 8) | rawData[startByte + 1];
      *pm2_5 = ((uint32_t)rawData[startByte + 2] << 8) | rawData[startByte + 3];
      *pm10  = ((uint32_t)rawData[startByte + 4] << 8) | rawData[startByte + 5];

      ESP_LOGI(__func__, "PMS7003 read success: PM1.0=%" PRIu32 ", PM2.5=%" PRIu32 ", PM10=%" PRIu32,
               *pm1_0, *pm2_5, *pm10);
      valid_frame = true;
      break;
    }
  }

  xSemaphoreGive(s_pms_mutex);

  if (!valid_frame) {
    *pm1_0 = PMS_ERROR_INVALID_VALUE;
    *pm2_5 = PMS_ERROR_INVALID_VALUE;
    *pm10  = PMS_ERROR_INVALID_VALUE;
    ESP_LOGD(__func__, "PMS7003 sensor read data failed: no valid frame found");
    return ESP_ERROR_PMS7003_READ_DATA_FAILED;
  }

  return ESP_OK;
}
