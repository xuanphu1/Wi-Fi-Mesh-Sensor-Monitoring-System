#include "DataManager.h"
#include "ButtonManager.h"
#include "MenuSystem.h"
#include "ScreenManager.h"
#include "WifiManager.h"
#include "BatteryManager.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "nvs_flash.h"
#include "bme280.h"
#include "i2cdev.h"

static const char *TAG_MAIN = "MAIN_PROJECT";
ssd1306_handle_t MainScreen = NULL;
static DataManager_t DataManager = {.version = {0, 0, 1}}; // static/global app context (or use malloc)

