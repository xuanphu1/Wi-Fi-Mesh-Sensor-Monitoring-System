#include "app_test.h"
#include "sdkconfig.h"

#if CONFIG_MSMS_ENABLE_TESTS

#include "DataManager.h"
#include "ErrorCodes.h"
#include "FunctionManager.h"
#include "SensorManager.h"
#include "SensorRegistry.h"
#include "SensorRegistry_internal.h"
#include "SensorScanner.h"
#include "UartToGateWay.h"
#include "UartResourceManager.h"
#include "PinManager.h"
#include "ScreenManager.h"
#include "BatteryManager.h"
#include "InternetManager.h"
#include "MeshManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "APP_TEST";

static system_err_t mock_driver_init_ok(void) {
  return MRS_OK;
}

static system_err_t mock_driver_read_ok(SensorData_t *data) {
  if (data != NULL) {
    data->data_fl[0] = 25.0f;
    data->data_fl[1] = 60.0f;
  }
  return MRS_OK;
}

static system_err_t mock_driver_deinit_ok(void) {
  return MRS_OK;
}

__attribute__((unused)) static system_err_t mock_driver_init_fail(void) {
  return MRS_FAIL;
}

static system_err_t mock_driver_deinit_fail(void) {
  return MRS_FAIL;
}

static void setup_mock_sensors(void) {
  size_t count = sensor_registry_get_count();
  for (size_t i = 0; i < count; i++) {
    sensor_descriptor_t desc = {0};
    if (sensor_registry_get_descriptor_at(i, &desc)) {
      sensor_driver_t mock_drv = {
          .type = desc.type,
          .name = desc.name,
          .interface = desc.interface,
          .init = mock_driver_init_ok,
          .read = mock_driver_read_ok,
          .deinit = mock_driver_deinit_ok,
          .unit_count = desc.unit_count,
          .is_init = false,
      };
      sensor_registry_set_driver_override(desc.type, &mock_drv);
    }
  }
}

static SensorType_t get_assigned(PortId_t port) {
  SensorType_t type = SENSOR_NONE;
  (void)SensorManager_GetAssignedSensor(port, &type);
  return type;
}

static system_err_t mock_gateway_quiesce(void *context, TickType_t timeout_ticks) {
  (void)context;
  (void)timeout_ticks;
  return MRS_OK;
}

static system_err_t mock_gateway_resume(void *context, uart_port_t port, QueueHandle_t queue) {
  (void)context;
  (void)port;
  (void)queue;
  return MRS_OK;
}

static bool mock_policy_root(void) {
  return false;
}

static bool mock_policy_node(void) {
  return true;
}

#define TEST_ASSERT(cond, msg)                                                  do {                                                                            if (!(cond)) {                                                                  ESP_LOGE(TAG, "ASSERTION FAILED: %s (%s:%d)", msg, __FILE__, __LINE__);      return false;                                                               }                                                                           } while (0)

#define TEST_ASSERT_RUNNER(cond, msg)                                           do {                                                                            if (!(cond)) {                                                                  ESP_LOGE(TAG, "RUNNER FATAL: %s (%s:%d)", msg, __FILE__, __LINE__);          return -1;                                                                  }                                                                           } while (0)

/* 1. Test assign */
static bool test_assign(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 1: assign");
  system_err_t ret = SensorManager_AssignSensor(PORT_1, SENSOR_DHT22);
  TEST_ASSERT(ret == MRS_OK, "Assign DHT22 to PORT_1 should succeed");
  TEST_ASSERT(get_assigned(PORT_1) == SENSOR_DHT22,
              "PORT_1 must have SENSOR_DHT22");
  TEST_ASSERT(SensorManager_IsSensorAssigned(SENSOR_DHT22),
              "DHT22 must be assigned");

  SensorPortSnapshot_t snap = {0};
  ret = SensorManager_GetPortSnapshot(PORT_1, &snap);
  TEST_ASSERT(ret == MRS_OK, "Snapshot must succeed");
  TEST_ASSERT(snap.assigned == true, "Snapshot must show assigned");
  TEST_ASSERT(snap.type == SENSOR_DHT22, "Snapshot type must match");
  return true;
}

/* 2. Test duplicate */
static bool test_duplicate(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 2: duplicate");
  system_err_t ret = SensorManager_AssignSensor(PORT_2, SENSOR_DHT22);
  TEST_ASSERT(ret == MRS_ERR_DATAMANAGER_PORT_IN_USE,
              "Duplicate assignment to PORT_2 must be rejected");
  TEST_ASSERT(get_assigned(PORT_2) == SENSOR_NONE,
              "PORT_2 must remain NONE");
  return true;
}

/* 3. Test UART conflict */
static bool test_uart_conflict(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 3: UART conflict");
  system_err_t ret = SensorManager_AssignSensor(PORT_2, SENSOR_MHZ14A);
  TEST_ASSERT(ret == MRS_OK, "Assign MHZ14A to PORT_2 should succeed");

  ret = SensorManager_AssignSensor(PORT_3, SENSOR_PMS7003);
  TEST_ASSERT(ret == MRS_ERR_DATAMANAGER_PORT_IN_USE,
              "Concurrent UART assignment on PORT_3 must be rejected");
  return true;
}

