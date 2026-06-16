#include "ScreenManager.h"
#include "DataManager.h"
#include "MeshManager.h"
#include "SensorRegistry.h"
#include "SystemPerfomance.h"
#include "TimeManager.h"
#include "WifiManager.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

/**
 * @file ScreenManager.c
 * @brief OLED drawing helpers and mutex-guarded public screen APIs.
 */

ssd1306_handle_t oled = NULL;
static SemaphoreHandle_t oled_mutex = NULL;

typedef struct {
  i2c_port_t bus;
  uint16_t dev_addr;
  uint8_t s_chDisplayBuffer[128][8];
} my_ssd1306_dev_t;

static void my_draw_small_char(ssd1306_handle_t dev, int chXpos, int chYpos,
                               uint8_t chChr) {
  if (chChr < 32 || chChr > 127)
    chChr = 32;
  for (int i = 0; i < 5; i++) {
    uint8_t col = font5x7[chChr - 32][i];
    for (int j = 0; j < 8; j++) {
      if (col & 1) {
        if (chXpos + i >= 0 && chXpos + i < 128 && chYpos + j >= 0 &&
            chYpos + j < 64) {
          ssd1306_fill_point(dev, chXpos + i, chYpos + j, 1);
        }
      }
      col >>= 1;
    }
  }
}
static void __attribute__((unused))
my_draw_small_string(ssd1306_handle_t dev, int chXpos, int chYpos,
                     const char *pchString) {
  while (*pchString != '\0') {
    my_draw_small_char(dev, chXpos, chYpos, *pchString);
    chXpos += 6;
    pchString++;
  }
}
static void __attribute__((unused))
my_draw_bitmap_horizontal(ssd1306_handle_t dev, int x, int y,
                          const uint8_t *bitmap, int w, int h) {
  if (!bitmap)
    return;
  int bytes_per_row = (w + 7) / 8;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      int byte_idx = j * bytes_per_row + (i / 8);
      uint8_t bit_mask = 1 << (7 - (i % 8));
      if (bitmap[byte_idx] & bit_mask) {
        if (x + i >= 0 && x + i < 128 && y + j >= 0 && y + j < 64) {
          ssd1306_fill_point(dev, x + i, y + j, 1);
        }
      }
    }
  }
}
static void __attribute__((unused))
invert_round_rect(ssd1306_handle_t dev, int x, int y, int w, int h) {
  my_ssd1306_dev_t *device = (my_ssd1306_dev_t *)dev;
  if (!device)
    return;
  for (int px = x; px < x + w; px++) {
    for (int py = y; py < y + h; py++) {
      if (px >= 0 && px < 128 && py >= 0 && py < 64) {
        // Bỏ qua 4 góc để bo tròn
        if ((px == x && py == y) || (px == x + 1 && py == y) ||
            (px == x && py == y + 1))
          continue;
        if ((px == x + w - 1 && py == y) || (px == x + w - 2 && py == y) ||
            (px == x + w - 1 && py == y + 1))
          continue;
        if ((px == x && py == y + h - 1) || (px == x + 1 && py == y + h - 1) ||
            (px == x && py == y + h - 2))
          continue;
        if ((px == x + w - 1 && py == y + h - 1) ||
            (px == x + w - 2 && py == y + h - 1) ||
            (px == x + w - 1 && py == y + h - 2))
          continue;
        // Tính toán vị trí bit trong buffer của thư viện espressif__ssd1306
        uint8_t chPos = 7 - py / 8;
        uint8_t chBx = py % 8;
        uint8_t chTemp = 1 << (7 - chBx);
        device->s_chDisplayBuffer[px][chPos] ^= chTemp;
      }
    }
  }
}

/**
 * @brief Draw one menu row: optional “>” prefix plus item title at 12 px font.
 */

static void initUIState(void);

/**
 * @brief Take ownership of the SSD1306 handle, create OLED mutex, show splash.
 */
