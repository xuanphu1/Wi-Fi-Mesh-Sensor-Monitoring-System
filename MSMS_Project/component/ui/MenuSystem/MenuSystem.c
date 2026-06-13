#include "MenuSystem.h"
#include "Common.h"
#include "SensorRegistry.h"
#include "SensorTypes.h"
#include "sdkconfig.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types.h>
#include <time.h>


#define NUM_INTERFACES 4
#define PORT_DISPLAY_NAME_LEN 28
static const TypeCommunication_t MENU_INTERFACE_ORDER[] = {
    COMMUNICATION_UART,
    COMMUNICATION_I2C,
    COMMUNICATION_SPI,
    COMMUNICATION_PULSE,
};
static const char *MENU_INTERFACE_NAMES[] = {"UART", "I2C", "SPI", "PULSE"};

menu_list_t WiFi_Config_Menu;
DataManager_t *Data;
static const char *port[NUM_PORTS] = {"Port 1", "Port 2", "Port 3"};

/* -------------------- Dynamic Arrays (theo port + giao tiáº¿p)
 * -------------------- */
static SelectionParam_t *SensorSelectionByIface[NUM_PORTS][NUM_INTERFACES] = {
    {NULL}};
static menu_item_t *SensorItemsByIface[NUM_PORTS][NUM_INTERFACES] = {{NULL}};
static char port_display_name[NUM_PORTS][PORT_DISPLAY_NAME_LEN];
ShowDataSensorParam_t ShowDataSensorSelection[NUM_PORTS];

/* -------------------- Menu: Sensors â†’ Port 1/2 â†’ UART/I2C/SPI/ANALOG/PULSE â†’
 * danh sÃ¡ch cáº£m biáº¿n -------------------- */
menu_list_t PortInterfaceMenus[NUM_PORTS][NUM_INTERFACES]; /* [port][interface]
                                                              = menu cáº£m biáº¿n */
menu_item_t PortInterfaceMenuItems[NUM_PORTS]
                                  [NUM_INTERFACES]; /* "UART", "I2C", ... cho
                                                       má»—i port */
menu_list_t PortMenus[NUM_PORTS];                   /* Port 1, 2, 3 menu */

menu_item_t Show_Data_Sensor[] = {{NULL, MENU_ACTION, show_data_sensor_cb,
                                   &ShowDataSensorSelection[PORT_1], NULL, {0}},
                                  {NULL, MENU_ACTION, show_data_sensor_cb,
                                   &ShowDataSensorSelection[PORT_2], NULL, {0}},
                                  {NULL, MENU_ACTION, show_data_sensor_cb,
                                   &ShowDataSensorSelection[PORT_3], NULL, {0}}};

menu_list_t Show_Data_Sensor_Menu = {
    .items = Show_Data_Sensor,
    .count = ARRAY_SIZE(Show_Data_Sensor),
    .parent = NULL,
    .port_index = -1,
};

/* Sensor_Menu_Items: 5 má»¥c = Port 1, Port 2, Port 3, Show data sensor, Reset
 * All Ports (gÃ¡n trong init) */
static menu_item_t Sensor_Menu_Items[5];
menu_list_t Sensor_Menu = {
    .items = Sensor_Menu_Items,
    .count = 5,
    .parent = NULL,
    .port_index = -1,
};

// Submenu "WiFi Config" (káº¿t ná»‘i WiFi thÃ´ng thÆ°á»ng)
menu_item_t WiFi_Config_Items[] = {
    {"OK", MENU_ACTION, wifi_config_callback, NULL, NULL, {0}},
};

const image_t WiFi_Config_Image = {
    .image = imageManager,
    .width = 19,
    .height = 16,
};

const text_t WiFi_Config_Text = {
    .size = 12,
    .text = ManagerText,
};

menu_list_t WiFi_Config_Menu = {
    .items = WiFi_Config_Items,
    .text = WiFi_Config_Text,
    .image = WiFi_Config_Image,
    .object = OBJECT_WIFI,
    .count = ARRAY_SIZE(WiFi_Config_Items),
    .parent = NULL,
    .port_index = -1,
};

const text_t WiFi_Mesh_Text = {
    .size = 12,
    .text = ManagerText,
};

const image_t WiFi_Mesh_Config_Image = {
  .image = imageManager,
  .width = 32,
  .height = 32,
};

menu_item_t WiFi_MeshConfig_Items[] = {
  {"OK", MENU_ACTION, wifi_mesh_join_as_node_callback, NULL, NULL, {0}},
};