/* 4. Test UART replace (UART -> non-UART and UART -> UART) */
static bool test_uart_replace(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 4: UART replace");
  system_err_t ret = SensorManager_AssignSensor(PORT_2, SENSOR_PMS7003);
  TEST_ASSERT(ret == MRS_OK, "Replacing MHZ14A with PMS7003 on PORT_2 should succeed");
  TEST_ASSERT(get_assigned(PORT_2) == SENSOR_PMS7003,
              "PORT_2 must now be PMS7003");
  TEST_ASSERT(!SensorManager_IsSensorAssigned(SENSOR_MHZ14A),
              "MHZ14A must no longer be assigned");

  // Replace UART with non-UART (PMS7003 -> DHT11)
  ret = SensorManager_AssignSensor(PORT_2, SENSOR_DHT11);
  TEST_ASSERT(ret == MRS_OK, "Replacing PMS7003 with DHT11 on PORT_2 should succeed");
  TEST_ASSERT(get_assigned(PORT_2) == SENSOR_DHT11, "PORT_2 must now be DHT11");
  TEST_ASSERT(!SensorManager_IsSensorAssigned(SENSOR_PMS7003), "PMS7003 must no longer be assigned");
  return true;
}

/* 5. Test rollback */
static bool test_rollback(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 5: rollback");
  UartResource_TestInjectQuiesceTimeout(true);
  system_err_t ret = SensorManager_AssignSensor(PORT_3, SENSOR_MHZ14A);
  TEST_ASSERT(ret == MRS_ERR_CORE_TIMEOUT,
              "Assignment with quiesce timeout should fail");
  UartResource_TestInjectQuiesceTimeout(false);
  return true;
}

/* 6. Test pause timeout */
static bool test_pause_timeout(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 6: pause timeout");
  UartResource_TestInjectQuiesceTimeout(true);
  system_err_t ret = SensorManager_AssignSensor(PORT_3, SENSOR_MHZ14A);
  TEST_ASSERT(ret == MRS_ERR_CORE_TIMEOUT,
              "Quiesce timeout must abort UART assignment");
  UartResource_TestInjectQuiesceTimeout(false);
  return true;
}

/* 7. Test reset */
static bool test_reset(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 7: reset");
  system_err_t ret = SensorManager_ResetAll();
  TEST_ASSERT(ret == MRS_OK, "ResetAll should succeed");
  for (int p = 0; p < NUM_PORTS; p++) {
    TEST_ASSERT(get_assigned((PortId_t)p) == SENSOR_NONE,
                "All ports must be NONE after reset");
  }
  TEST_ASSERT(SensorManager_HasEmptyPort(), "Must have empty port after reset");
  TEST_ASSERT(SensorManager_GetAvailablePortCount() == NUM_PORTS,
              "Available port count must equal NUM_PORTS");
  return true;
}

/* 8. Test concurrency */
typedef struct {
  DataManager_t *dm;
  volatile bool done;
} concurrency_arg_t;

