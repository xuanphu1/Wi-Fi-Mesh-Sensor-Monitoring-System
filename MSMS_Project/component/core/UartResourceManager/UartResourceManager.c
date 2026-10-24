#include "UartResourceManager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UartResourceManager";

static SemaphoreHandle_t s_res_mutex = NULL;
static uart_resource_state_t s_state = UART_RES_STATE_UNINITIALIZED;
static uart_owner_t s_current_owner = UART_OWNER_NONE;
static uint32_t s_generation = 0;
static uint32_t s_active_token = 0;
static uart_port_t s_port = UART_NUM_MAX;
static int s_tx_pin = -1;
static int s_rx_pin = -1;
static bool s_driver_installed = false;

static uart_gateway_quiesce_fn s_quiesce_cb = NULL;
static uart_gateway_resume_fn s_resume_cb = NULL;
static void *s_gateway_ctx = NULL;

static bool s_has_gateway_config = false;
static uart_resource_config_t s_gateway_config;

static bool s_has_active_config = false;
static uart_resource_config_t s_active_config;

#if CONFIG_MSMS_ENABLE_TESTS
static bool s_inject_quiesce_timeout = false;
static bool s_inject_resume_fault = false;
static bool s_inject_driver_install_fault = false;

void UartResource_TestInjectQuiesceTimeout(bool inject) {
  s_inject_quiesce_timeout = inject;
}

void UartResource_TestInjectResumeFault(bool inject) {
  s_inject_resume_fault = inject;
}

void UartResource_TestInjectDriverInstallFault(bool inject) {
  s_inject_driver_install_fault = inject;
}

void UartResource_TestReset(void) {
  s_inject_quiesce_timeout = false;
  s_inject_resume_fault = false;
  s_inject_driver_install_fault = false;
  s_generation = 0;
  s_active_token = 0;
  s_current_owner = UART_OWNER_NONE;
  s_state = UART_RES_STATE_UNINITIALIZED;
  s_driver_installed = false;
  s_has_gateway_config = false;
  s_has_active_config = false;
  s_quiesce_cb = NULL;
  s_resume_cb = NULL;
  s_gateway_ctx = NULL;
}
#endif

const char *UartResource_StateToString(uart_resource_state_t state) {
  switch (state) {
  case UART_RES_STATE_UNINITIALIZED:
    return "UNINITIALIZED";
  case UART_RES_STATE_GATEWAY_ACTIVE:
    return "GATEWAY_ACTIVE";
  case UART_RES_STATE_QUIESCE_REQUESTED:
    return "QUIESCE_REQUESTED";
  case UART_RES_STATE_SENSOR_PROBE_ACTIVE:
    return "SENSOR_PROBE_ACTIVE";
  case UART_RES_STATE_SENSOR_ASSIGNED:
    return "SENSOR_ASSIGNED";
  case UART_RES_STATE_GATEWAY_RESUMING:
    return "GATEWAY_RESUMING";
  case UART_RES_STATE_STOPPING:
    return "STOPPING";
  case UART_RES_STATE_CLEANUP_REQUIRED:
    return "CLEANUP_REQUIRED";
  case UART_RES_STATE_FAILED:
    return "FAILED";
  default:
    return "UNKNOWN";
  }
}

const char *UartResource_OwnerToString(uart_owner_t owner) {
  switch (owner) {
  case UART_OWNER_NONE:
    return "NONE";
  case UART_OWNER_GATEWAY:
    return "GATEWAY";
  case UART_OWNER_SENSOR_SCANNER:
    return "SENSOR_SCANNER";
  case UART_OWNER_SENSOR_ACTIVE:
    return "SENSOR_ACTIVE";
  default:
    return "UNKNOWN";
  }
}

static uint32_t generate_token(void) {
  static uint32_t token_counter = 100;
  token_counter++;
  if (token_counter == 0) {
    token_counter = 1;
  }
  return token_counter;
}