menu_list_t WiFi_Mesh_Config_Menu = {
    .items = WiFi_MeshConfig_Items,
    .text = WiFi_Mesh_Text,
    .image = WiFi_Mesh_Config_Image,
    .object = OBJECT_WIFI_MESH,
    .count = ARRAY_SIZE(WiFi_MeshConfig_Items),
    .parent = NULL,
    .port_index = -1,
};


// Submenu chá»n cháº¿ Ä‘á»™ WiFi: Connect WiFi / Join WiFi Mesh
menu_item_t WiFi_Mode_Items[] = {
    {"Connect WiFi", MENU_SUBMENU, NULL, NULL, &WiFi_Config_Menu, {0}},
    {"Join WiFi Mesh", MENU_SUBMENU, NULL, NULL, &WiFi_Mesh_Config_Menu, {0}},
};

menu_list_t WiFi_Mode_Menu = {
    .items = WiFi_Mode_Items,
    .object = OBJECT_NONE,
    .count = ARRAY_SIZE(WiFi_Mode_Items),
    .parent = NULL,
    .port_index = -1,
};

// Submenu "Battery Status"
menu_item_t Battery_Status_Items[] = {
    {"OK", MENU_ACTION, NULL, NULL, NULL, {0}},
};

const image_t Battery_Status_Image = {
    .image = imageManager,
    .width = 19,
    .height = 16,
};

const text_t Battery_Status_Text = {
    .size = 12,
    .text = ManagerText,
};

menu_list_t Battery_Status_Menu = {
    .items = Battery_Status_Items,
    .text = Battery_Status_Text,
    .image = Battery_Status_Image,
    .object = OBJECT_BATTERY,
    .count = ARRAY_SIZE(Battery_Status_Items),
    .parent = NULL,
    .port_index = -1,
};

/* -------------------- Actuators: board-fixed GPIOs -------------------- */
#if CONFIG_IDF_TARGET_ESP32
#define PIN_ACTUATOR_IO1_P1 32
#define PIN_ACTUATOR_IO3_P2 35
#define PIN_ACTUATOR_IO1_P3 33
#elif CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32C3
#define PIN_ACTUATOR_IO1_P1 -1
#define PIN_ACTUATOR_IO3_P2 2
#define PIN_ACTUATOR_IO1_P3 3
#else
#define PIN_ACTUATOR_IO1_P1 0
#define PIN_ACTUATOR_IO3_P2 0
#define PIN_ACTUATOR_IO1_P3 0
#endif

menu_item_t Actuator_IO1_Port1_Items[] = {
    {"ON", MENU_ACTION, actuator_on_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO1_P1,
     NULL, {0}},
    {"OFF", MENU_ACTION, actuator_off_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO1_P1,
     NULL, {0}},
};
menu_item_t Actuator_IO3_Port2_Items[] = {
    {"ON", MENU_ACTION, actuator_on_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO3_P2,
     NULL, {0}},
    {"OFF", MENU_ACTION, actuator_off_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO3_P2,
     NULL, {0}},
};
menu_item_t Actuator_IO1_Port3_Items[] = {
    {"ON", MENU_ACTION, actuator_on_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO1_P3,
     NULL, {0}},
    {"OFF", MENU_ACTION, actuator_off_cb, (void *)(uintptr_t)PIN_ACTUATOR_IO1_P3,
     NULL, {0}},
};

#undef PIN_ACTUATOR_IO1_P1
#undef PIN_ACTUATOR_IO3_P2
#undef PIN_ACTUATOR_IO1_P3

menu_list_t Actuator_IO1_Port1_Menu = {
    .items = Actuator_IO1_Port1_Items,
    .count = ARRAY_SIZE(Actuator_IO1_Port1_Items),
    .parent = NULL,
    .port_index = -1,
};
menu_list_t Actuator_IO3_Port2_Menu = {
    .items = Actuator_IO3_Port2_Items,
    .count = ARRAY_SIZE(Actuator_IO3_Port2_Items),
    .parent = NULL,
    .port_index = -1,
};
menu_list_t Actuator_IO1_Port3_Menu = {
    .items = Actuator_IO1_Port3_Items,
    .count = ARRAY_SIZE(Actuator_IO1_Port3_Items),
    .parent = NULL,
    .port_index = -1,
};

menu_item_t Actuators_Items[] = {
    {"IO1-Port1", MENU_SUBMENU, NULL, NULL, &Actuator_IO1_Port1_Menu, {0}},
    {"IO3-Port2", MENU_SUBMENU, NULL, NULL, &Actuator_IO3_Port2_Menu, {0}},
    {"IO1-Port3", MENU_SUBMENU, NULL, NULL, &Actuator_IO1_Port3_Menu, {0}},
};

