const MODULE_LABELS = Object.freeze({
  0x01: "CORE",
  0x02: "SENSORS",
  0x03: "UI",
  0x04: "NETWORK",
  0x05: "DRIVERS",
  0x06: "UTILS",
});

const ERROR_DEFINITIONS = Object.freeze([
  // CORE
  { symbol: "MRS_ERR_CORE_INVALID_PARAM", code: 0x1001, description: "Invalid parameter" },
  { symbol: "MRS_ERR_CORE_NOT_INITIALIZED", code: 0x1002, description: "Component not initialized" },
  { symbol: "MRS_ERR_CORE_ALREADY_INIT", code: 0x1003, description: "Component already initialized" },
  { symbol: "MRS_ERR_CORE_OUT_OF_MEMORY", code: 0x1004, description: "Out of memory" },
  { symbol: "MRS_ERR_CORE_INVALID_STATE", code: 0x1005, description: "Invalid state" },
  { symbol: "MRS_ERR_CORE_NOT_FOUND", code: 0x1006, description: "Resource not found" },
  { symbol: "MRS_ERR_CORE_TIMEOUT", code: 0x1007, description: "Operation timeout" },
  { symbol: "MRS_ERR_DATAMANAGER_INVALID_PORT", code: 0x1011, description: "Invalid port ID" },
  { symbol: "MRS_ERR_DATAMANAGER_PORT_IN_USE", code: 0x1012, description: "Port already in use" },
  { symbol: "MRS_ERR_DATAMANAGER_NO_SENSOR", code: 0x1013, description: "No sensor assigned to port" },
  { symbol: "MRS_ERR_FUNCTIONMANAGER_TASK_FAILED", code: 0x1021, description: "Failed to create task" },

  // SENSORS
  { symbol: "MRS_ERR_SENSORS_INVALID_TYPE", code: 0x2001, description: "Invalid sensor type" },
  { symbol: "MRS_ERR_SENSORS_NOT_INITIALIZED", code: 0x2002, description: "Sensor not initialized" },
  { symbol: "MRS_ERR_SENSORS_INIT_FAILED", code: 0x2003, description: "Sensor initialization failed" },
  { symbol: "MRS_ERR_SENSORS_READ_FAILED", code: 0x2004, description: "Sensor read failed" },
  { symbol: "MRS_ERR_SENSORS_INVALID_DATA", code: 0x2005, description: "Invalid sensor data" },
  { symbol: "MRS_ERR_SENSORS_NOT_FOUND", code: 0x2006, description: "Sensor not found in registry" },
  { symbol: "MRS_ERR_SENSORS_REGISTRY_FULL", code: 0x2007, description: "Sensor registry full" },
  { symbol: "MRS_ERR_SENSORCONFIG_WRAPPER_FAILED", code: 0x2011, description: "Wrapper function failed" },
  { symbol: "MRS_ERR_SENSORREGISTRY_INVALID_INDEX", code: 0x2021, description: "Invalid sensor index" },
  { symbol: "MRS_ERR_SENSOR_BME280_INIT_FAILED", code: 0x5000, description: "BME280 init failed" },
  { symbol: "MRS_ERR_SENSOR_BME280_READ_FAILED", code: 0x5080, description: "BME280 read failed" },

  // UI
  { symbol: "MRS_ERR_UI_INVALID_MENU", code: 0x3001, description: "Invalid menu" },
  { symbol: "MRS_ERR_UI_INVALID_ITEM", code: 0x3002, description: "Invalid menu item" },
  { symbol: "MRS_ERR_UI_RENDER_FAILED", code: 0x3003, description: "UI render failed" },
  { symbol: "MRS_ERR_UI_INVALID_BUTTON", code: 0x3004, description: "Invalid button state" },
  { symbol: "MRS_ERR_MENUSYSTEM_INVALID_CALLBACK", code: 0x3011, description: "Invalid callback function" },
  { symbol: "MRS_ERR_MENUSYSTEM_NAVIGATION_FAILED", code: 0x3012, description: "Navigation failed" },
  { symbol: "MRS_ERR_SCREENMANAGER_NOT_INIT", code: 0x3021, description: "ScreenManager not initialized" },
  { symbol: "MRS_ERR_SCREENMANAGER_DISPLAY_FAIL", code: 0x3022, description: "Display operation failed" },
  { symbol: "MRS_ERR_BUTTONMANAGER_GPIO_FAILED", code: 0x3031, description: "GPIO configuration failed" },

  // NETWORK
  { symbol: "MRS_ERR_NETWORK_NOT_INITIALIZED", code: 0x4001, description: "Network not initialized" },
  { symbol: "MRS_ERR_NETWORK_CONNECTION_FAILED", code: 0x4002, description: "Connection failed" },
  { symbol: "MRS_ERR_NETWORK_TIMEOUT", code: 0x4003, description: "Network operation timeout" },
  { symbol: "MRS_ERR_NETWORK_INVALID_CONFIG", code: 0x4004, description: "Invalid network configuration" },
  { symbol: "MRS_ERR_WIFIMANAGER_INIT_FAILED", code: 0x4011, description: "WiFi initialization failed" },
  { symbol: "MRS_ERR_WIFIMANAGER_STA_FAILED", code: 0x4012, description: "STA mode failed" },
  { symbol: "MRS_ERR_WIFIMANAGER_AP_FAILED", code: 0x4013, description: "AP mode failed" },
  { symbol: "MRS_ERR_WIFIMANAGER_INVALID_SSID", code: 0x4014, description: "Invalid SSID" },
  { symbol: "MRS_ERR_WIFIMANAGER_INVALID_PASS", code: 0x4015, description: "Invalid password" },
  { symbol: "MRS_ERR_MESHMANAGER_INIT_FAILED", code: 0x4021, description: "Mesh initialization failed" },
  { symbol: "MRS_ERR_MESHMANAGER_NOT_SUPPORTED", code: 0x4022, description: "Mesh not supported" },

  // DRIVERS
  { symbol: "MRS_ERR_DRIVERS_INIT_FAILED", code: 0x5001, description: "Driver initialization failed" },
  { symbol: "MRS_ERR_DRIVERS_NOT_INITIALIZED", code: 0x5002, description: "Driver not initialized" },
  { symbol: "MRS_ERR_DRIVERS_INVALID_PARAM", code: 0x5003, description: "Invalid driver parameter" },
  { symbol: "MRS_ERR_DRIVERS_COMM_FAILED", code: 0x5004, description: "Communication failed" },
  { symbol: "MRS_ERR_I2CDEV_INIT_FAILED", code: 0x5011, description: "I2C initialization failed" },
  { symbol: "MRS_ERR_I2CDEV_BUSY", code: 0x5012, description: "I2C bus busy" },
  { symbol: "MRS_ERR_I2CDEV_NACK", code: 0x5013, description: "I2C NACK received" },
  { symbol: "MRS_ERR_I2CDEV_TIMEOUT", code: 0x5014, description: "I2C timeout" },
  { symbol: "MRS_ERR_SSD1306_INIT_FAILED", code: 0x5021, description: "SSD1306 initialization failed" },
  { symbol: "MRS_ERR_SSD1306_DISPLAY_FAILED", code: 0x5022, description: "SSD1306 display operation failed" },
  { symbol: "MRS_ERR_DS3231_INIT_FAILED", code: 0x5031, description: "DS3231 initialization failed" },
  { symbol: "MRS_ERR_DS3231_READ_FAILED", code: 0x5032, description: "DS3231 read failed" },
  { symbol: "MRS_ERR_DS3231_WRITE_FAILED", code: 0x5033, description: "DS3231 write failed" },
  { symbol: "MRS_ERR_LEDRGB_INIT_FAILED", code: 0x5041, description: "LED RGB initialization failed" },
  { symbol: "MRS_ERR_LEDRGB_RMT_FAILED", code: 0x5042, description: "RMT peripheral failed" },

  // UTILS
  { symbol: "MRS_ERR_UTILS_INVALID_OPERATION", code: 0x6001, description: "Invalid operation" },
]);

