#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdint.h>

/**
 * @brief Get the system memory (RAM) usage.
 * 
 * @param used_bytes Pointer to store the number of used bytes (can be NULL).
 * @param total_bytes Pointer to store the total number of bytes (can be NULL).
 * @param percent Pointer to store the percentage of used memory (0-100) (can be NULL).
 */
void memory_manager_get_usage(uint32_t *used_bytes, uint32_t *total_bytes, uint8_t *percent);

#endif // MEMORY_MANAGER_H
