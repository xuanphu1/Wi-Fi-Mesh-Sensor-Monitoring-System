#include <stdio.h>
#include <string.h>
#include "SD_Card.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

sdmmc_card_t *sdcard; // Toàn cục để hàm nào cũng dùng được
esp_err_t initSDCard(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // Theo cấu hình của bạn: SD dùng SPI host 2.
    // Trên ESP32-C3 cũng chỉ nên dùng SPI2_HOST (SPI3 thường invalid).
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || \
    defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    host.slot = SPI2_HOST;
#else
    host.slot = CONFIG_SPI_HOST_SDCARD;
#endif

    host.max_freq_khz = 4000; // giảm tốc độ để dễ bắt tay hơn

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_MOSI_SDCARD,
        .miso_io_num = CONFIG_MISO_SDCARD,
        .sclk_io_num = CONFIG_CLK_SDCARD,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    printf("[SD ] initSDCard: host.slot=%d (CS=%d, MISO=%d MOSI=%d CLK=%d)\n",
           host.slot, CONFIG_CS_SDCARD, CONFIG_MISO_SDCARD, CONFIG_MOSI_SDCARD, CONFIG_CLK_SDCARD);

    // Đảm bảo CS idle HIGH trước khi init/mount
    gpio_config_t cs_cfg = {
        .pin_bit_mask = 1ULL << CONFIG_CS_SDCARD,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_cfg);
    gpio_set_level(CONFIG_CS_SDCARD, 1);

    // DMA auto-alloc
    bool bus_initialized_by_us = false;
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK)
    {
        bus_initialized_by_us = true;
    }
    else if (ret == ESP_ERR_INVALID_STATE)
    {
        // LVGL/display có thể đã init bus trước đó. Cho phép reuse.
        printf("[SD ] spi bus already initialized, reuse it.\n");
        ret = ESP_OK;
    }
    else
    {
        printf("[SD ] Failed to initialize SPI bus. Err: 0x%x\n", ret);
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_CS_SDCARD;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sdcard);
    if (ret != ESP_OK)
    {
        printf("[SD ] Failed to mount filesystem. err: 0x%x\n", ret);
        if (bus_initialized_by_us)
        {
            spi_bus_free(host.slot);
        }
        return ret;
    }

    printf("[SD ] SD Card mounted successfully (host.slot=%d)\n", host.slot);
    sdmmc_card_print_info(stdout, sdcard);
    return ESP_OK;
}

esp_err_t writeSD_Card(const char *filename, const char *data)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(path, "w"); // Ghi đè file
    if (f == NULL)
    {
        printf("Failed to open file %s for writing\n", path);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    printf("Wrote to file %s\n", path);
    return ESP_OK;
}

esp_err_t writeFinalFileSD_Card(const char *filename, const char *data)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(path, "a+"); // Ghi đè file
    if (f == NULL)
    {
        printf("Failed to open file %s for writing\n", path);
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", data);
    fclose(f);
    // printf("Wrote to file %s\n", path);
    return ESP_OK;
}

esp_err_t readSD_Card(const char *filename, char *out_buf, size_t buf_size)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        printf("Failed to open file %s for reading\n", path);
        return ESP_FAIL;
    }
    if (fgets(out_buf, buf_size, f) == NULL)
    {
        printf("File %s is empty or error.\n", path);
        fclose(f);
        return ESP_FAIL;
    }
    fclose(f);
    printf("Read from file %s: %s\n", path, out_buf);
    return ESP_OK;
}

esp_err_t renameSD_Card(const char *oldname, const char *newname)
{
    char oldpath[128];
    char newpath[128];
    snprintf(oldpath, sizeof(oldpath), "%s/%s", MOUNT_POINT, oldname);
    snprintf(newpath, sizeof(newpath), "%s/%s", MOUNT_POINT, newname);

    // Kiểm tra file gốc có tồn tại không
    struct stat st;
    if (stat(oldpath, &st) != 0)
    {
        printf("File %s không tồn tại, không thể rename!\n", oldpath);
        return ESP_FAIL;
    }

    // Nếu file mới đã tồn tại thì xóa đi
    if (stat(newpath, &st) == 0)
    {
        printf("File đích %s đã tồn tại, sẽ bị xóa!\n", newpath);
        unlink(newpath);
    }

    // Đổi tên file
    if (rename(oldpath, newpath) != 0)
    {
        printf("Đổi tên file thất bại!\n");
        return ESP_FAIL;
    }

    printf("Đã đổi tên file từ %s sang %s\n", oldpath, newpath);
    return ESP_OK;
}

esp_err_t getSD_CardSpaceKB(uint32_t *total_kb, uint32_t *free_kb)
{
    if (total_kb == NULL || free_kb == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    esp_err_t ret = esp_vfs_fat_info(MOUNT_POINT, &total_bytes, &free_bytes);
    if (ret != ESP_OK)
    {
        return ret;
    }

    *total_kb = (uint32_t)(total_bytes / 1024ULL);
    *free_kb = (uint32_t)(free_bytes / 1024ULL);
    return ESP_OK;
}