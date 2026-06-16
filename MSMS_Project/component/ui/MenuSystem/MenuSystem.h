#ifndef __MENU_H__
#define __MENU_H__

#include "esp_log.h"
#include "ssd1306.h"
#include "DataManager.h"
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

/**
 * @file MenuSystem.h
 * @brief Build the menu tree from SensorRegistry and run the navigation FreeRTOS task.
 */

/**
 * @brief Wire all menus, dynamic sensor submenus, and callbacks into DataManager.
 *
 * @param data Global app context (must not be NULL).
 */
void MenuSystemInit(DataManager_t *data);

/**
 * @brief Poll ADC keys, update selection, invoke menu callbacks, redraw OLED.
 *
 * @param pvParameter DataManager_t * as void *.
 */
void MenuNavigation_Task(void *pvParameter);

/**
 * @brief Reserved periodic sensor read task hook (not used in current firmware).
 *
 * @param pvParameter Intended DataManager_t * or task params.
 */
void ReadSensor_Task(void *pvParameter);

#endif