function normalizeHex(text, maxDigits) {
  if (typeof text !== "string") return null;
  const m = text.trim().match(/^0x([0-9a-fA-F]{1,8})$/);
  if (!m) return null;
  const hex = m[1].toUpperCase();
  if (hex.length > maxDigits) return null;
  return hex.padStart(maxDigits, "0");
}

function moduleFromCode(code) {
  return (Number(code) >> 12) & 0xff;
}

function toViewModel(def) {
  const moduleId = moduleFromCode(def.code);
  return {
    ...def,
    moduleId,
    moduleLabel: MODULE_LABELS[moduleId] || `0x${moduleId.toString(16).toUpperCase()}`,
    codeHex: `0x${def.code.toString(16).toUpperCase().padStart(4, "0")}`,
    shortHex: `0x${(def.code & 0xff).toString(16).toUpperCase().padStart(2, "0")}`,
  };
}

const VIEW_DEFINITIONS = ERROR_DEFINITIONS.map(toViewModel);
const BY_FULL = new Map(VIEW_DEFINITIONS.map((d) => [d.codeHex, d]));
const BY_SHORT = VIEW_DEFINITIONS.reduce((acc, d) => {
  const key = d.shortHex;
  const list = acc.get(key) || [];
  list.push(d);
  acc.set(key, list);
  return acc;
}, new Map());

export function resolveMrrErrorCode(rawCode) {
  const norm2 = normalizeHex(rawCode, 2);
  const norm4 = normalizeHex(rawCode, 4);
  if (!norm2 && !norm4) {
    return {
      rawCode,
      normalized: null,
      mode: "invalid",
      matches: [],
      summary: "Invalid error code format",
    };
  }

  if (norm4 && rawCode.trim().length > 4) {
    const fullKey = `0x${norm4}`;
    const fullMatch = BY_FULL.get(fullKey);
    return {
      rawCode,
      normalized: fullKey,
      mode: "full",
      matches: fullMatch ? [fullMatch] : [],
      summary: fullMatch ? fullMatch.symbol : "Unknown full code",
    };
  }

  const shortKey = `0x${norm2 || norm4.slice(-2)}`;
  const shortMatches = BY_SHORT.get(shortKey) || [];
  return {
    rawCode,
    normalized: shortKey,
    mode: "short",
    matches: shortMatches,
    summary:
      shortMatches.length === 1
        ? shortMatches[0].symbol
        : shortMatches.length > 1
          ? `Ambiguous (${shortMatches.length} definitions)`
          : "Unknown short code",
  };
}