static bool validate_invariants_locked(void) {
  bool driver_actual = (s_port < UART_NUM_MAX) && uart_is_driver_installed(s_port);
  switch (s_state) {
  case UART_RES_STATE_UNINITIALIZED:
    if (s_current_owner != UART_OWNER_NONE || s_active_token != 0 || s_driver_installed || driver_actual) {
      ESP_LOGE(TAG, "Invariant violation in UNINITIALIZED: owner=%s token=%lu driver_cfg=%d driver_act=%d",
               UartResource_OwnerToString(s_current_owner), (unsigned long)s_active_token,
               s_driver_installed, driver_actual);
      return false;
    }
    break;
  case UART_RES_STATE_GATEWAY_ACTIVE:
    if (s_current_owner != UART_OWNER_GATEWAY || s_active_token == 0 || !s_driver_installed || !driver_actual) {
      ESP_LOGE(TAG, "Invariant violation in GATEWAY_ACTIVE: owner=%s token=%lu driver_cfg=%d driver_act=%d",
               UartResource_OwnerToString(s_current_owner), (unsigned long)s_active_token,
               s_driver_installed, driver_actual);
      return false;
    }
    break;
  case UART_RES_STATE_QUIESCE_REQUESTED:
    if (s_current_owner != UART_OWNER_GATEWAY || !driver_actual) {
      ESP_LOGE(TAG, "Invariant violation in QUIESCE_REQUESTED: owner=%s driver_act=%d",
               UartResource_OwnerToString(s_current_owner), driver_actual);
      return false;
    }
    break;
  case UART_RES_STATE_SENSOR_PROBE_ACTIVE:
    if (s_current_owner != UART_OWNER_SENSOR_SCANNER || s_active_token == 0 || !s_driver_installed || !driver_actual) {
      ESP_LOGE(TAG, "Invariant violation in SENSOR_PROBE_ACTIVE: owner=%s token=%lu driver_cfg=%d driver_act=%d",
               UartResource_OwnerToString(s_current_owner), (unsigned long)s_active_token,
               s_driver_installed, driver_actual);
      return false;
    }
    break;
  case UART_RES_STATE_SENSOR_ASSIGNED:
    if (s_current_owner != UART_OWNER_SENSOR_ACTIVE || s_active_token == 0 || !s_driver_installed || !driver_actual) {
      ESP_LOGE(TAG, "Invariant violation in SENSOR_ASSIGNED: owner=%s token=%lu driver_cfg=%d driver_act=%d",
               UartResource_OwnerToString(s_current_owner), (unsigned long)s_active_token,
               s_driver_installed, driver_actual);
      return false;
    }
    break;
  case UART_RES_STATE_GATEWAY_RESUMING:
  case UART_RES_STATE_STOPPING:
  case UART_RES_STATE_CLEANUP_REQUIRED:
  case UART_RES_STATE_FAILED:
    break;
  }
  return true;
}

static bool is_config_compatible(const uart_resource_config_t *a, const uart_resource_config_t *b) {
  if (a == NULL || b == NULL) {
    return false;
  }
  if (a->uart_config.baud_rate != b->uart_config.baud_rate ||
      a->uart_config.data_bits != b->uart_config.data_bits ||
      a->uart_config.parity != b->uart_config.parity ||
      a->uart_config.stop_bits != b->uart_config.stop_bits ||
      a->rx_buffer_size != b->rx_buffer_size ||
      a->tx_buffer_size != b->tx_buffer_size ||
      a->queue_size != b->queue_size) {
    return false;
  }
  return true;
}

static esp_err_t install_driver_locked(const uart_resource_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_inject_driver_install_fault) {
    return ESP_FAIL;
  }
#endif

  if (s_port >= UART_NUM_MAX) {
    return ESP_ERR_INVALID_STATE;
  }

  // Delete previous driver if present
  if (uart_is_driver_installed(s_port)) {
    uart_driver_delete(s_port);
    s_driver_installed = false;
  }

  esp_err_t err = uart_driver_install(
      s_port,
      config->rx_buffer_size,
      config->tx_buffer_size,
      config->queue_size,
      config->out_queue,
      config->intr_alloc_flags);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    return err;
  }

  err = uart_param_config(s_port, &config->uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    uart_driver_delete(s_port);
    return err;
  }

  err = uart_set_pin(s_port, s_tx_pin, s_rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    uart_driver_delete(s_port);
    return err;
  }

  if (config->enable_intr) {
    err = uart_intr_config(s_port, &config->intr_config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "uart_intr_config failed: %s", esp_err_to_name(err));
      uart_driver_delete(s_port);
      return err;
    }
    err = uart_enable_rx_intr(s_port);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "uart_enable_rx_intr failed: %s", esp_err_to_name(err));
      uart_driver_delete(s_port);
      return err;
    }
  }

  if (!uart_is_driver_installed(s_port)) {
    ESP_LOGE(TAG, "Driver verification failed after install!");
    return ESP_FAIL;
  }

  s_driver_installed = true;
  return ESP_OK;
}

