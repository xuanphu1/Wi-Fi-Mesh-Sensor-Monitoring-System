#include "SystemPerfomance.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static uint8_t cpu_history[PERFORMANCE_HISTORY_SIZE];
static uint8_t ram_history[PERFORMANCE_HISTORY_SIZE];
static SemaphoreHandle_t perf_mutex;

// --- DUMMY TASK TO TEST CPU LOAD ---
// static void DummyHeavyTask(void *pvParameter)
// {
//     while (1) {
//         // Vòng lặp tính toán nặng để chiếm dụng CPU
//         volatile float x = 1.1;
//         for (int i = 0; i < 3000000; i++) {
//             x = x * 1.00001f;
//         }
//         // Nghỉ 10ms để tránh bị Watchdog (TWDT) reset thiết bị do chiếm CPU
//         quá lâu vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }
// -----------------------------------

static void SystemPerformance_Task(void *pvParameter) {
  uint32_t last_total_time = 0;
  uint32_t last_idle_time = 0;

  while (1) {
    if (xSemaphoreTake(perf_mutex, portMAX_DELAY) == pdTRUE) {
      // Shift history
      memmove(&cpu_history[0], &cpu_history[1], PERFORMANCE_HISTORY_SIZE - 1);
      memmove(&ram_history[0], &ram_history[1], PERFORMANCE_HISTORY_SIZE - 1);

      // CPU Load using FreeRTOS Run Time Stats
      uint32_t total_run_time = 0;
      UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
      TaskStatus_t *task_status_array =
          malloc(num_tasks * sizeof(TaskStatus_t));

      uint8_t cpu_load = 0;

      if (task_status_array != NULL) {
        num_tasks =
            uxTaskGetSystemState(task_status_array, num_tasks, &total_run_time);
        uint32_t idle_run_time = 0;
        uint32_t sum_run_time = 0;

        for (UBaseType_t i = 0; i < num_tasks; i++) {
          sum_run_time += task_status_array[i].ulRunTimeCounter;
          // IDLE tasks have name "IDLE" or "IDLE0" or "IDLE1" on multicore
          if (strncmp(task_status_array[i].pcTaskName, "IDLE", 4) == 0 ||
              strncmp(task_status_array[i].pcTaskName, "idle", 4) == 0) {
            idle_run_time += task_status_array[i].ulRunTimeCounter;
          }
        }

        if (sum_run_time > last_total_time) {
          uint32_t diff_total = sum_run_time - last_total_time;
          uint32_t diff_idle = idle_run_time - last_idle_time;

          if (diff_idle <= diff_total) {
            cpu_load = 100 - ((diff_idle * 100) / diff_total);
          } else {
            cpu_load = 0;
          }
        }

        last_total_time = sum_run_time;
        last_idle_time = idle_run_time;
        free(task_status_array);
      }

      cpu_history[PERFORMANCE_HISTORY_SIZE - 1] = cpu_load;

      // RAM Usage
      uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
      uint32_t free_heap = esp_get_free_heap_size();
      uint32_t used_heap = total_heap - free_heap;
      uint8_t ram_percent = 0;
      if (total_heap > 0) {
        ram_percent = (used_heap * 100) / total_heap;
      }
      ram_history[PERFORMANCE_HISTORY_SIZE - 1] = ram_percent;

      xSemaphoreGive(perf_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void SystemPerformance_Init(void) {
  perf_mutex = xSemaphoreCreateMutex();
  memset(cpu_history, 0, sizeof(cpu_history));
  memset(ram_history, 0, sizeof(ram_history));

  xTaskCreate(SystemPerformance_Task, "sys_perf", 2048, NULL, 5, NULL);

  // Khởi chạy Dummy Task ở mức ưu tiên 1 (thấp hơn sys_perf nhưng cao hơn IDLE)
  // xTaskCreate(DummyHeavyTask, "dummy_heavy", 2048, NULL, 1, NULL);
}

void SystemPerformance_GetCPUHistory(uint8_t *buffer) {
  if (buffer && xSemaphoreTake(perf_mutex, portMAX_DELAY) == pdTRUE) {
    memcpy(buffer, cpu_history, PERFORMANCE_HISTORY_SIZE);
    xSemaphoreGive(perf_mutex);
  }
}

void SystemPerformance_GetRAMHistory(uint8_t *buffer) {
  if (buffer && xSemaphoreTake(perf_mutex, portMAX_DELAY) == pdTRUE) {
    memcpy(buffer, ram_history, PERFORMANCE_HISTORY_SIZE);
    xSemaphoreGive(perf_mutex);
  }
}