system_err_t ScreenManagerInit(ssd1306_handle_t *_oled) {
  if (_oled == NULL || *_oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "ScreenManagerInit: oled handle is NULL");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }
  oled = *_oled;
  if (oled_mutex == NULL) {
    oled_mutex = xSemaphoreCreateMutex();
    if (oled_mutex == NULL) {
      ESP_LOGE(TAG_SCREEN_MANAGER,
               "ScreenManagerInit: failed to create oled mutex");
      return MRS_ERR_SCREENMANAGER_NOT_INIT;
    }
  }
  initUIState();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  return MRS_OK;
}

/**
 * @brief Splash: logo bitmap and static text on first boot.
 */
static void initUIState(void) {
  ssd1306_clear_screen(oled, 0x00);
  ssd1306_draw_bitmap(oled, 40, -18,
                      (uint8_t *)imageManager[OBJECT_DIFFERENT][0], 48, 71);
  ssd1306_draw_string(oled, 16, 52, (const uint8_t *)"Designed by MrKoi", 12,
                      1);
  ssd1306_draw_string(oled, 54, 37, (const uint8_t *)"MRS", 16, 1);
  ssd1306_refresh_gram(oled);
}

bool g_menu_is_animating = false;

static system_err_t Draw_Menu_Frame(menu_list_t *menu, int8_t *selected,
                                    objectInfoManager_t *objectInfo) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "MenuRender: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }
  if (menu == NULL || selected == NULL) {
    ESP_LOGW(TAG_SCREEN_MANAGER, "MenuRender: invalid params");
    return MRS_ERR_UI_INVALID_MENU;
  }
  if (objectInfo == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }

  // Khởi tạo các biến tracking cho animation
  static float anim_selection = 0;
  static menu_list_t *last_menu = NULL;

  if (menu != last_menu) {
    anim_selection = *selected; // Reset animation khi đổi menu
    last_menu = menu;
  }
  ssd1306_clear_screen(oled, 0);
  // 1. VẼ PHẦN HEADER CỦA DỰ ÁN CŨ (Wifi, Battery, v.v...)
  uint32_t offset = 0;
  if (menu->image.image != NULL) {
    switch (menu->object) {
    case OBJECT_WIFI: {
      objectInfo->wifiInfo.wifiStatus =
          is_wifi_connected() ? CONNECTED : DISCONNECTED;
      ssd1306_draw_bitmap(
          oled, 55, 0,
          (uint8_t *)
              menu->image.image[menu->object][objectInfo->wifiInfo.wifiStatus],
          menu->image.width, menu->image.height);
      break;
    }
    case OBJECT_BATTERY: {
      uint8_t index = objectInfo->batteryInfo.batteryLevel;
      if (index >= 7)
        index = 0;
      ssd1306_draw_bitmap(oled, 55, 0,
                          (uint8_t *)menu->image.image[menu->object][index],
                          menu->image.width, menu->image.height);
      break;
    }
    case OBJECT_WIFI_MESH: {
      ssd1306_draw_bitmap(oled, 48, 0, (uint8_t *)menu->image.image[0][2],
                          menu->image.width, menu->image.height);
      break;
    }
    default:
      break;
    }
    offset += menu->image.height;
  }

  if (menu->text.text != NULL) {
    bool extraText = false;
    switch (menu->object) {
    case OBJECT_WIFI: {
      unsigned int textLength = strlen(
          menu->text.text[menu->object][objectInfo->wifiInfo.wifiStatus]);
      ssd1306_draw_string(
          oled, 0, offset,
          (uint8_t *)
              menu->text.text[menu->object][objectInfo->wifiInfo.wifiStatus],
          menu->text.size, 1);
      if (objectInfo->wifiInfo.wifiStatus == CONNECTED &&
          objectInfo->wifiInfo.wifiName != NULL) {
        char wifi_info_text[64];
        snprintf(wifi_info_text, sizeof(wifi_info_text), "%s%s",
                 objectInfo->wifiInfo.wifiName,
                 menu->text.text[menu->object][2]);
        ssd1306_draw_string(oled, textLength * (menu->text.size / 2), offset,
                            (uint8_t *)wifi_info_text, menu->text.size, 1);
        textLength += strlen(wifi_info_text);
      }
      if (textLength > MAX_TEXT_LENGTH)
        extraText = true;
    } break;
    case OBJECT_BATTERY: {
      const char *battery_text =
          objectInfo->batteryInfo.batteryName
              ? objectInfo->batteryInfo.batteryName
              : menu->text.text[menu->object]
                               [objectInfo->batteryInfo.batteryLevel < 7
                                    ? objectInfo->batteryInfo.batteryLevel
                                    : 0];
      if (battery_text != NULL) {
        ssd1306_draw_string(oled, 0, offset, (uint8_t *)battery_text,
                            menu->text.size, 1);
        if (strlen(battery_text) > MAX_TEXT_LENGTH)
          extraText = true;
      }
      break;
    }
    case OBJECT_WIFI_MESH: {
      char mesh_information_text[64];
      const char *base = menu->text.text[2][0];
      snprintf(mesh_information_text, sizeof(mesh_information_text), "%s",
               base);
      ssd1306_draw_string(oled, 0, offset, (uint8_t *)mesh_information_text,
                          menu->text.size, 1);
      break;
    }
    default:
      break;
    }
    offset += extraText ? (menu->text.size * 2) : menu->text.size;
  }
  // 2. TÍNH TOÁN ANIMATION
  float diff = *selected - anim_selection;
  if (diff > menu->count / 2.0f)
    diff -= menu->count;
  if (diff < -menu->count / 2.0f)
    diff += menu->count;
  if (diff > -0.02f && diff < 0.02f) {
    anim_selection = *selected;
    g_menu_is_animating = false; // Đã đến đích
  } else {
    anim_selection += diff * 0.2f; // Hệ số cuộn animation
    g_menu_is_animating = true;    // Đang chạy animation
  }
  if (anim_selection < 0)
    anim_selection += menu->count;
  if (anim_selection >= menu->count)
    anim_selection -= menu->count;
  // 3. VẼ MENU DẠNG VÒNG CUNG
  int available_h = 64 - offset;
  int center_base_y = offset + available_h / 2;
  // ESP_LOGI("TEST_MENU", "Start drawing menu. count = %d, items = %p",
  // menu->count, menu->items); // Bỏ log debug
  if (menu->count > 0) {
    int start_i, end_i;
    if (menu->count == 1) {
      start_i = 0;
      end_i = 0;
    } else {
      start_i = (int)anim_selection - 3;
      end_i = (int)anim_selection + 3;
    }
    for (int i = start_i; i <= end_i; i++) {
      float rel = i - anim_selection;
      float center_y = center_base_y + rel * 20;
      uint8_t size = (rel > -0.5f && rel < 0.5f) ? 12 : 8;
      int draw_y = (int)(center_y - (size == 12 ? 6.0f : 4.0f));

      if (draw_y > (int)offset - 16 && draw_y < 64) {
        int actual_index = i % menu->count;
        if (actual_index < 0) {
          actual_index += menu->count;
        }

        float arc_offset = rel * rel * 6.0f;
        uint8_t base_x = (menu->items[actual_index].image_item_preview.image != NULL) ? 50 : 10;
        uint8_t x_pos = base_x + (uint8_t)arc_offset;

        if (menu->items != NULL && menu->items[actual_index].name != NULL) {
          if (size == 12) {
            ssd1306_draw_string(oled, x_pos, (uint8_t)draw_y,
                                (uint8_t *)menu->items[actual_index].name, 12,
                                1);
          } else {
            my_draw_small_string(oled, x_pos, draw_y,
                                 menu->items[actual_index].name);
          }
        }
      }
    }
  }
  // 4. VẼ HÌNH ẢNH ICON BÊN TRÁI CHO ITEM ĐƯỢC CHỌN (Tĩnh)
  if (menu->items[*selected].image_item_preview.image != NULL) {
    int img_w = menu->items[*selected].image_item_preview.width;
    int img_h = menu->items[*selected].image_item_preview.height;
    int img_x = (46 - img_w) / 2;
    int img_y = center_base_y - img_h / 2;
    my_draw_bitmap_horizontal(
        oled, img_x, img_y,
        menu->items[*selected].image_item_preview.image[3][*selected], img_w,
        img_h);
  }

  // 5. VẼ HỘP INVERT ROUND RECT VÀO TRUNG TÂM
  int box_h = 16;
  int box_y = center_base_y - box_h / 2;
  int box_x = (menu->items[*selected].image_item_preview.image != NULL) ? 46 : 6;
  int box_w = 128 - box_x; 
  invert_round_rect(oled, box_x, box_y, box_w, box_h);
  esp_err_t ret = ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "MenuRender: refresh_gram failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  return MRS_OK;
}

