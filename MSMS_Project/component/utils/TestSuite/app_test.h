#ifndef APP_TEST_H
#define APP_TEST_H

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_MSMS_ENABLE_TESTS
/**
 * @brief Run all architectural verification scenarios.
 * @return Number of failed tests (0 = all passed).
 */
int app_run_all_tests(void);
#else
static inline int app_run_all_tests(void) {
  return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_TEST_H */