system_err_t UartResource_Init(uart_port_t port, int tx_pin, int rx_pin) {
  if (port >= UART_NUM_MAX || tx_pin < 0 || rx_pin < 0) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (s_res_mutex == NULL) {
    s_res_mutex = xSemaphoreCreateMutex();
    if (s_res_mutex == NULL) {
      return MRS_ERR_CORE_OUT_OF_MEMORY;
    }
  }

  if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  // Idempotency check
  if (s_state != UART_RES_STATE_UNINITIALIZED) {
    if (s_port == port && s_tx_pin == tx_pin && s_rx_pin == rx_pin) {
      xSemaphoreGive(s_res_mutex);
      return MRS_OK;
    }
    ESP_LOGE(TAG, "Init rejected: already initialized with different port/pins");
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_CORE_INVALID_STATE;
  }

  s_port = port;
  s_tx_pin = tx_pin;
  s_rx_pin = rx_pin;
  s_generation = 1;
  s_active_token = 0;
  s_current_owner = UART_OWNER_NONE;
  s_state = UART_RES_STATE_UNINITIALIZED;
  s_driver_installed = false;
  s_has_gateway_config = false;
  s_has_active_config = false;

  (void)validate_invariants_locked();

  xSemaphoreGive(s_res_mutex);
  ESP_LOGI(TAG, "Initialized: port=%d, TX=%d, RX=%d, owner=NONE, state=UNINITIALIZED",
           (int)port, tx_pin, rx_pin);
  return MRS_OK;
}

system_err_t UartResource_Deinit(void) {
  if (s_res_mutex == NULL) {
    return MRS_OK;
  }

  if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    ESP_LOGE(TAG, "Deinit: timeout taking mutex");
    return MRS_ERR_CORE_TIMEOUT;
  }

  s_state = UART_RES_STATE_STOPPING;

  if (s_port < UART_NUM_MAX && uart_is_driver_installed(s_port)) {
    uart_driver_delete(s_port);
  }
  s_driver_installed = false;

  s_current_owner = UART_OWNER_NONE;
  s_active_token = 0;
  s_generation++;
  s_state = UART_RES_STATE_UNINITIALIZED;
  s_quiesce_cb = NULL;
  s_resume_cb = NULL;
  s_gateway_ctx = NULL;
  s_has_gateway_config = false;
  s_has_active_config = false;

  (void)validate_invariants_locked();

  xSemaphoreGive(s_res_mutex);
  ESP_LOGI(TAG, "Deinitialized successfully.");
  return MRS_OK;
}

system_err_t UartResource_RegisterGatewayCallbacks(uart_gateway_quiesce_fn quiesce_cb,
                                                   uart_gateway_resume_fn resume_cb,
                                                   void *context) {
  if (quiesce_cb == NULL || resume_cb == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_res_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }

  if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    ESP_LOGE(TAG, "RegisterGatewayCallbacks: timeout taking mutex");
    return MRS_ERR_CORE_TIMEOUT;
  }

  s_quiesce_cb = quiesce_cb;
  s_resume_cb = resume_cb;
  s_gateway_ctx = context;

  // IMPORTANT: Do NOT alter s_state, s_current_owner, s_active_token, s_generation, or s_driver_installed!
  (void)validate_invariants_locked();

  xSemaphoreGive(s_res_mutex);
  ESP_LOGI(TAG, "Registered gateway callbacks. Owner=%s, State=%s",
           UartResource_OwnerToString(s_current_owner),
           UartResource_StateToString(s_state));
  return MRS_OK;
}