void ScreenDashboard(DataManager_t *data) {
  if (oled == NULL || data == NULL)
    return;
  if (oled_mutex != NULL && xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE)
    return;

  ssd1306_clear_screen(oled, 0x00);

  // Line 1
  ssd1306_draw_line(oled, 0, 16, 127, 16);

  // MeshLabel
  my_draw_small_string(oled, 1, 4, "Mesh Network");

  // battery icon
  int bat_idx = data->objectInfo.batteryInfo.batteryLevel;
  if (bat_idx > 7)
    bat_idx = 7;
  my_draw_bitmap_horizontal(oled, 104, 0, imageManager[1][bat_idx], 24, 16);

  if (data->objectInfo.batteryInfo.is_charging) {
    my_draw_small_string(oled, 80, 4, "Chg");
  } else {
    // Battery value
    char bat_str[10];
    snprintf(bat_str, sizeof(bat_str), "%d%%",
             data->objectInfo.batteryInfo.levelPercent);
    my_draw_small_string(oled, 80, 4, bat_str);
  }

  // Mac
  uint8_t mac[6];
  wifi_manager_get_mac(mac);
  char mac_str[24];
  snprintf(mac_str, sizeof(mac_str), "MAC:%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  my_draw_small_string(oled, 0, 56, mac_str);

  // NetworkSig text (dBm)
  char rssi_str[16] = "--dBm";
  if (data->objectInfo.wifiInfo.wifiStatus == CONNECTED) {
    snprintf(rssi_str, sizeof(rssi_str), "%ddBm", wifi_manager_get_rssi());
  }
  my_draw_small_string(oled, 22, 46, rssi_str);

  // NetworkSig icon
  my_draw_bitmap_horizontal(
      oled, 1, 39, imageManager[2][2], 16,
      16); // image_NetworkSig_bits is index 2 in imageManagerDifferent

  // clock icon
  my_draw_bitmap_horizontal(oled, 1, 19, imageManager[2][1], 15,
                            16); // image_clock_bits is index 1

  // Time text
  struct tm timeinfo = {0};
  char time_str[16] = "--:--:--";
  if (TimeManager_GetTime(&timeinfo) == ESP_OK) {
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", timeinfo.tm_hour,
             timeinfo.tm_min, timeinfo.tm_sec);
  }
  my_draw_small_string(oled, 21, 23, time_str);

  // Uptime text
  int64_t uptime_us = esp_timer_get_time();
  int uptime_s = uptime_us / 1000000;
  char uptime_str[16];
  snprintf(uptime_str, sizeof(uptime_str), "%02d:%02d:%02d", uptime_s / 3600,
           (uptime_s % 3600) / 60, uptime_s % 60);
  my_draw_small_string(oled, 21, 33, uptime_str);

  // Vertical line 10
  ssd1306_draw_line(oled, 71, 18, 71, 40);

  // StatusMesh
  my_draw_small_string(oled, 85, 20, "Active");

  // Version
  my_draw_small_string(oled, 74, 33, "Ver:1.0.0");

  // Vertical line 13
  ssd1306_draw_line(oled, 71, 39, 71, 52);

  // NetworkSpeed
  char speed_str[16] = "--MBS";
  if (data->objectInfo.meshInfo.meshStatus == CONNECTED) {
    snprintf(speed_str, sizeof(speed_str), "%dMBS",
             mesh_manager_get_throughput());
  }
  my_draw_small_string(oled, 86, 46, speed_str);

  ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
}