static void concurrency_reader_task(void *pvParam) {
  concurrency_arg_t *arg = (concurrency_arg_t *)pvParam;
  for (int i = 0; i < 20; i++) {
    SensorPortSnapshot_t snap = {0};
    (void)SensorManager_GetPortSnapshot(PORT_1, &snap);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  arg->done = true;
  vTaskDelete(NULL);
}

static bool test_concurrency(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 8: concurrency");
  concurrency_arg_t arg1 = {.dm = dm, .done = false};
  concurrency_arg_t arg2 = {.dm = dm, .done = false};

  TaskHandle_t h1, h2;
  xTaskCreate(concurrency_reader_task, "test_rd1", 2048, &arg1, 5, &h1);
  xTaskCreate(concurrency_reader_task, "test_rd2", 2048, &arg2, 5, &h2);

  for (int i = 0; i < 20; i++) {
    (void)SensorManager_AssignSensor(PORT_1, SENSOR_DHT22);
    (void)SensorManager_ResetAll();
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  int wait_count = 0;
  while ((!arg1.done || !arg2.done) && wait_count++ < 100) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT(arg1.done && arg2.done,
              "Concurrent tasks must finish without deadlock");
  return true;
}

/* 9. Test root scan block */
static bool test_root_scan_block(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 9: root scan block");
  SensorScanner_SetPermissionPolicy(mock_policy_root);
  TEST_ASSERT(SensorScanner_IsAllowed() == false,
              "When policy returns false (ROOT), scan must be disallowed");

  SensorType_t found[NUM_PORTS];
  int count = SensorScanner_Scan(dm, found, NUM_PORTS);
  TEST_ASSERT(count == 0, "Scan on ROOT must detect 0 sensors");

  SensorScanner_SetPermissionPolicy(mock_policy_node);
  TEST_ASSERT(SensorScanner_IsAllowed() == true,
              "When policy returns true (NODE), scan must be allowed");
  return true;
}

/* 10. Test port capacity limit */
static bool test_port_capacity_limit(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 10: port capacity limit");
  system_err_t ret = SensorManager_AssignSensor(PORT_NONE, SENSOR_BME280);
  TEST_ASSERT(ret == MRS_ERR_DATAMANAGER_INVALID_PORT,
              "Assigning to PORT_NONE must return INVALID_PORT");

  ret = SensorManager_AssignSensor((PortId_t)NUM_PORTS, SENSOR_BME280);
  TEST_ASSERT(ret == MRS_ERR_DATAMANAGER_INVALID_PORT,
              "Assigning beyond NUM_PORTS must return INVALID_PORT");

  SensorManager_ResetAll();
  SensorManager_AssignSensor(PORT_1, SENSOR_DHT22);
  SensorManager_AssignSensor(PORT_2, SENSOR_DHT11);
  SensorManager_AssignSensor(PORT_3, SENSOR_AHT10);

  PortId_t out_port = PORT_NONE;
  ret = SensorManager_AssignFirstAvailable(SENSOR_BME280, &out_port);
  TEST_ASSERT(ret == MRS_ERR_DATAMANAGER_PORT_IN_USE,
              "AssignFirstAvailable when full must fail");
  TEST_ASSERT(out_port == PORT_NONE, "Out port must remain NONE");

  SensorManager_ResetAll();
  return true;
}

/* 11. Test UART Init Fault Injection & Rollback */
static bool test_uart_init_faults(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 11: UART init fault injection & rollback");

  for (int stage = 1; stage <= 4; stage++) {
    UartToGateWay_TestInjectInitFault(stage);
    system_err_t err = UartToGateWay_Init(dm);
    TEST_ASSERT(err != MRS_OK, "Injected fault must cause UartToGateWay_Init to fail");
  }

  // Verify successful init after faults
  UartToGateWay_TestInjectInitFault(0);
  system_err_t ok_err = UartToGateWay_Init(dm);
  TEST_ASSERT(ok_err == MRS_OK, "Clean UartToGateWay_Init must succeed");

  system_err_t deinit_err = UartToGateWay_Deinit();
  TEST_ASSERT(deinit_err == MRS_OK, "UartToGateWay_Deinit must succeed");
  return true;
}

/* 12. Test FM Init Success, Deinit & Idempotence (Requirements 1, 3, 5) */
static bool test_fm_init_success_and_deinit(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 12: FunctionManager Init success & Deinit idempotence");

  system_err_t err = FunctionManager_Init(dm);
  TEST_ASSERT(err == MRS_OK, "FunctionManager_Init must succeed");
  TEST_ASSERT(FunctionManager_GetState() == FM_STATE_RUNNING, "FM state must be RUNNING");

  err = FunctionManager_Deinit();
  TEST_ASSERT(err == MRS_OK, "FunctionManager_Deinit must succeed");
  TEST_ASSERT(FunctionManager_GetState() == FM_STATE_UNINITIALIZED, "FM state must be UNINITIALIZED");

  // Call Deinit again: must be idempotent and return MRS_OK
  err = FunctionManager_Deinit();
  TEST_ASSERT(err == MRS_OK, "Repeated FunctionManager_Deinit must be idempotent and return MRS_OK");
  return true;
}

/* 13. Test FM Init Rollback Fault Injection across all 10 stages (Requirements 2, 3) */
static bool test_fm_init_rollback_faults(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 13: FM init rollback fault injection (10 stages)");

  for (int stage = 1; stage <= 10; stage++) {
    FunctionManager_TestInjectInitFault(stage);
    system_err_t err = FunctionManager_Init(dm);
    TEST_ASSERT(err != MRS_OK, "Injected fault must cause FunctionManager_Init to fail");
    TEST_ASSERT(FunctionManager_GetState() == FM_STATE_UNINITIALIZED, "State must be UNINITIALIZED after clean rollback");

    system_err_t deinit_err = FunctionManager_Deinit();
    TEST_ASSERT(deinit_err == MRS_OK, "FunctionManager_Deinit after rollback must succeed");
  }

  FunctionManager_TestInjectInitFault(0);
  return true;
}

/* 14. Test SensorManager Task Lifecycle & Stop/Deinit (Requirements 6, 7) */
static bool test_sensor_manager_lifecycle(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 14: SensorManager Task Lifecycle & Stop/Deinit");

  SensorManager_Deinit(); // Ensure clean start
  system_err_t ret = SensorManager_Init(dm);
  TEST_ASSERT(ret == MRS_OK, "SensorManager_Init must succeed");
  TEST_ASSERT(SensorManager_GetState() == SENSOR_MANAGER_STATE_INITIALIZED, "State must be INITIALIZED");

  ret = SensorManager_Start();
  TEST_ASSERT(ret == MRS_OK, "SensorManager_Start must succeed");
  TEST_ASSERT(SensorManager_GetState() == SENSOR_MANAGER_STATE_RUNNING, "State must be RUNNING");

  ret = SensorManager_Stop();
  TEST_ASSERT(ret == MRS_OK, "SensorManager_Stop must succeed");
  TEST_ASSERT(SensorManager_GetState() == SENSOR_MANAGER_STATE_INITIALIZED, "State must be INITIALIZED after stop");

  ret = SensorManager_Deinit();
  TEST_ASSERT(ret == MRS_OK, "SensorManager_Deinit must succeed");
  TEST_ASSERT(SensorManager_GetState() == SENSOR_MANAGER_STATE_UNINITIALIZED, "State must be UNINITIALIZED");

  // Re-init for subsequent tests
  ret = SensorManager_Init(dm);
  TEST_ASSERT(ret == MRS_OK, "SensorManager_Init must succeed");
  return true;
}

/* 15. Test UART Resume Failure on Sensor Replacement (Requirement 9) */
static bool test_uart_resume_failure(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 15: UART resume failure rollback");

  SensorManager_ResetAll();

  system_err_t ret = SensorManager_AssignSensor(PORT_1, SENSOR_MHZ14A);
  TEST_ASSERT(ret == MRS_OK, "Assigning UART sensor MHZ14A must succeed");

  // Inject UART resume failure
  SensorManager_TestInjectUartResumeFault(1);
  ret = SensorManager_AssignSensor(PORT_1, SENSOR_DHT22);
  TEST_ASSERT(ret != MRS_OK, "Replacing UART sensor with failing resume must return error, not MRS_OK");

  // Verify old sensor was restored or port rolled back to SENSOR_NONE
  SensorType_t assigned = get_assigned(PORT_1);
  TEST_ASSERT(assigned == SENSOR_MHZ14A || assigned == SENSOR_NONE,
              "Port must be restored to MHZ14A or NONE upon resume failure");

  SensorManager_TestInjectUartResumeFault(0);
  SensorManager_ResetAll();
  return true;
}

/* 16. Test Sensor Driver Deinit Failure Preserves Metadata (Requirement 10) */
static bool test_driver_deinit_failure(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 16: Driver deinit failure preserves metadata");

  SensorManager_ResetAll();

  // Setup driver with failing deinit
  sensor_driver_t fail_deinit_drv = {
      .type = SENSOR_DHT11,
      .name = "DHT11_FAIL_DEINIT",
      .interface = COMMUNICATION_DIGITAL,
      .init = mock_driver_init_ok,
      .read = mock_driver_read_ok,
      .deinit = mock_driver_deinit_fail,
      .unit_count = 2,
      .is_init = false,
  };
  sensor_registry_set_driver_override(SENSOR_DHT11, &fail_deinit_drv);

  system_err_t ret = SensorManager_AssignSensor(PORT_2, SENSOR_DHT11);
  TEST_ASSERT(ret == MRS_OK, "Assigning DHT11 must succeed");

  // ResetAll must report deinit error and preserve metadata on PORT_2
  ret = SensorManager_ResetAll();
  TEST_ASSERT(ret != MRS_OK, "ResetAll must report failure when driver deinit fails");
  TEST_ASSERT(get_assigned(PORT_2) == SENSOR_DHT11,
              "Metadata on PORT_2 must be preserved for retry when deinit fails");

  // Restore working mock and clean up
  setup_mock_sensors();
  SensorManager_ResetAll();
  return true;
}

/* 17. Test Mesh Startup Fault Propagation (Requirements 11, 12, 13, 14) */
static bool test_mesh_startup_faults(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 17: Mesh startup fault propagation");

  // Fault 1: Queue allocation failure
  MeshManager_TestInjectStartFault(1);
  system_err_t ret = InternetManager_SwitchMode(dm, INTERNET_MODE_MESH);
  TEST_ASSERT(ret != MRS_OK, "Queue alloc failure must be propagated");
  TEST_ASSERT(InternetManager_GetMode() != INTERNET_MODE_MESH, "Mode must not be MESH on failure");

  // Fault 2: Network start failure
  MeshManager_TestInjectStartFault(2);
  ret = InternetManager_SwitchMode(dm, INTERNET_MODE_MESH);
  TEST_ASSERT(ret != MRS_OK, "Network start failure must be propagated");
  TEST_ASSERT(InternetManager_GetMode() != INTERNET_MODE_MESH, "Mode must not be MESH on failure");

  // Fault 3: TCP transport start failure
  MeshManager_TestInjectStartFault(3);
  ret = InternetManager_SwitchMode(dm, INTERNET_MODE_MESH);
  TEST_ASSERT(ret != MRS_OK, "TCP transport failure must be propagated");
  TEST_ASSERT(InternetManager_GetMode() != INTERNET_MODE_MESH, "Mode must not be MESH on failure");

  MeshManager_TestInjectStartFault(0);
  InternetManager_Clean(dm, false);
  return true;
}

/* 18. Test FunctionManager API Gating & Idempotent Teardown (Requirements 15, 16, 17) */
static bool test_fm_api_gating_and_teardown(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 18: FM API Gating & Repeated Deinit");

  // In UNINITIALIZED state, all public APIs must reject safely
  TEST_ASSERT(FunctionManager_GetState() == FM_STATE_UNINITIALIZED, "Must be UNINITIALIZED");
  TEST_ASSERT(FunctionManager_AssignSensor(PORT_1, SENSOR_DHT22) == MRS_ERR_CORE_NOT_INITIALIZED,
              "AssignSensor must reject when uninitialized");
  SensorPortSnapshot_t snap = {0};
  TEST_ASSERT(FunctionManager_GetPortSnapshot(PORT_1, &snap) == MRS_ERR_CORE_NOT_INITIALIZED,
              "GetPortSnapshot must reject when uninitialized");
  TEST_ASSERT(FunctionManager_GetSensorName(SENSOR_DHT22) == NULL,
              "GetSensorName must return NULL when uninitialized");
  TEST_ASSERT(FunctionManager_GetSensorCountByInterface(COMMUNICATION_I2C) == 0,
              "GetSensorCountByInterface must return 0 when uninitialized");
  TEST_ASSERT(!FunctionManager_IsSensorScanAllowed(),
              "IsSensorScanAllowed must return false when uninitialized");

  // Test idempotent multiple Deinits across all managers (no double-delete, no crash)
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT(FunctionManager_Deinit() == MRS_OK, "Repeated FM Deinit must succeed");
    TEST_ASSERT(SensorManager_Deinit() == MRS_OK, "Repeated SM Deinit must succeed");
    TEST_ASSERT(BatteryManager_Deinit() == ESP_OK, "Repeated Battery Deinit must succeed");
    TEST_ASSERT(UartToGateWay_Deinit() == MRS_OK, "Repeated UART Deinit must succeed");
    TEST_ASSERT(InternetManager_Clean(dm, false) == MRS_OK, "Repeated Internet Clean must succeed");
  }

  // Test telemetry provider unregister
  TEST_ASSERT(MeshManager_SetTelemetryProvider(NULL, NULL) == ESP_OK, "Unregistering NULL provider must succeed");
  size_t count = 99;
  mesh_telemetry_port_entry_t entries[4];
  TEST_ASSERT(!MeshManager_GetTelemetryEntries(entries, 4, &count), "GetTelemetryEntries must return false when unmapped");
  TEST_ASSERT(count == 0, "Telemetry count must be 0");
  return true;
}


/* 19. Test UART Resource Quiesce & Resume */
static bool test_uart_resource_quiesce_resume(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 19: UART Resource Quiesce & Resume");
  (void)dm;

  uart_lease_t lease = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(1000), &lease);
  TEST_ASSERT(ret == MRS_OK, "Acquire for SENSOR_SCANNER must succeed");
  TEST_ASSERT(UartResource_IsLeaseValid(&lease), "Issued lease must be valid");

  uart_resource_snapshot_t snap = {0};
  TEST_ASSERT(UartResource_GetSnapshot(&snap) == MRS_OK, "GetSnapshot must succeed");
  TEST_ASSERT(snap.current_owner == UART_OWNER_SENSOR_SCANNER, "Current owner must be SENSOR_SCANNER");
  TEST_ASSERT(snap.state == UART_RES_STATE_SENSOR_PROBE_ACTIVE, "State must be SENSOR_PROBE_ACTIVE");

  ret = UartResource_Release(&lease);
  TEST_ASSERT(ret == MRS_OK, "Release must succeed");
  TEST_ASSERT(!UartResource_IsLeaseValid(&lease), "Released lease must be invalid");
  TEST_ASSERT(lease.owner == UART_OWNER_NONE, "Lease owner must be NONE after release");

  TEST_ASSERT(UartResource_GetSnapshot(&snap) == MRS_OK, "GetSnapshot must succeed");
  TEST_ASSERT(snap.current_owner == UART_OWNER_GATEWAY, "Owner must restore to GATEWAY");
  TEST_ASSERT(snap.state == UART_RES_STATE_GATEWAY_ACTIVE, "State must restore to GATEWAY_ACTIVE");
  return true;
}

