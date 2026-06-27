#include "MemoryManager.h"
#include "esp_heap_caps.h"

void memory_manager_get_usage(uint32_t *used_bytes, uint32_t *total_bytes, uint8_t *percent)
{
    uint32_t free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t used = total - free;

    if (used_bytes) {
        *used_bytes = used;
    }
    if (total_bytes) {
        *total_bytes = total;
    }
    if (percent) {
        if (total > 0) {
            *percent = (uint8_t)((used * 100) / total);
        } else {
            *percent = 0;
        }
    }
}
