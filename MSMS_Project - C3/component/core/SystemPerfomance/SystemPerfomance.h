#ifndef SYSTEM_PERFORMANCE_H
#define SYSTEM_PERFORMANCE_H

#include <stdint.h>

#define PERFORMANCE_HISTORY_SIZE 128

/**
 * @brief Initialize the System Performance monitoring task.
 */
void SystemPerformance_Init(void);

/**
 * @brief Get the history of CPU load.
 * 
 * @param buffer Array of size PERFORMANCE_HISTORY_SIZE to copy the data into.
 */
void SystemPerformance_GetCPUHistory(uint8_t *buffer);

/**
 * @brief Get the history of RAM usage.
 * 
 * @param buffer Array of size PERFORMANCE_HISTORY_SIZE to copy the data into.
 * The data is scaled to 0-100%.
 */
void SystemPerformance_GetRAMHistory(uint8_t *buffer);

#endif /* SYSTEM_PERFORMANCE_H */
