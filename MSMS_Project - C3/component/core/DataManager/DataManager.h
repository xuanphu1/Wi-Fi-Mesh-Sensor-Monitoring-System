#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "SensorTypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_TEXT_LENGTH 21
#define NUM_OBJECT_MAX 5
#define NUM_SENSORS 20
#define BTN_UP_GPIO 18
#define BTN_DOWN_GPIO 14
#define BTN_SEL_GPIO 25
#define BTN_BACK_GPIO 13
#define MAX_VISIBLE_ITEMS 4

#if defined(CONFIG_IDF_TARGET_ESP32)
#define PIN_ACTUATOR_IO1_P1 32
#define PIN_ACTUATOR_IO3_P2 35
#define PIN_ACTUATOR_IO1_P3 33
#else
#define PIN_ACTUATOR_IO1_P1 -1
#define PIN_ACTUATOR_IO3_P2 -1
#define PIN_ACTUATOR_IO1_P3 -1
#endif

/**
 * @file DataManager.h
 * @brief Cáº¥u trÃºc dá»¯ liá»‡u á»©ng dá»¥ng: menu, mÃ n hÃ¬nh, objectInfo,
 * DataManager vÃ  bundle tham sá»‘ task.
 */

typedef enum { BTN_UP, BTN_DOWN, BTN_SEL, BTN_BACK, BTN_NONE } button_type_t;

typedef enum {
  IMAGE_BATTERY_0,
  IMAGE_BATTERY_17,
  IMAGE_BATTERY_33,
  IMAGE_BATTERY_50,
  IMAGE_BATTERY_67,
  IMAGE_BATTERY_83,
  IMAGE_BATTERY_FULL,
} Image_Battery_t;

typedef enum {
  IMAGE_WIFI_NOT_CONNECTED,
  IMAGE_WIFI_CONNECTED,
} Image_Wifi_t;

typedef enum {
  DISCONNECTED,
  CONNECTED,
  ERROR,
} status;

typedef enum {
  STATUS_OK = 0,
  STATUS_ERROR = 1,
  STATUS_WARNING = 2,
  STATUS_INFO = 3,
  STATUS_DEBUG = 4,
  STATUS_TRACE = 5,
  STATUS_FATAL = 6,
  STATUS_UNKNOWN = 7,

} Status_t;

typedef enum {
  OBJECT_WIFI,
  OBJECT_BATTERY,
  OBJECT_DIFFERENT,
  OBJECT_SENSOR,
  OBJECT_INFORMATION,
  OBJECT_WIFI_MESH,
  OBJECT_NONE,
} object_type_t;

enum {
  TASK_MESH_LINK,
  TASK_MESH_DATA,
  TASK_WIFI_CONFIG,
  TASK_WIFI_MESH_JOIN,
  TASK_READ_SENSOR,
  TASK_SHOW_DATA_SENSOR,
  TASK_BATTERY_STATUS,
  TASK_RESET_ALL_PORTS,
  TASK_ACTUATOR_ON,
  TASK_ACTUATOR_OFF,
  TASK_NONE,
};

typedef enum {
  MENU_ACTION,
  MENU_SUBMENU,
  MENU_READ_SENSOR,
  MENU_NONE
} menu_item_type_t;

struct menu_list;

typedef struct {
  uint8_t state;
  uint32_t last_press_ms;
} Button_t;

typedef struct {
  Button_t up;
  Button_t down;
  Button_t sel;
  Button_t back;
} ButtonManager_t;

typedef struct {
  const unsigned char ***image;
  uint8_t img_category;
  uint8_t img_index;
  uint8_t width;
  uint8_t height;
} image_t;

typedef struct {
  const char ***text;
  uint8_t size;
} text_t;

typedef struct menu_item {
  const char *name;
  menu_item_type_t type;
  void (*callback)(void *ctx);
  void *ctx;
  struct menu_list *children;
  image_t image_item_preview;
} menu_item_t;

typedef struct menu_list {
  menu_item_t *items;
  text_t text;
  image_t image;
  int8_t count;
  object_type_t object;
  struct menu_list *parent;
  int8_t port_index;
} menu_list_t;

typedef struct {
  menu_list_t *current;
  int8_t selected;
  int8_t prev_selected;
  bool is_menu_active;
  bool is_dashboard_active;
  uint8_t dashboard_page;
} ScreenManager_t;