system_err_t UartResource_Acquire(uart_owner_t owner,
                                  const uart_resource_config_t *config,
                                  TickType_t timeout_ticks,
                                  uart_lease_t *out_lease) {
  if (out_lease == NULL || owner == UART_OWNER_NONE) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_res_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }

  if (xSemaphoreTake(s_res_mutex, timeout_ticks) != pdTRUE) {
    ESP_LOGE(TAG, "Acquire: mutex timeout for owner %s", UartResource_OwnerToString(owner));
    return MRS_ERR_CORE_TIMEOUT;
  }

  // --- SAME-OWNER FAST PATH ---
  if (s_current_owner == owner && s_active_token != 0) {
    bool driver_actual = (s_port < UART_NUM_MAX) && uart_is_driver_installed(s_port);
    bool state_ok = (owner == UART_OWNER_GATEWAY && s_state == UART_RES_STATE_GATEWAY_ACTIVE) ||
                    (owner == UART_OWNER_SENSOR_SCANNER && s_state == UART_RES_STATE_SENSOR_PROBE_ACTIVE) ||
                    (owner == UART_OWNER_SENSOR_ACTIVE && s_state == UART_RES_STATE_SENSOR_ASSIGNED);

    if (!state_ok || !s_driver_installed || !driver_actual) {
      ESP_LOGE(TAG, "Fast-path invariant violation! state=%s, driver_cfg=%d, driver_act=%d. Rejecting fake lease.",
               UartResource_StateToString(s_state), s_driver_installed, driver_actual);
      s_state = UART_RES_STATE_CLEANUP_REQUIRED;
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_CORE_INVALID_STATE;
    }

    if (config != NULL && s_has_active_config && !is_config_compatible(&s_active_config, config)) {
      ESP_LOGE(TAG, "Fast-path rejected: requested config differs from active config");
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_CORE_INVALID_STATE;
    }

    out_lease->owner = owner;
    out_lease->token = s_active_token;
    out_lease->generation = s_generation;
    out_lease->port = s_port;
    xSemaphoreGive(s_res_mutex);
    return MRS_OK;
  }

  // Cannot acquire if another active sensor owns the bus
  if (s_current_owner == UART_OWNER_SENSOR_ACTIVE) {
    ESP_LOGW(TAG, "Acquire rejected: bus currently owned by active sensor (gen %lu)",
             (unsigned long)s_generation);
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_CORE_INVALID_STATE;
  }

  // Cannot acquire if another scanner is probing
  if (s_current_owner == UART_OWNER_SENSOR_SCANNER && owner == UART_OWNER_SENSOR_SCANNER) {
    ESP_LOGW(TAG, "Acquire rejected: concurrent scanner probe attempted");
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_CORE_INVALID_STATE;
  }

  // --- QUIESCE GATEWAY IF OWNED BY GATEWAY ---
  if (s_current_owner == UART_OWNER_GATEWAY) {
    uart_resource_state_t prev_state = s_state;
    s_state = UART_RES_STATE_QUIESCE_REQUESTED;
    ESP_LOGI(TAG, "Requesting Gateway quiesce for owner %s...", UartResource_OwnerToString(owner));

    uart_gateway_quiesce_fn q_fn = s_quiesce_cb;
    void *q_ctx = s_gateway_ctx;
    uint32_t expected_gen = s_generation;

    if (q_fn == NULL) {
      ESP_LOGE(TAG, "No Gateway quiesce callback registered!");
      s_state = prev_state;
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_CORE_INVALID_STATE;
    }

    // Unlock mutex during blocking quiesce call
    xSemaphoreGive(s_res_mutex);

    system_err_t q_err = MRS_OK;
#if CONFIG_MSMS_ENABLE_TESTS
    if (s_inject_quiesce_timeout) {
      q_err = MRS_ERR_CORE_TIMEOUT;
    } else
#endif
    {
      q_err = q_fn(q_ctx, timeout_ticks);
    }

    // Re-acquire mutex
    if (xSemaphoreTake(s_res_mutex, timeout_ticks) != pdTRUE) {
      ESP_LOGE(TAG, "Acquire: timeout re-taking mutex after quiesce");
      return MRS_ERR_CORE_TIMEOUT;
    }

    if (s_generation != expected_gen || s_state != UART_RES_STATE_QUIESCE_REQUESTED ||
        s_current_owner != UART_OWNER_GATEWAY) {
      ESP_LOGE(TAG, "State mutated during quiesce! gen=%lu vs %lu, state=%s",
               (unsigned long)s_generation, (unsigned long)expected_gen,
               UartResource_StateToString(s_state));
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_CORE_INVALID_STATE;
    }

    if (q_err != MRS_OK) {
      ESP_LOGE(TAG, "Gateway quiesce failed: %s; keeping owner GATEWAY", system_err_to_name(q_err));
      s_state = prev_state;
      xSemaphoreGive(s_res_mutex);
      return q_err;
    }

    ESP_LOGI(TAG, "Gateway successfully quiesced.");
    if (uart_is_driver_installed(s_port)) {
      uart_driver_delete(s_port);
      s_driver_installed = false;
    }
  }

  // --- INSTALL REQUESTED DRIVER ---
  if (config == NULL) {
    ESP_LOGE(TAG, "Acquire failed: NULL config provided for new owner %s",
             UartResource_OwnerToString(owner));
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  ESP_LOGI(TAG, "Installing UART%d driver for %s...", (int)s_port, UartResource_OwnerToString(owner));
  esp_err_t inst_err = install_driver_locked(config);
  if (inst_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install driver for owner %s: %s",
             UartResource_OwnerToString(owner), esp_err_to_name(inst_err));

    // If we previously quiesced Gateway, attempt to restore it
    if (s_current_owner == UART_OWNER_GATEWAY && s_resume_cb != NULL && s_has_gateway_config) {
      s_state = UART_RES_STATE_GATEWAY_RESUMING;
      QueueHandle_t r_q = NULL;
      uart_resource_config_t r_cfg = s_gateway_config;
      r_cfg.out_queue = &r_q;
      if (install_driver_locked(&r_cfg) == ESP_OK) {
        (void)s_resume_cb(s_gateway_ctx, s_port, r_q);
        s_state = UART_RES_STATE_GATEWAY_ACTIVE;
      } else {
        s_state = UART_RES_STATE_CLEANUP_REQUIRED;
      }
    }
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  // Record configurations
  if (owner == UART_OWNER_GATEWAY) {
    s_gateway_config = *config;
    s_has_gateway_config = true;
  }
  s_active_config = *config;
  s_has_active_config = true;

  // Issue lease and commit state
  s_generation++;
  s_active_token = generate_token();
  s_current_owner = owner;

  if (owner == UART_OWNER_GATEWAY) {
    s_state = UART_RES_STATE_GATEWAY_ACTIVE;
  } else if (owner == UART_OWNER_SENSOR_SCANNER) {
    s_state = UART_RES_STATE_SENSOR_PROBE_ACTIVE;
  } else if (owner == UART_OWNER_SENSOR_ACTIVE) {
    s_state = UART_RES_STATE_SENSOR_ASSIGNED;
  }

  out_lease->owner = owner;
  out_lease->token = s_active_token;
  out_lease->generation = s_generation;
  out_lease->port = s_port;

  (void)validate_invariants_locked();

  ESP_LOGI(TAG, "Lease acquired: owner=%s, gen=%lu, token=%lu",
           UartResource_OwnerToString(owner),
           (unsigned long)s_generation,
           (unsigned long)s_active_token);

  xSemaphoreGive(s_res_mutex);
  return MRS_OK;
}

