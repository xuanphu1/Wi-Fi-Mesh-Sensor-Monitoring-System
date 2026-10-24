#include "BatteryManager.h"
#include "ButtonManager.h"
#include "DataManager.h"
#include "MenuSystem.h"
#include "ScreenManager.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "i2cdev.h"
#include "nvs_flash.h"

static const char *TAG_MAIN = "MAIN_PROJECT";
static DataManager_t DataManager = {0}; // static/global app context, version populated dynamically from PROJECT_VER in CMakeLists.txt