void ScreenSensors(DataManager_t *data) {
  if (oled == NULL || data == NULL)
    return;
  if (oled_mutex != NULL && xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE)
    return;

  ssd1306_clear_screen(oled, 0x00);

  // Header
  my_draw_small_string(oled, 28, 0, "SENSORS DATA");
  ssd1306_draw_line(oled, 0, 10, 127, 10);

  for (int i = 0; i < NUM_PORTS; i++) {
    char buf[32];
    int y_offset = 14 + i * 17;

    // Draw port number
    snprintf(buf, sizeof(buf), "P%d:", i + 1);
    my_draw_small_string(oled, 0, y_offset, buf);

    if (data->selectedSensor[i] != SENSOR_NONE) {
      sensor_driver_t *driver =
          sensor_registry_get_driver(data->selectedSensor[i]);
      if (driver != NULL) {
        // Print sensor name
        my_draw_small_string(oled, 22, y_offset, driver->name);

        // Print first value and unit
        if (driver->unit_count > 0) {
          snprintf(buf, sizeof(buf), "%.1f %s", data->port_data[i].data_fl[0],
                   driver->unit[0] ? driver->unit[0] : "");
          my_draw_small_string(oled, 70, y_offset, buf);
        }
      }
    } else {
      my_draw_small_string(oled, 22, y_offset, "None");
    }
    // Draw line separator between ports
    if (i < 2)
      ssd1306_draw_line(oled, 0, y_offset + 14, 127, y_offset + 14);
  }

  ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
}