menu_list_t Actuators_Menu = {
    .items = Actuators_Items,
    .count = ARRAY_SIZE(Actuators_Items),
    .parent = NULL,
    .port_index = NUM_PORTS, /* hiá»ƒn thá»‹ cáº£ Port 1, 2, 3 Ä‘Ã£ chá»n á»Ÿ Ä‘áº§u menu */
};

// Root Menu
menu_item_t Root_Items[] = {
    {"WiFi Config", MENU_SUBMENU, NULL, NULL, &WiFi_Mode_Menu, {0}},
    {"Sensors", MENU_SUBMENU, NULL, NULL, &Sensor_Menu, {0}},
    {"Actuators", MENU_SUBMENU, NULL, NULL, &Actuators_Menu, {0}},
    {"Battery Status", MENU_SUBMENU, NULL, NULL, &Battery_Status_Menu, {0}},
    {"Information", MENU_ACTION, information_callback, NULL, NULL, {0}},
};

menu_list_t Root_Menu = {
    .items = Root_Items,
    .text = {NULL, 0},
    .image = {NULL, 0, 0},
    .object = OBJECT_NONE,
    .count = ARRAY_SIZE(Root_Items),
    .parent = NULL,
    .port_index = -1,
};

// LiÃªn káº¿t parent cho submenu
__attribute__((constructor)) static void link_menus(void) {
  Sensor_Menu.parent = &Root_Menu;
  WiFi_Mode_Menu.parent = &Root_Menu;
  WiFi_Config_Menu.parent = &WiFi_Mode_Menu;
  WiFi_Mesh_Config_Menu.parent = &WiFi_Mode_Menu;
  Battery_Status_Menu.parent = &Root_Menu;
  Actuators_Menu.parent = &Root_Menu;
  Actuator_IO1_Port1_Menu.parent = &Actuators_Menu;
  Actuator_IO3_Port2_Menu.parent = &Actuators_Menu;
  Actuator_IO1_Port3_Menu.parent = &Actuators_Menu;
  Show_Data_Sensor_Menu.parent = &Sensor_Menu;
  for (int p = 0; p < NUM_PORTS; p++) {
    PortMenus[p].parent = &Sensor_Menu;
    for (int i = 0; i < NUM_INTERFACES; i++) {
      PortInterfaceMenus[p][i].parent = &PortMenus[p];
    }
  }
}

/* -------------------- Menu Functions -------------------- */

/**
 * @brief Khá»Ÿi táº¡o menu Sensors: Port 1/2 â†’ UART/I2C/SPI/ANALOG/PULSE â†’ danh
 * sÃ¡ch cáº£m biáº¿n.
 */