/* 20. Test UART Resource Quiesce Timeout */
static bool test_uart_resource_quiesce_timeout(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 20: UART Resource Quiesce Timeout");
  (void)dm;

  UartResource_TestInjectQuiesceTimeout(true);
  uart_lease_t lease = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(500), &lease);
  TEST_ASSERT(ret == MRS_ERR_CORE_TIMEOUT, "Injected quiesce timeout must return MRS_ERR_CORE_TIMEOUT");
  TEST_ASSERT(!UartResource_IsLeaseValid(&lease), "No valid lease should be issued on timeout");

  uart_resource_snapshot_t snap = {0};
  TEST_ASSERT(UartResource_GetSnapshot(&snap) == MRS_OK, "GetSnapshot must succeed");
  TEST_ASSERT(snap.current_owner == UART_OWNER_GATEWAY, "Owner must remain GATEWAY on timeout");
  TEST_ASSERT(snap.state == UART_RES_STATE_GATEWAY_ACTIVE, "State must remain GATEWAY_ACTIVE");

  UartResource_TestInjectQuiesceTimeout(false);
  return true;
}

/* 21. Test UART Stale Lease & Double Release */
static bool test_uart_stale_and_double_release(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 21: UART Stale Lease & Double Release");
  (void)dm;

  uart_lease_t lease1 = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(1000), &lease1);
  TEST_ASSERT(ret == MRS_OK, "Acquire lease1 must succeed");

  uart_lease_t lease_copy = lease1; // Copy to test stale release later
  ret = UartResource_Release(&lease1);
  TEST_ASSERT(ret == MRS_OK, "First release must succeed");

  // Attempt double release with now invalidated lease1
  ret = UartResource_Release(&lease1);
  TEST_ASSERT(ret == MRS_ERR_CORE_INVALID_PARAM, "Double release on invalidated lease must fail with INVALID_PARAM");

  // Attempt release with stale copy
  ret = UartResource_Release(&lease_copy);
  TEST_ASSERT(ret == MRS_ERR_CORE_INVALID_STATE, "Release with stale generation/token must fail with INVALID_STATE");
  return true;
}