void ScreenPerformance(DataManager_t *data) {
  if (oled == NULL || data == NULL)
    return;
  if (oled_mutex != NULL && xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE)
    return;

  ssd1306_clear_screen(oled, 0x00);
  my_draw_small_string(oled, 15, 0, "SYSTEM PERFORMANCE");
  ssd1306_draw_line(oled, 0, 10, 127, 10);

  uint8_t cpu_hist[PERFORMANCE_HISTORY_SIZE];
  uint8_t ram_hist[PERFORMANCE_HISTORY_SIZE];
  SystemPerformance_GetCPUHistory(cpu_hist);
  SystemPerformance_GetRAMHistory(ram_hist);

  char buf[32];

  // Draw CPU Graph
  snprintf(buf, sizeof(buf), "CPU: %d%%",
           cpu_hist[PERFORMANCE_HISTORY_SIZE - 1]);
  my_draw_small_string(oled, 0, 14, buf);
  for (int i = 0; i < 100; i++) {
    int hist_idx = PERFORMANCE_HISTORY_SIZE - 100 + i;
    int x = 28 + i;
    int y = 35 - (cpu_hist[hist_idx] * 20) / 100; // max height 20
    ssd1306_draw_line(oled, x, y, x, y);
  }
  ssd1306_draw_line(oled, 0, 37, 127, 37);

  // Draw RAM Graph
  snprintf(buf, sizeof(buf), "RAM: %d%%",
           ram_hist[PERFORMANCE_HISTORY_SIZE - 1]);
  my_draw_small_string(oled, 0, 41, buf);
  for (int i = 0; i < 100; i++) {
    int hist_idx = PERFORMANCE_HISTORY_SIZE - 100 + i;
    int x = 28 + i;
    int y = 62 - (ram_hist[hist_idx] * 20) / 100; // max height 20
    ssd1306_draw_line(oled, x, y, x, y);
  }

  ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
}