system_err_t UartResource_Release(uart_lease_t *lease) {
  if (lease == NULL || lease->owner == UART_OWNER_NONE || lease->token == 0) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_res_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }

  if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    ESP_LOGE(TAG, "Release: mutex timeout");
    return MRS_ERR_CORE_TIMEOUT;
  }

  // Validate lease token and generation to prevent stale/double release
  if (lease->token != s_active_token || lease->generation != s_generation ||
      lease->owner != s_current_owner) {
    ESP_LOGE(TAG, "Release rejected: stale or invalid lease (token %lu vs %lu, owner %d vs %d)",
             (unsigned long)lease->token, (unsigned long)s_active_token,
             (int)lease->owner, (int)s_current_owner);
    xSemaphoreGive(s_res_mutex);
    return MRS_ERR_CORE_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Releasing lease: owner=%s, token=%lu",
           UartResource_OwnerToString(lease->owner), (unsigned long)lease->token);

  // Invalidate caller's lease
  lease->token = 0;
  lease->owner = UART_OWNER_NONE;

  // Delete sensor driver
  if (s_port < UART_NUM_MAX && uart_is_driver_installed(s_port)) {
    uart_driver_delete(s_port);
  }
  s_driver_installed = false;
  s_has_active_config = false;

  s_active_token = 0;
  s_generation++;
  s_current_owner = UART_OWNER_NONE;

  // Restore Gateway if resume callback and config are available
  if (s_resume_cb != NULL && s_has_gateway_config) {
    s_state = UART_RES_STATE_GATEWAY_RESUMING;
    ESP_LOGI(TAG, "Restoring Gateway UART driver...");

    QueueHandle_t new_queue = NULL;
    uart_resource_config_t r_cfg = s_gateway_config;
    r_cfg.out_queue = &new_queue;

    esp_err_t inst_err = install_driver_locked(&r_cfg);
    if (inst_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to restore Gateway driver: %s; entering CLEANUP_REQUIRED",
               esp_err_to_name(inst_err));
      s_state = UART_RES_STATE_CLEANUP_REQUIRED;
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_DRIVERS_INIT_FAILED;
    }

    s_active_config = r_cfg;
    s_has_active_config = true;

    uart_gateway_resume_fn r_fn = s_resume_cb;
    void *r_ctx = s_gateway_ctx;
    uint32_t expected_gen = s_generation;
    uart_port_t port = s_port;

    // Unlock mutex during resume callback
    xSemaphoreGive(s_res_mutex);

    system_err_t r_err = MRS_OK;
