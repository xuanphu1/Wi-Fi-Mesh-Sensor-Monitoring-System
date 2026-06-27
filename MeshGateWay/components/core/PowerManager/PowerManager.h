#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "Datamanager.h"

void power_manager_battery_adc_init(dm_hw_t *hw);

/**
 * @brief Read battery ADC and convert to percent using divider ratio.
 *
 * @param hw ADC runtime state (needs hw->adc_ready and hw->adc_chars initialized).
 * @param raw_avg_out Optional: average ADC raw value.
 * @param v_adc_mv_out Optional: average ADC voltage at ADC pin (after divider, mV).
 * @return battery percent in range 0..100
 */
int power_manager_battery_get_percent(dm_hw_t *hw, uint32_t *raw_avg_out, uint32_t *v_adc_mv_out);

/**
 * @brief Turn the screen power on or off using GPIO 2.
 * 
 * @param on true to turn on (GPIO high), false to turn off (GPIO low)
 */
void power_manager_screen_set_state(bool on);

#endif // POWER_MANAGER_H