void MenuRender_Task(void *pvParameters) {
  DataManager_t *data = (DataManager_t *)pvParameters;
  ESP_LOGI(TAG_SCREEN_MANAGER, "MenuRender_Task started");
  while (1) {
    if (data->screen.is_dashboard_active) {
      if (data->screen.dashboard_page == 0) {
        ScreenDashboard(data);
      } else if (data->screen.dashboard_page == 1) {
        ScreenSensors(data);
      } else if (data->screen.dashboard_page == 2) {
        ScreenPerformance(data);
      } else {
        ScreenDashboard(data);
      }
    } else if (data->screen.is_menu_active && data->screen.current != NULL) {
      Draw_Menu_Frame(data->screen.current, &data->screen.selected,
                      &data->objectInfo);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // 20 FPS
  }
}

/**
 * @brief Animated “Connecting to WiFi…” line while STA is disconnected.
 */
system_err_t ScreenWifiConnecting(DataManager_t *data) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER,
             "ScreenWifiConnecting: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }

  if (data == NULL) {
    ESP_LOGW(TAG_SCREEN_MANAGER, "ScreenWifiConnecting: data is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }

  static uint8_t dot_count = 0;
  const char *base_message = "Connecting to WiFi";
  char display_buffer[30];
  if (data->objectInfo.wifiInfo.wifiStatus == DISCONNECTED ||
      data->objectInfo.wifiInfo.wifiStatus == ERROR) {
    strcpy(display_buffer, base_message);
    for (int i = 0; i < (dot_count % 4); i++) {
      strcat(display_buffer, ".");
    }
    dot_count++;
    ssd1306_clear_screen(oled, 0);
    ssd1306_draw_string(oled, 0, 0, (uint8_t *)display_buffer, 12, 1);

    esp_err_t ret = ssd1306_refresh_gram(oled);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_SCREEN_MANAGER,
               "ScreenWifiConnecting: refresh_gram failed: %s",
               esp_err_to_name(ret));
      if (oled_mutex != NULL)
        xSemaphoreGive(oled_mutex);
      return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
    }
    if (oled_mutex != NULL)
      xSemaphoreGive(oled_mutex);
    return MRS_OK;
  } else {
    ESP_LOGW(TAG_SCREEN_MANAGER,
             "ScreenWifiCallback: wifi status is not DISCONNECTED or ERROR");
    if (oled_mutex != NULL)
      xSemaphoreGive(oled_mutex);
    return MRS_ERR_NETWORK_INVALID_CONFIG;
  }
}

/**
 * @brief Status line while mesh child attempts to reach root IP.
 */
system_err_t ScreenShowMeshInformation(DataManager_t *data) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER,
             "ScreenShowMeshInformation: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  static uint8_t dot_count = 0;
  char display_buffer[100];
  if (data->objectInfo.meshInfo.meshStatus == DISCONNECTED ||
      data->objectInfo.meshInfo.meshStatus == ERROR) {
    const char *ip = (data->objectInfo.meshInfo.ipRoot != NULL)
                         ? data->objectInfo.meshInfo.ipRoot
                         : "?";
    snprintf(display_buffer, sizeof(display_buffer),
             "Connecting to Root IP: %s", ip);
    for (int i = 0; i < (dot_count % 4); i++) {
      strcat(display_buffer, ".");
    }
    dot_count++;
    ssd1306_clear_screen(oled, 0);
    ssd1306_draw_string(oled, 0, 0, (uint8_t *)display_buffer, 12, 1);

    esp_err_t ret = ssd1306_refresh_gram(oled);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_SCREEN_MANAGER,
               "ScreenWifiConnecting: refresh_gram failed: %s",
               esp_err_to_name(ret));
      if (oled_mutex != NULL)
        xSemaphoreGive(oled_mutex);
      return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
    }
    if (oled_mutex != NULL)
      xSemaphoreGive(oled_mutex);
    return MRS_OK;
  } else {
    ESP_LOGW(
        TAG_SCREEN_MANAGER,
        "ScreenShowMeshInformation: mesh status is not DISCONNECTED or ERROR");
    if (oled_mutex != NULL)
      xSemaphoreGive(oled_mutex);
    return MRS_ERR_NETWORK_INVALID_CONFIG;
  }
  if (oled_mutex != NULL)
    xSemaphoreGive(oled_mutex);
  return MRS_OK;
}

/**
 * @brief Show a short user message (validation / errors) full-screen one line.
 */