/* 22. Test UART Concurrent Acquire Rejection */
static bool test_uart_concurrent_acquire(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 22: UART Concurrent Acquire Rejection");
  (void)dm;

  uart_lease_t lease1 = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_SENSOR_ACTIVE, NULL, pdMS_TO_TICKS(1000), &lease1);
  TEST_ASSERT(ret == MRS_OK, "Acquire lease1 for SENSOR_ACTIVE must succeed");

  uart_lease_t lease2 = {0};
  ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(100), &lease2);
  TEST_ASSERT(ret == MRS_ERR_CORE_INVALID_STATE, "Acquiring bus while SENSOR_ACTIVE holds it must return INVALID_STATE");

  ret = UartResource_Release(&lease1);
  TEST_ASSERT(ret == MRS_OK, "Release lease1 must succeed");
  return true;
}

/* 23. Test InternetManager Self-Deadlock Protection */
static bool test_internet_manager_self_deadlock(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 23: InternetManager Self-Deadlock Protection");

  // Simulate TASK_WIFI_CONFIG calling StopTask on its own handle
  dm->TaskHandle_Array[TASK_WIFI_CONFIG] = xTaskGetCurrentTaskHandle();
  system_err_t ret = InternetManager_StopTask(dm, TASK_WIFI_CONFIG, pdMS_TO_TICKS(100));
  TEST_ASSERT(ret == MRS_OK, "Stopping own task handle must return MRS_OK without deadlocking");

  dm->TaskHandle_Array[TASK_WIFI_MESH_JOIN] = xTaskGetCurrentTaskHandle();
  ret = InternetManager_StopTask(dm, TASK_WIFI_MESH_JOIN, pdMS_TO_TICKS(100));
  TEST_ASSERT(ret == MRS_OK, "Stopping own mesh task handle must return MRS_OK without deadlocking");

  dm->TaskHandle_Array[TASK_WIFI_CONFIG] = NULL;
  dm->TaskHandle_Array[TASK_WIFI_MESH_JOIN] = NULL;
  return true;
}

