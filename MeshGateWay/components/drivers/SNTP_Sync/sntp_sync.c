#include "sntp_sync.h"


void sntp_init_func()
{
    ESP_LOGI(__func__, "Initializing SNTP.");
    setenv("TZ", "ICT-7", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "vn.pool.ntp.org");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_init();
}


esp_err_t sntp_setTime(struct tm *timeInfo, time_t *timeNow)
{
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
    {
        ESP_LOGI(__func__, "Waiting for system time to be set...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // POSIX TZ uses the opposite sign: ICT-7 means UTC+7 for Viet Nam.
    setenv("TZ", "ICT-7", 1);
    tzset();

    time(timeNow);
    localtime_r(timeNow, timeInfo);

    char timeString[64];
    strftime(timeString, sizeof(timeString), "%c", timeInfo);
    ESP_LOGI(__func__, "The current date/time in Viet Nam is: %s ", timeString);
    return ESP_OK;
}