system_err_t ScreenShowMessage(Message_t message) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "ScreenShowMessage: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }

  if (message < 0 || message > MESSAGE_PORT_NOT_SELECTED) {
    ESP_LOGW(TAG_SCREEN_MANAGER, "ScreenShowMessage: invalid message type: %d",
             message);
    return MRS_ERR_UI_INVALID_MENU;
  }

  if (MessageText[message] == NULL) {
    ESP_LOGW(TAG_SCREEN_MANAGER,
             "ScreenShowMessage: message text is NULL for message: %d",
             message);
    return MRS_ERR_UI_INVALID_MENU;
  }

  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }

  ssd1306_clear_screen(oled, 0);
  ssd1306_draw_string(oled, 0, 0, (uint8_t *)MessageText[message], 12, 1);

  esp_err_t ret = ssd1306_refresh_gram(oled);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "ScreenShowMessage: refresh_gram failed: %s",
             esp_err_to_name(ret));
    if (oled_mutex != NULL)
      xSemaphoreGive(oled_mutex);
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  if (oled_mutex != NULL)
    xSemaphoreGive(oled_mutex);
  return MRS_OK;
}

/**
 * @brief Draw several lines on the OLED (e.g. Information screen), 12 px per
 * line.
 */
system_err_t ScreenShowInformation(const char **lines, size_t n_lines) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER,
             "ScreenShowInformation: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }
  if (lines == NULL || n_lines == 0) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  ssd1306_clear_screen(oled, 0);
  for (size_t i = 0; i < n_lines && i < 6; i++) {
    if (lines[i] != NULL) {
      ssd1306_draw_string(oled, 0, (int)(i * 12), (const uint8_t *)lines[i], 12,
                          1);
    }
  }
  esp_err_t ret = ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_SCREEN_MANAGER,
             "ScreenShowInformation: refresh_gram failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  return MRS_OK;
}

/**
 * @brief Cycle up to three sensor fields with large numeric font and units.
 */
system_err_t ScreenShowDataSensor(const char **field_names, const float *values,
                                  const char **units, size_t count) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER,
             "ScreenShowDataSensor: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }

  if (field_names == NULL || values == NULL || units == NULL || count == 0) {
    ESP_LOGW(TAG_SCREEN_MANAGER,
             "ScreenShowDataSensor: invalid arguments (fields=%p, values=%p, "
             "units=%p, count=%u)",
             field_names, values, units, (unsigned)count);
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }

  static size_t current_index = 0;

  // Rotate through fields (each ~300 ms slot in caller’s loop)
  size_t iterations = count < 3 ? count : 3; // at most 3 fields per pass
  for (size_t step = 0; step < iterations; ++step) {
    size_t idx = current_index % count;

    const char *name = field_names[idx] ? field_names[idx] : "Field";
    const char *unit = units[idx] ? units[idx] : "";
    float value = values[idx];

    ssd1306_clear_screen(oled, 0);

    // Field name (12px), centered
    {
      unsigned int name_px = strlen(name) * (12 / 2);
      int name_x = (int)((SSD1306_WIDTH - name_px) / 2);
      if (name_x < 0)
        name_x = 0;
      ssd1306_draw_string(oled, name_x, 0, (uint8_t *)name, 12, 1);
    }

    // Value: 32x16 digits; '.' and '-' in 12px font
    char value_buf[24];
    snprintf(value_buf, sizeof(value_buf), "%.2f", value);
    // Approximate width for centering: digit 16px, '.'/'-' 6px
    int total_px = 0;
    for (const char *p = value_buf; *p; ++p) {
      if (*p >= '0' && *p <= '9')
        total_px += 16;
      else
        total_px += 6;
    }
    int base_x = (SSD1306_WIDTH - total_px) / 2;
    if (base_x < 0)
      base_x = 0;

    int cursor_x = base_x;
    int cursor_y = 12; // below field name line
    for (const char *p = value_buf; *p; ++p) {
      if (*p >= '0' && *p <= '9') {
        ssd1306_draw_3216char(oled, (uint8_t)cursor_x, (uint8_t)cursor_y,
                              (uint8_t)*p);
        cursor_x += 16;
      } else {
        // small font for '.' or '-'
        char tmp[2] = {*p, '\0'};
        ssd1306_draw_string(oled, (uint8_t)cursor_x, (uint8_t)(cursor_y + 8),
                            (uint8_t *)tmp, 12, 1);
        cursor_x += 6;
      }
    }

    // Unit (12px), centered
    if (unit[0] != '\0') {
      unsigned int unit_px = (unsigned int)(strlen(unit) * (12 / 2));
      int unit_x = (int)((SSD1306_WIDTH - unit_px) / 2);
      if (unit_x < 0)
        unit_x = 0;
      // unit below large numeric value
      ssd1306_draw_string(oled, unit_x, cursor_y + 32, (uint8_t *)unit, 12, 1);
    }

    esp_err_t ret = ssd1306_refresh_gram(oled);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG_SCREEN_MANAGER,
               "ScreenShowDataSensor: refresh_gram failed: %s",
               esp_err_to_name(ret));
      if (oled_mutex != NULL)
        xSemaphoreGive(oled_mutex);
      return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(300));
    current_index = (current_index + 1) % count;
  }
  if (oled_mutex != NULL)
    xSemaphoreGive(oled_mutex);
  return MRS_OK;
}

