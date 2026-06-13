#ifndef __MENU_RENDER_H__
#define __MENU_RENDER_H__

#include "BitManager.h"
#include "DataManager.h"
#include "ErrorCodes.h"
#include "WifiManager.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "string.h"

#define TAG_SCREEN_MANAGER "SCREEN_MANAGER"

/**
 * @file ScreenManager.h
 * @brief SSD1306 menu rendering, dialogs, and sensor value display.
 */

/**
 * @brief Take ownership of the SSD1306 handle, create OLED mutex, show splash.
 *
 * @param _oled Pointer to global display handle (non-NULL after creation).
 *
 * @return MRS_OK or MRS_ERR_SCREENMANAGER_* .
 */
system_err_t ScreenManagerInit(ssd1306_handle_t *_oled);

/**
 * @brief Draw the current menu page (icons, title text, paginated items) with mutex protection.
 *
 * @param menu Current menu_list_t.
 * @param selected In/out highlighted item index.
 * @param objectInfo Wi‑Fi / battery / mesh state for header widgets.
 *
 * @return MRS_OK or display error code.
 */
system_err_t MenuRender(menu_list_t *menu, int8_t *selected,
                        objectInfoManager_t *objectInfo);

/**
 * @brief Reserved single-port sensor screen; current implementation is a no-op stub.
 *
 * @param port Logical port id (unused).
 * @param data Last SensorData_t sample (unused).
 *
 * @return MRS_OK
 */
system_err_t SensorRender(PortId_t port, SensorData_t *data);

/**
 * @brief Animated “Connecting to WiFi…” line while STA is disconnected.
 *
 * @param data App context with wifiInfo.
 *
 * @return MRS_OK while animating; error if OLED or state invalid.
 */
system_err_t ScreenWifiConnecting(DataManager_t *data);

/**
 * @brief Show a short user message (validation / errors) full-screen one line.
 *
 * @param message Enum index into MessageText[].
 *
 * @return MRS_OK or UI error.
 */
system_err_t ScreenShowMessage(Message_t message);

/**
 * @brief Draw several lines on the OLED (e.g. Information screen), 12 px per line.
 *
 * @param lines Array of UTF-8 / ASCII C strings.
 * @param n_lines Number of entries in lines.
 *
 * @return MRS_OK or display error.
 */
system_err_t ScreenShowInformation(const char **lines, size_t n_lines);

/**
 * @brief Cycle up to three sensor fields with large numeric font and units.
 *
 * @param field_names Driver description[] labels.
 * @param values data_fl[] values.
 * @param units Driver unit[] strings.
 * @param count Number of fields.
 *
 * @return MRS_OK or display error.
 */
system_err_t ScreenShowDataSensor(const char **field_names, const float *values,
                                  const char **units, size_t count);

/**
 * @brief Status line while mesh child attempts to reach root IP.
 *
 * @param data App context with meshInfo.
 *
 * @return MRS_OK while showing progress; error if OLED invalid.
 */

system_err_t ScreenMeshRoot(DataManager_t *data);

system_err_t ScreenShowMeshInformation(DataManager_t *data);

#endif
