#ifndef __BUTTON_MANAGER_H__
#define __BUTTON_MANAGER_H__

#include "stdint.h"
#include "DataManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG_BUTTON_MANAGER "BUTTON_MANAGER"

/**
 * @file ButtonManager.h
 * @brief Target-specific keypad driver.
 *
 * ESP32-C3/C6: one ADC resistor ladder decoded by configured raw ranges.
 * ESP32: three direct GPIO inputs: GPIO2=DOWN, GPIO35=SEL, GPIO39=BACK.
 */

/**
 * @brief Initialize button input for the active target.
 */
void ButtonManagerInit(void);

/**
 * @brief Edge-detect one button press.
 *
 * @return Button enum, or BTN_NONE when idle/no new press.
 */
button_type_t ReadButtonStatus(void);

#endif