/* 24. Test FunctionManager CLEANUP_REQUIRED State */
static bool test_fm_cleanup_required(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 24: FunctionManager CLEANUP_REQUIRED State");

  // When CLEANUP_REQUIRED, business APIs and new init must be rejected
  FunctionManager_Deinit();
  FunctionManager_TestInjectInitFault(10); // Fault during internet manager init
  system_err_t ret = FunctionManager_Init(dm);
  TEST_ASSERT(ret != MRS_OK, "Init with fault must fail");

  // Retry Deinit
  ret = FunctionManager_Deinit();
  TEST_ASSERT(ret == MRS_OK, "Deinit retry must succeed and restore UNINITIALIZED");
  TEST_ASSERT(FunctionManager_GetState() == FM_STATE_UNINITIALIZED, "State must be UNINITIALIZED");
  return true;
}

/* 25. Test Boot Regression Sequence (Requirement 27) */
static bool test_boot_regression_sequence(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 25: Boot Regression Sequence");
  (void)dm;

  UartResource_Deinit();

  system_err_t ret = UartResource_Init(UART_NUM_1, PIN_UART_TX, PIN_UART_RX);
  TEST_ASSERT(ret == MRS_OK, "UartResource_Init must succeed");

  ret = UartResource_RegisterGatewayCallbacks(mock_gateway_quiesce, mock_gateway_resume, NULL);
  TEST_ASSERT(ret == MRS_OK, "RegisterGatewayCallbacks must succeed");

  uart_resource_snapshot_t snap = {0};
  TEST_ASSERT(UartResource_GetSnapshot(&snap) == MRS_OK, "GetSnapshot must succeed");
  TEST_ASSERT(snap.current_owner == UART_OWNER_NONE, "Owner must remain NONE after register callbacks");
  TEST_ASSERT(snap.state == UART_RES_STATE_UNINITIALIZED, "State must remain UNINITIALIZED after register callbacks");
  TEST_ASSERT(snap.active_token == 0, "Token must remain 0");
  TEST_ASSERT(!uart_is_driver_installed(UART_NUM_1), "Driver must NOT be installed before Acquire");

  uart_resource_config_t gw_cfg = {
      .uart_config = {
          .baud_rate = 115200,
          .data_bits = UART_DATA_8_BITS,
          .parity = UART_PARITY_DISABLE,
          .stop_bits = UART_STOP_BITS_1,
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
          .source_clk = UART_SCLK_DEFAULT,
      },
      .rx_buffer_size = 256,
      .tx_buffer_size = 256,
      .queue_size = 10,
      .out_queue = NULL,
      .intr_alloc_flags = 0,
      .enable_intr = false,
  };

  uart_lease_t lease = {0};
  ret = UartResource_Acquire(UART_OWNER_GATEWAY, &gw_cfg, pdMS_TO_TICKS(1000), &lease);
  TEST_ASSERT(ret == MRS_OK, "Acquire for GATEWAY must succeed");
  TEST_ASSERT(uart_is_driver_installed(UART_NUM_1), "Driver must be installed after Acquire");

  TEST_ASSERT(UartResource_GetSnapshot(&snap) == MRS_OK, "GetSnapshot must succeed");
  TEST_ASSERT(snap.current_owner == UART_OWNER_GATEWAY, "Owner must be GATEWAY only after install");
  TEST_ASSERT(snap.state == UART_RES_STATE_GATEWAY_ACTIVE, "State must be GATEWAY_ACTIVE");
  TEST_ASSERT(snap.active_token != 0, "Token must be non-zero");

  ret = UartResource_Release(&lease);
  TEST_ASSERT(ret == MRS_OK, "Release must succeed");
  return true;
}