typedef struct {
  uint8_t batteryLevel;
  uint8_t levelPercent;
  char *batteryName;
  bool is_charging;
} BatteryInfo_t;

typedef struct {
  status wifiStatus;
  char *wifiName;
} wifiInfo_t;

typedef struct {
  status meshStatus;
  char *ipRoot;
} meshInfo_t;

/** Vai trÃ² mesh-lite (dÃ¹ng chung MeshManager / menu / UART gateway). */
typedef enum {
  MESH_ROLE_ROOT,
  MESH_ROLE_NODE,
} mesh_role_t;

/** KÃ­ch thÆ°á»›c payload UDP tá»« node (JSON telemetry, tá»‘i Ä‘a). */
#define MESH_ROOT_UDP_FRAME_SIZE 256
/** Má»™t queue chung root â†’ gateway (Ä‘á»™ sÃ¢u FreeRTOS). */
#define MESH_GATEWAY_RX_QUEUE_DEPTH 64
/** Gá»­i heartbeat "No node" khi queue rá»—ng (root, ms). */
#define MESH_ROOT_UART_NO_NODE_MS 1000
/** Schema JSON trong payload UDP telemetry (MeshManager). v4: thÃªm "ver" +
 * "err" (máº£ng lá»—i). */
#define MESH_UDP_JSON_SCHEMA 1

/** Má»™t gÃ³i Ä‘Ã£ nháº­n trÃªn root (Ä‘Æ°a vÃ o gateway_rx_queue). */
typedef struct {
  uint32_t src_ip; /**< sin_addr.s_addr (network byte order). */
  uint16_t len;
  uint8_t data[MESH_ROOT_UDP_FRAME_SIZE];
} mesh_gateway_rx_msg_t;

/**
 * Tráº¡ng thÃ¡i mesh I/O dÃ¹ng chung: link, vai trÃ², queue gateway, socket UDP
 * (má»Ÿ bá»Ÿi mesh_link). udp_tx_* / udp_tx_sock: node gá»­i telemetry.
 * udp_rx_sock: root nháº­n UDP.
 */
typedef struct {
  QueueHandle_t gateway_rx_queue;
  SemaphoreHandle_t sock_mutex;
  volatile bool link_up;
  mesh_role_t role;
  int udp_tx_sock;
  int udp_rx_sock;
  /** ÄÃ­ch gá»­i node â†’ root (network byte order, giá»‘ng
   * sin_addr.s_addr).
   */
  uint32_t udp_tx_ip_be;
  /** Cá»•ng Ä‘Ã­ch host order (sáº½ htons trong mesh_data). */
  uint16_t udp_tx_port_host;
} mesh_io_context_t;

typedef struct {
  Status_t status;

  /** Äá»“ng bá»™ Wiâ€‘Fi STA (cáº­p nháº­t tá»«
   * WifiManager).
   */
  bool sta_connected;
} dm_wifi_t;

typedef struct {
  BatteryInfo_t batteryInfo;
  wifiInfo_t wifiInfo;
  meshInfo_t meshInfo;
  const char *selectedSensorName[NUM_PORTS];
} objectInfoManager_t;

struct DataManager_t;
typedef void (*on_sensor_selected_fn)(struct DataManager_t *data, int port);
typedef void (*on_ports_reset_fn)(struct DataManager_t *data);

typedef struct DataManager_t {
  SensorData_t sensor;
  ButtonManager_t button;
  ScreenManager_t screen;
  objectInfoManager_t objectInfo;
  menu_list_t *MenuReturn[10];
  SensorType_t selectedSensor[NUM_PORTS];
  SensorData_t port_data[NUM_PORTS];
  on_sensor_selected_fn on_sensor_selected;
  on_ports_reset_fn on_ports_reset;

  mesh_io_context_t meshIo;

  TaskHandle_t TaskHandle_Array[10];

  uint8_t version[3];
  uint16_t error_code[10];
} DataManager_t;

#define DATA_MANAGER_ERROR_CAPACITY 10

typedef struct {
  DataManager_t *data;
  PortId_t port;
  SensorType_t sensor;
} SelectionParam_t;

typedef struct {
  DataManager_t *data;
  PortId_t port;
  bool ShowDataScreen;
} ShowDataSensorParam_t;

/** Tham sá»‘ heap cho wifi_mesh_join_task (FunctionManager). */
typedef struct {
  DataManager_t *data;
  bool as_root;
} MeshJoinTaskArg_t;

#endif /* DATA_MANAGER_H */