static void init_sensor_interface_port_menu_items(DataManager_t *data) {
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    for (int i = 0; i < NUM_INTERFACES; i++) {
      TypeCommunication_t iface = MENU_INTERFACE_ORDER[i];
      size_t count = sensor_registry_get_count_by_interface(iface);

      if (SensorSelectionByIface[port][i]) {
        free(SensorSelectionByIface[port][i]);
        SensorSelectionByIface[port][i] = NULL;
      }
      if (SensorItemsByIface[port][i]) {
        free(SensorItemsByIface[port][i]);
        SensorItemsByIface[port][i] = NULL;
      }

      if (count == 0) {
        PortInterfaceMenus[port][i].items = NULL;
        PortInterfaceMenus[port][i].count = 0;
        PortInterfaceMenus[port][i].parent = &PortMenus[port];
        PortInterfaceMenus[port][i].port_index = -1;
        continue;
      }

      SensorSelectionByIface[port][i] =
          (SelectionParam_t *)malloc(count * sizeof(SelectionParam_t));
      SensorItemsByIface[port][i] =
          (menu_item_t *)malloc(count * sizeof(menu_item_t));
      if (!SensorSelectionByIface[port][i] || !SensorItemsByIface[port][i]) {
        ESP_LOGE(TAG_MENU_SYSTEM, "Failed to allocate for port %d interface %d",
                 (int)port, i);
        continue;
      }

      for (size_t k = 0; k < count; k++) {
        SensorType_t sensor_type;
        sensor_driver_t *driver =
            sensor_registry_get_driver_at_interface(iface, k, &sensor_type);
        if (!driver) {
          continue;
        }
        SensorSelectionByIface[port][i][k] = (SelectionParam_t){
            .data = data,
            .port = port,
            .sensor = sensor_type,
        };
        SensorItemsByIface[port][i][k] = (menu_item_t){
            .name = driver->name,
            .type = MENU_ACTION,
            .callback = select_sensor_cb,
            .ctx = &SensorSelectionByIface[port][i][k],
            .children = NULL,
        };
      }

      PortInterfaceMenus[port][i].items = SensorItemsByIface[port][i];
      PortInterfaceMenus[port][i].count = count;
      PortInterfaceMenus[port][i].parent = &PortMenus[port];
      PortInterfaceMenus[port][i].port_index = -1;
    }

    for (int i = 0; i < NUM_INTERFACES; i++) {
      PortInterfaceMenuItems[port][i] = (menu_item_t){
          .name = MENU_INTERFACE_NAMES[i],
          .type = MENU_SUBMENU,
          .callback = NULL,
          .ctx = NULL,
          .children = &PortInterfaceMenus[port][i],
      };
    }
    PortMenus[port].items = PortInterfaceMenuItems[port];
    PortMenus[port].count = NUM_INTERFACES;
    PortMenus[port].parent = &Sensor_Menu;
    PortMenus[port].port_index =
        (int8_t)port; /* menu Port 1/2/3 hiá»ƒn thá»‹ cáº£m biáº¿n Ä‘Ã£ chá»n */
  }

  Sensor_Menu_Items[0] = (menu_item_t){
      .name = "Port 1",
      .type = MENU_SUBMENU,
      .callback = NULL,
      .ctx = NULL,
      .children = &PortMenus[PORT_1],
  };
  Sensor_Menu_Items[1] = (menu_item_t){
      .name = "Port 2",
      .type = MENU_SUBMENU,
      .callback = NULL,
      .ctx = NULL,
      .children = &PortMenus[PORT_2],
  };
  Sensor_Menu_Items[2] = (menu_item_t){
      .name = "Port 3",
      .type = MENU_SUBMENU,
      .callback = NULL,
      .ctx = NULL,
      .children = &PortMenus[PORT_3],
  };
  Sensor_Menu_Items[3] = (menu_item_t){
      .name = "Show data sensor",
      .type = MENU_SUBMENU,
      .callback = NULL,
      .ctx = NULL,
      .children = &Show_Data_Sensor_Menu,
  };
  Sensor_Menu_Items[4] = (menu_item_t){
      .name = "Reset All Ports",
      .type = MENU_ACTION,
      .callback = reset_all_ports_callback,
      .ctx = NULL, /* gÃ¡n Data trong MenuSystemInit */
      .children = NULL,
  };
}

static void init_show_data_sensor_selection(DataManager_t *data) {
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    ShowDataSensorSelection[port] = (ShowDataSensorParam_t){
        .data = data, .port = port, .ShowDataScreen = false};
  }
}

/** Cáº­p nháº­t tÃªn Sensor_Menu_Items[0..2]: "Port X" hoáº·c "Port X - tÃªn cáº£m biáº¿n"
 * theo data->selectedSensor. */
static void update_port_names(DataManager_t *data) {
  if (data == NULL) {
    return;
  }
  for (int i = 0; i < NUM_PORTS; i++) {
    if (data->selectedSensor[i] == SENSOR_NONE || data->selectedSensor[i] < 0) {
      snprintf(port_display_name[i], PORT_DISPLAY_NAME_LEN, "Port %d", i + 1);
    } else {
      snprintf(port_display_name[i], PORT_DISPLAY_NAME_LEN, "Port %d - %s",
               i + 1,
               sensor_type_to_name((SensorType_t)data->selectedSensor[i]));
    }
    Sensor_Menu_Items[i].name = port_display_name[i];
  }
}

/** Chuyá»ƒn mÃ n hÃ¬nh vá» menu Sensors, highlight má»¥c selected_index (0=Port1,
 * 1=Port2, 2=Port3). */
static void go_to_sensor_menu(DataManager_t *data, int8_t selected_index) {
  if (data == NULL) {
    return;
  }
  data->screen.current = &Sensor_Menu;
  if (selected_index < 0) {
    selected_index = 0;
  }
  if (selected_index >= Sensor_Menu.count) {
    selected_index = Sensor_Menu.count - 1;
  }
  data->screen.selected = selected_index;
  MenuRender(data->screen.current, &data->screen.selected, &data->objectInfo);
}

static void menu_on_sensor_selected(DataManager_t *data, int port) {
  update_port_names(data);
  go_to_sensor_menu(data, (int8_t)port);
}

