#include "PowerManager.h"

void power_manager_battery_adc_init(dm_hw_t *hw)
{
    if (hw == NULL)
        return;

    if (adc1_config_width(BAT_ADC_WIDTH) != ESP_OK)
        return;
    if (adc1_config_channel_atten(BAT_ADC_CHANNEL, BAT_ADC_ATTEN) != ESP_OK)
        return;

    esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN, BAT_ADC_WIDTH, 1100, &hw->adc_chars);
    hw->adc_ready = true;
}

int power_manager_battery_get_percent(dm_hw_t *hw, uint32_t *raw_avg_out, uint32_t *v_adc_mv_out)
{
    if (raw_avg_out) *raw_avg_out = 0;
    if (v_adc_mv_out) *v_adc_mv_out = 0;

    if (!hw || !hw->adc_ready)
        return 0;

    const int samples = 8;
    uint32_t sum_raw = 0;
    uint32_t sum_mv = 0;
    for (int i = 0; i < samples; i++)
    {
        int raw = adc1_get_raw(BAT_ADC_CHANNEL);
        sum_raw += (uint32_t)raw;
        uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &hw->adc_chars);
        sum_mv += mv;
    }

    uint32_t raw_avg = sum_raw / samples;
    uint32_t v_adc_mv_raw = sum_mv / samples;
    uint32_t v_adc_mv = (uint32_t)((uint64_t)v_adc_mv_raw * BAT_ADC_MV_SCALE_NUM / BAT_ADC_MV_SCALE_DEN);
    if (raw_avg_out) *raw_avg_out = raw_avg;
    // Trả ra mV đã calib để bạn nhìn đúng theo % đang tính
    if (v_adc_mv_out) *v_adc_mv_out = v_adc_mv;

    // Compute ADC-pin voltage thresholds (divider already applied)
    const int32_t vmin_adc_mv =
        (int32_t)((int64_t)BAT_VMIN_MV * BAT_R_BOTTOM_OHMS / (BAT_R_TOP_OHMS + BAT_R_BOTTOM_OHMS));
    const int32_t vmax_adc_mv =
        (int32_t)((int64_t)BAT_VMAX_MV * BAT_R_BOTTOM_OHMS / (BAT_R_TOP_OHMS + BAT_R_BOTTOM_OHMS));

    int32_t pct = 0;
    if (vmax_adc_mv > vmin_adc_mv)
        pct = (int32_t)((((int32_t)v_adc_mv - vmin_adc_mv) * 100) / (vmax_adc_mv - vmin_adc_mv));

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (int)pct;
}