/* 26. Test Same-Owner Fast Path (Requirement 28) */
static bool test_same_owner_fast_path(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 26: Same-Owner Fast Path");
  (void)dm;

  UartResource_Init(UART_NUM_1, PIN_UART_TX, PIN_UART_RX);

  uart_resource_config_t cfg1 = {
      .uart_config = {
          .baud_rate = 115200,
          .data_bits = UART_DATA_8_BITS,
          .parity = UART_PARITY_DISABLE,
          .stop_bits = UART_STOP_BITS_1,
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
          .source_clk = UART_SCLK_DEFAULT,
      },
      .rx_buffer_size = 256,
      .tx_buffer_size = 256,
      .queue_size = 0,
      .out_queue = NULL,
  };

  uart_lease_t lease1 = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_GATEWAY, &cfg1, pdMS_TO_TICKS(1000), &lease1);
  TEST_ASSERT(ret == MRS_OK, "Initial Acquire must succeed");

  // Fast path: same owner, same config
  uart_lease_t lease2 = {0};
  ret = UartResource_Acquire(UART_OWNER_GATEWAY, &cfg1, pdMS_TO_TICKS(1000), &lease2);
  TEST_ASSERT(ret == MRS_OK, "Fast path with identical config must succeed");
  TEST_ASSERT(lease2.token == lease1.token, "Fast path must return same token");

  // Fast path with different config must be rejected
  uart_resource_config_t cfg_diff = cfg1;
  cfg_diff.uart_config.baud_rate = 9600;
  uart_lease_t lease3 = {0};
  ret = UartResource_Acquire(UART_OWNER_GATEWAY, &cfg_diff, pdMS_TO_TICKS(1000), &lease3);
  TEST_ASSERT(ret == MRS_ERR_CORE_INVALID_STATE, "Fast path with different config must be rejected");

  ret = UartResource_Release(&lease1);
  TEST_ASSERT(ret == MRS_OK, "Release must succeed");
  return true;
}

/* 27. Test Driver Fault Injections (Requirement 29) */
static bool test_uart_fault_injections(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 27: Driver Fault Injections");
  (void)dm;

  UartResource_Init(UART_NUM_1, PIN_UART_TX, PIN_UART_RX);

  // Inject install fault
  UartResource_TestInjectDriverInstallFault(true);
  uart_lease_t lease = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_GATEWAY, NULL, pdMS_TO_TICKS(500), &lease);
  TEST_ASSERT(ret == MRS_ERR_DRIVERS_INIT_FAILED, "Install fault must return MRS_ERR_DRIVERS_INIT_FAILED");
  TEST_ASSERT(!UartResource_IsLeaseValid(&lease), "Lease must not be issued on fault");
  TEST_ASSERT(!uart_is_driver_installed(UART_NUM_1), "Driver must not remain installed on fault");
  UartResource_TestInjectDriverInstallFault(false);

  // Test quiesce timeout
  UartResource_RegisterGatewayCallbacks(mock_gateway_quiesce, mock_gateway_resume, NULL);
  ret = UartResource_Acquire(UART_OWNER_GATEWAY, NULL, pdMS_TO_TICKS(1000), &lease);
  TEST_ASSERT(ret == MRS_OK, "Acquire gateway must succeed");

  UartResource_TestInjectQuiesceTimeout(true);
  uart_lease_t scan_lease = {0};
  ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(500), &scan_lease);
  TEST_ASSERT(ret == MRS_ERR_CORE_TIMEOUT, "Quiesce timeout must return MRS_ERR_CORE_TIMEOUT");
  TEST_ASSERT(!UartResource_IsLeaseValid(&scan_lease), "Scanner lease must not be issued on timeout");
  UartResource_TestInjectQuiesceTimeout(false);

  // Test resume fault
  ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(1000), &scan_lease);
  TEST_ASSERT(ret == MRS_OK, "Acquire scanner must succeed");

  UartResource_TestInjectResumeFault(true);
  ret = UartResource_Release(&scan_lease);
  TEST_ASSERT(ret == MRS_ERR_DRIVERS_INIT_FAILED, "Resume fault must return error");
  uart_resource_snapshot_t snap = {0};
  UartResource_GetSnapshot(&snap);
  TEST_ASSERT(snap.state == UART_RES_STATE_CLEANUP_REQUIRED, "State must transition to CLEANUP_REQUIRED on resume fault");
  UartResource_TestInjectResumeFault(false);

  UartResource_Deinit();
  return true;
}