static void menu_on_ports_reset(DataManager_t *data) {
  update_port_names(data);
}

void MenuSystemInit(DataManager_t *data) {
  Data = data;
  Data->screen.current = &Root_Menu;
  Data->screen.selected = 0;
  Data->screen.prev_selected = 0;
  Data->MenuReturn[0] = &WiFi_Config_Menu;
  Data->MenuReturn[1] = &Battery_Status_Menu;
  Data->MenuReturn[2] = &WiFi_Mesh_Config_Menu;
  // Khá»Ÿi táº¡o selectedSensor
  for (int i = 0; i < NUM_PORTS; ++i) {
    Data->selectedSensor[i] = SENSOR_NONE;
  }

  init_sensor_interface_port_menu_items(Data);
  init_show_data_sensor_selection(Data);

  Show_Data_Sensor[0].name = port[0];
  Show_Data_Sensor[1].name = port[1];
  Show_Data_Sensor[2].name = port[2];

  WiFi_Config_Items[0].ctx = Data;
  WiFi_MeshConfig_Items[0].ctx = Data;  /* Join WiFi Mesh - OK button */
  WiFi_Mode_Items[1].ctx = Data;        /* Join WiFi Mesh submenu */
  Battery_Status_Items[0].ctx = Data;
  Sensor_Menu_Items[3].ctx = Data; /* Show data sensor */
  Sensor_Menu_Items[4].ctx = Data; /* Reset All Ports */

  update_port_names(Data); /* "Port 1", "Port 2", "Port 3" */
  Data->on_sensor_selected = menu_on_sensor_selected;
  Data->on_ports_reset = menu_on_ports_reset;

  // Cáº­p nháº­t callback cho Battery Status menu item trong Root Menu
  Root_Items[3].callback = battery_status_callback;
  Root_Items[3].ctx = Data;
  // Information callback cáº§n Data Ä‘á»ƒ váº½ láº¡i menu sau khi hiá»ƒn thá»‹ thÃ´ng tin
  Root_Items[4].ctx = Data;
}

/* -------------------- Navigation Task -------------------- */
void MenuNavigation_Task(void *pvParameter) {
  DataManager_t *data = (DataManager_t *)pvParameter;

  data->screen.current = &Root_Menu;
  data->screen.selected = 0;
  data->screen.prev_selected = 0;
  for (int p = 0; p < NUM_PORTS; p++) {
    data->objectInfo.selectedSensorName[p] =
        sensor_type_to_name(data->selectedSensor[p]);
  }
  MenuRender(data->screen.current, &data->screen.selected, &data->objectInfo);

  while (1) {
    for (int p = 0; p < NUM_PORTS; p++) {
      data->objectInfo.selectedSensorName[p] =
          sensor_type_to_name(data->selectedSensor[p]);
    }
    switch (ReadButtonStatus()) {
    case BTN_UP:
      data->screen.prev_selected = data->screen.selected;
      data->screen.selected--;

      if (data->screen.selected < 0)
        data->screen.selected = data->screen.current->count - 1;
      MenuRender(data->screen.current, &data->screen.selected,
                 &data->objectInfo);
      break;

    case BTN_DOWN:
      data->screen.prev_selected = data->screen.selected;
      data->screen.selected++;

      if (data->screen.selected >= data->screen.current->count)
        data->screen.selected = 0;
      MenuRender(data->screen.current, &data->screen.selected,
                 &data->objectInfo);
      break;

    case BTN_SEL: {
      menu_item_t *item = &data->screen.current->items[data->screen.selected];

      if (item->type == MENU_ACTION && item->callback) {
        if (data->screen.current == &Show_Data_Sensor_Menu &&
            data->screen.selected < NUM_PORTS) {
          ShowDataSensorSelection[data->screen.selected].ShowDataScreen = true;
        }
        item->callback(item->ctx);
      } else if (item->type == MENU_SUBMENU && item->children) {
        data->screen.current = item->children;
        data->screen.selected = 0;
        MenuRender(data->screen.current, &data->screen.selected,
                   &data->objectInfo);
      }
      break;
    }

    case BTN_BACK:

      if (data->screen.current->parent) {
        if (data->screen.current == &Show_Data_Sensor_Menu &&
            data->screen.selected < NUM_PORTS) {
          ShowDataSensorSelection[data->screen.selected].ShowDataScreen = false;
        }
        data->screen.current = data->screen.current->parent;
        data->screen.selected = 0;
        MenuRender(data->screen.current, &data->screen.selected,
                   &data->objectInfo);
      }
      break;

    default:
      break;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