system_err_t ScreenMeshRoot(DataManager_t *data) {
  if (oled == NULL) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "ScreenMeshRoot: oled is not initialized");
    return MRS_ERR_SCREENMANAGER_NOT_INIT;
  }
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (oled_mutex != NULL &&
      xSemaphoreTake(oled_mutex, portMAX_DELAY) != pdTRUE) {
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }

  const uint8_t node_count = MeshManager_GetConnectedNodeCount();
  UBaseType_t queue_used = 0;
  UBaseType_t queue_total = 0;
  if (data->meshIo.gateway_rx_queue != NULL) {
    queue_used = uxQueueMessagesWaiting(data->meshIo.gateway_rx_queue);
    queue_total =
        queue_used + uxQueueSpacesAvailable(data->meshIo.gateway_rx_queue);
  }

  char node_line[24];
  char queue_line[32];
  char stats_line[32];
  mesh_gateway_stats_t gateway_stats = {0};
  MeshManager_GetGatewayStats(&gateway_stats);
  snprintf(node_line, sizeof(node_line), "Nodes: %u", (unsigned)node_count);
  snprintf(queue_line, sizeof(queue_line), "Queue: %u/%u", (unsigned)queue_used,
           (unsigned)queue_total);
  snprintf(stats_line, sizeof(stats_line), "RX:%u Q:%u D:%u",
           (unsigned)gateway_stats.udp_rx_frames,
           (unsigned)gateway_stats.queued_frames,
           (unsigned)gateway_stats.dropped_frames);

  ssd1306_clear_screen(oled, 0);

  menu_list_t *menu = data->screen.current;
  const uint8_t *mesh_icon = imageManager[0][2];
  uint8_t icon_width = 32;
  uint8_t icon_height = 32;
  if (menu != NULL && menu->object == OBJECT_WIFI_MESH &&
      menu->image.image != NULL && menu->image.width > 0 &&
      menu->image.height > 0) {
    /* Mesh icon is imageManagerWifi[2], not imageManager[OBJECT_WIFI_MESH]. */
    mesh_icon = menu->image.image[0][2];
    icon_width = menu->image.width;
    icon_height = menu->image.height;
  }
  ssd1306_draw_bitmap(oled, 48, 0, (uint8_t *)mesh_icon, icon_width,
                      icon_height);

  ssd1306_draw_string(oled, 0, 24, (uint8_t *)node_line, 12, 1);
  ssd1306_draw_string(oled, 0, 38, (uint8_t *)queue_line, 12, 1);
  ssd1306_draw_string(oled, 0, 52, (uint8_t *)stats_line, 12, 1);

  esp_err_t ret = ssd1306_refresh_gram(oled);
  if (oled_mutex != NULL) {
    xSemaphoreGive(oled_mutex);
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_SCREEN_MANAGER, "ScreenMeshRoot: refresh_gram failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_SCREENMANAGER_DISPLAY_FAIL;
  }
  return MRS_OK;
}

/**
 * @brief Stub: dedicated per-port sensor page not implemented yet.
 *
 * @param port Unused.
 * @param data Unused.
 *
 * @return MRS_OK
 */
system_err_t SensorRender(PortId_t port, SensorData_t *data) {
  (void)port;
  (void)data;
  return MRS_OK;
}
