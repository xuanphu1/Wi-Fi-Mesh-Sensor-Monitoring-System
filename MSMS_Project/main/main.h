#include "BatteryManager.h"
#include "ButtonManager.h"
#include "DataManager.h"
#include "MenuSystem.h"
#include "ScreenManager.h"
#include "WifiManager.h"
#include "bme280.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "i2cdev.h"
#include "nvs_flash.h"
#include "ssd1306.h"

static const char *TAG_MAIN = "MAIN_PROJECT";
static ssd1306_handle_t MainScreen = NULL;
static DataManager_t DataManager = {0}; // static/global app context, version populated dynamically from PROJECT_VER in CMakeLists.txt