#if CONFIG_MSMS_ENABLE_TESTS
    if (s_inject_resume_fault) {
      r_err = MRS_ERR_DRIVERS_INIT_FAILED;
    } else
#endif
    {
      r_err = r_fn(r_ctx, port, new_queue);
    }

    // Re-acquire mutex
    if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
      ESP_LOGE(TAG, "Release: timeout re-taking mutex after resume");
      return MRS_ERR_CORE_TIMEOUT;
    }

    if (s_generation != expected_gen || s_state != UART_RES_STATE_GATEWAY_RESUMING) {
      ESP_LOGE(TAG, "State mutated during Gateway resume!");
      s_state = UART_RES_STATE_CLEANUP_REQUIRED;
      xSemaphoreGive(s_res_mutex);
      return MRS_ERR_CORE_INVALID_STATE;
    }

    if (r_err != MRS_OK) {
      ESP_LOGE(TAG, "Gateway resume callback failed: %s; entering CLEANUP_REQUIRED",
               system_err_to_name(r_err));
      s_state = UART_RES_STATE_CLEANUP_REQUIRED;
      xSemaphoreGive(s_res_mutex);
      return r_err;
    }

    s_state = UART_RES_STATE_GATEWAY_ACTIVE;
    s_current_owner = UART_OWNER_GATEWAY;
    s_active_token = generate_token();
    (void)validate_invariants_locked();

    ESP_LOGI(TAG, "Gateway successfully resumed. Owner=GATEWAY, token=%lu",
             (unsigned long)s_active_token);
  } else {
    s_state = UART_RES_STATE_UNINITIALIZED;
    (void)validate_invariants_locked();
  }

  xSemaphoreGive(s_res_mutex);
  return MRS_OK;
}

bool UartResource_IsLeaseValid(const uart_lease_t *lease) {
  if (lease == NULL || lease->owner == UART_OWNER_NONE || lease->token == 0) {
    return false;
  }
  return (lease->token == s_active_token && lease->generation == s_generation &&
          lease->owner == s_current_owner);
}

system_err_t UartResource_GetSnapshot(uart_resource_snapshot_t *out_snapshot) {
  if (out_snapshot == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_res_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }

  if (xSemaphoreTake(s_res_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  out_snapshot->state = s_state;
  out_snapshot->current_owner = s_current_owner;
  out_snapshot->current_generation = s_generation;
  out_snapshot->active_token = s_active_token;
  out_snapshot->port = s_port;

  xSemaphoreGive(s_res_mutex);
  return MRS_OK;
}
