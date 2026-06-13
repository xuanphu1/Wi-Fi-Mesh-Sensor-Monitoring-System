#ifndef __MENU_H__
#define __MENU_H__

#include "esp_log.h"
#include "ssd1306.h"
#include "Common.h"
#include "string.h"
#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ButtonManager.h"
#include <sys/param.h>
#include "FunctionManager.h"
#include "WifiManager.h"
#include "BitManager.h"
#include "ScreenManager.h"

#define TAG_MENU_SYSTEM "MENU_SYSTEM"


void MenuSystemInit(DataManager_t *data);
void MenuNavigation_Task(void *pvParameter);
void ReadSensor_Task(void *pvParameter);

// SensorSelection được cấp phát động, không cần extern declaration
// Sử dụng sensor_registry_get_count() để lấy số lượng sensor thực tế
#endif