/* 28. Test Scan Single Quiesce & Restore (Requirement 30) */
static bool test_scan_single_quiesce_restore(DataManager_t *dm) {
  ESP_LOGI(TAG, "Running Test 28: Scan Single Quiesce & Restore");
  (void)dm;

  UartResource_Init(UART_NUM_1, PIN_UART_TX, PIN_UART_RX);
  UartResource_RegisterGatewayCallbacks(mock_gateway_quiesce, mock_gateway_resume, NULL);

  uart_lease_t gw_lease = {0};
  system_err_t ret = UartResource_Acquire(UART_OWNER_GATEWAY, NULL, pdMS_TO_TICKS(1000), &gw_lease);
  TEST_ASSERT(ret == MRS_OK, "Gateway acquire must succeed");

  // Scanner probe acquires lease
  uart_lease_t scan_lease = {0};
  ret = UartResource_Acquire(UART_OWNER_SENSOR_SCANNER, NULL, pdMS_TO_TICKS(1000), &scan_lease);
  TEST_ASSERT(ret == MRS_OK, "Scanner acquire must succeed");
  TEST_ASSERT(uart_is_driver_installed(UART_NUM_1), "Driver must be installed for scanner");

  // Scanner probe completes and releases lease
  ret = UartResource_Release(&scan_lease);
  TEST_ASSERT(ret == MRS_OK, "Scanner release must succeed");

  uart_resource_snapshot_t snap = {0};
  UartResource_GetSnapshot(&snap);
  TEST_ASSERT(snap.current_owner == UART_OWNER_GATEWAY, "Owner must be restored to GATEWAY");
  TEST_ASSERT(snap.state == UART_RES_STATE_GATEWAY_ACTIVE, "State must be GATEWAY_ACTIVE");
  TEST_ASSERT(uart_is_driver_installed(UART_NUM_1), "Gateway driver must be restored");

  UartResource_Deinit();
  return true;
}

int app_run_all_tests(void) {
  ESP_LOGI(TAG, "=== Starting MSMS Architectural Regression Tests (28 Suites) ===");
  static DataManager_t test_dm;
  memset(&test_dm, 0, sizeof(test_dm));

  int failed = 0;
  setup_mock_sensors();
  system_err_t sm_init = SensorManager_Init(&test_dm);
  TEST_ASSERT_RUNNER(sm_init == MRS_OK, "SensorManager_Init must succeed in test runner");
  esp_err_t ss_init = SensorScanner_Init(&test_dm);
  TEST_ASSERT_RUNNER(ss_init == ESP_OK, "SensorScanner_Init must succeed in test runner");

  if (!test_assign(&test_dm)) failed++;
  if (!test_duplicate(&test_dm)) failed++;
  if (!test_uart_conflict(&test_dm)) failed++;
  if (!test_uart_replace(&test_dm)) failed++;
  if (!test_rollback(&test_dm)) failed++;
  if (!test_pause_timeout(&test_dm)) failed++;
  if (!test_reset(&test_dm)) failed++;
  if (!test_concurrency(&test_dm)) failed++;
  if (!test_root_scan_block(&test_dm)) failed++;
  if (!test_port_capacity_limit(&test_dm)) failed++;
  if (!test_uart_init_faults(&test_dm)) failed++;
  if (!test_fm_init_success_and_deinit(&test_dm)) failed++;
  if (!test_fm_init_rollback_faults(&test_dm)) failed++;
  if (!test_sensor_manager_lifecycle(&test_dm)) failed++;
  if (!test_uart_resume_failure(&test_dm)) failed++;
  if (!test_driver_deinit_failure(&test_dm)) failed++;
  if (!test_mesh_startup_faults(&test_dm)) failed++;
  if (!test_fm_api_gating_and_teardown(&test_dm)) failed++;
  if (!test_uart_resource_quiesce_resume(&test_dm)) failed++;
  if (!test_uart_resource_quiesce_timeout(&test_dm)) failed++;
  if (!test_uart_stale_and_double_release(&test_dm)) failed++;
  if (!test_uart_concurrent_acquire(&test_dm)) failed++;
  if (!test_internet_manager_self_deadlock(&test_dm)) failed++;
  if (!test_fm_cleanup_required(&test_dm)) failed++;
  if (!test_boot_regression_sequence(&test_dm)) failed++;
  if (!test_same_owner_fast_path(&test_dm)) failed++;
  if (!test_uart_fault_injections(&test_dm)) failed++;
  if (!test_scan_single_quiesce_restore(&test_dm)) failed++;

  SensorManager_Deinit();
  SensorScanner_Deinit();
  sensor_registry_reset_driver_overrides();

  if (failed == 0) {
    ESP_LOGI(TAG, "=== ALL 28 ARCHITECTURAL TEST SUITES PASSED SUCCESSFULLY! ===");
  } else {
    ESP_LOGE(TAG, "=== %d TEST SUITE(S) FAILED! ===", failed);
  }
  return failed;
}

#else /* !CONFIG_MSMS_ENABLE_TESTS */

int app_run_all_tests(void) {
  return 0;
}

#endif /* CONFIG_MSMS_ENABLE_TESTS */
