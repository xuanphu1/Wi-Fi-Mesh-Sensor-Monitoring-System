# Giải thích chi tiết MenuSystem.c

File này xây dựng **cây menu** của firmware (Root → WiFi Config, Sensors, Actuators, Battery Status, Information) và chạy **task điều hướng** đọc 4 nút (UP, DOWN, SEL, BACK) để di chuyển trong menu và gọi callback.

---

## 1. Hằng số và thứ tự giao tiếp

```c
#define NUM_INTERFACES 5
static const TypeCommunication_t MENU_INTERFACE_ORDER[] = {
  COMMUNICATION_UART, COMMUNICATION_I2C, COMMUNICATION_SPI,
  COMMUNICATION_ANALOG, COMMUNICATION_PULSE,
};
static const char *MENU_INTERFACE_NAMES[] = { "UART", "I2C", "SPI", "ANALOG", "PULSE" };
```

- Có **5 loại giao tiếp** cảm biến: UART, I2C, SPI, ANALOG, PULSE.
- Thứ tự trong menu luôn giống `MENU_INTERFACE_ORDER`; tên hiển thị lấy từ `MENU_INTERFACE_NAMES`.

---

## 2. Con trỏ Data và tên Port

```c
DataManager_t *Data;   // Con trỏ toàn cục tới DataManager (gán trong MenuSystemInit)
static const char *port[NUM_PORTS] = {"Port 1", "Port 2", "Port 3"};
```

- Mọi menu và callback đều dùng **một** bộ dữ liệu chung qua `Data`.
- `port[]` là tên mặc định cho 3 port, dùng cho menu "Show data sensor" và lúc chưa chọn cảm biến.

---

## 3. Mảng động theo Port + giao tiếp

```c
static SelectionParam_t *SensorSelectionByIface[NUM_PORTS][NUM_INTERFACES];  // [port][UART/I2C/...]
static menu_item_t *SensorItemsByIface[NUM_PORTS][NUM_INTERFACES];
ShowDataSensorParam_t ShowDataSensorSelection[NUM_PORTS];

menu_list_t PortInterfaceMenus[NUM_PORTS][NUM_INTERFACES];   // Menu con: danh sách cảm biến
menu_item_t PortInterfaceMenuItems[NUM_PORTS][NUM_INTERFACES];  // 5 mục: "UART", "I2C", ...
menu_list_t PortMenus[NUM_PORTS];  // Port 1 menu, Port 2 menu, Port 3 menu
```

- **Port 1 / 2 / 3**: mỗi port có một `menu_list_t` (`PortMenus[port]`).
- Mỗi port có **5 mục con** tương ứng 5 giao tiếp: UART, I2C, SPI, ANALOG, PULSE (`PortInterfaceMenuItems[port][0..4]`).
- Mỗi mục giao tiếp trỏ tới **menu lá** là danh sách cảm biến thuộc giao tiếp đó (`PortInterfaceMenus[port][i]`).
- Danh sách cảm biến được **cấp phát động** trong `init_sensor_interface_port_menu_items()` từ SensorRegistry (số lượng cảm biến UART/I2C/... có thể thay đổi).
- `SensorSelectionByIface[port][i]`: mảng `SelectionParam_t` (data, port, sensor) truyền vào callback khi chọn cảm biến.
- `SensorItemsByIface[port][i]`: mảng `menu_item_t` (name = tên cảm biến, callback = `select_sensor_cb`, ctx = SelectionParam).
- `ShowDataSensorSelection[port]`: tham số cho menu "Show data sensor" (chọn xem dữ liệu port nào).

---

## 4. Menu Sensors (5 mục)

```c
static menu_item_t Sensor_Menu_Items[5];
menu_list_t Sensor_Menu = { .items = Sensor_Menu_Items, .count = 5, ... };
```

Năm mục (gán trong `init_sensor_interface_port_menu_items`):

| Index | name (cập nhật động) | type      | children / callback |
|-------|----------------------|-----------|----------------------|
| 0     | "Port 1" hoặc "Port 1 - BME280" | MENU_SUBMENU | &PortMenus[PORT_1] |
| 1     | "Port 2" hoặc "Port 2 - ..."   | MENU_SUBMENU | &PortMenus[PORT_2] |
| 2     | "Port 3" hoặc "Port 3 - ..."   | MENU_SUBMENU | &PortMenus[PORT_3] |
| 3     | "Show data sensor"             | MENU_SUBMENU | &Show_Data_Sensor_Menu |
| 4     | "Reset All Ports"              | MENU_ACTION  | reset_all_ports_callback |

- **Sensor_Menu_Items[0..2].name** có thể thay đổi: ban đầu "Port 1", "Port 2", "Port 3"; sau khi chọn cảm biến thành "Port 1 - BME280", ... (do `MenuSystem_UpdatePortNames`).

---

## 5. Show data sensor (3 mục)

```c
menu_item_t Show_Data_Sensor[] = { ... 3 phần tử, callback show_data_sensor_cb, ctx = ShowDataSensorSelection[PORT_1/2/3] };
menu_list_t Show_Data_Sensor_Menu = { .items = Show_Data_Sensor, .count = 3, .parent = &Sensor_Menu };
```

- Ba mục tương ứng Port 1, Port 2, Port 3; mỗi mục gọi `show_data_sensor_cb` với `ShowDataSensorSelection[port]`.
- Tên mục (`Show_Data_Sensor[i].name`) được gán trong init từ `port[]` ("Port 1", "Port 2", "Port 3").

---

## 6. WiFi Config, Battery Status, Actuators, Root

- **WiFi_Config_Menu**: 1 mục "OK", callback `wifi_config_callback`, có image/text (OBJECT_WIFI).
- **Battery_Status_Menu**: 1 mục "OK", callback gán trong init = `battery_status_callback`, có image/text (OBJECT_BATTERY).
- **Actuators_Menu**: 3 mục con — IO1-Port1, IO3-Port2, IO1-Port3; mỗi mục có submenu ON/OFF, callback `actuator_on_cb` / `actuator_off_cb` với ctx = số GPIO (CONFIG_IO_1_PORT_1, ...). `port_index = NUM_PORTS` (không dùng để vẽ dòng Port nữa, chỉ để cấu trúc).
- **Root_Menu**: 5 mục — WiFi Config, Sensors, Actuators, Battery Status, Information (MENU_ACTION, `information_callback`).

Tất cả submenu đều được nối **parent** trong hàm `link_menus()` (chạy tự động trước main nhờ `__attribute__((constructor))`):

- Sensor_Menu, WiFi_Config_Menu, Battery_Status_Menu, Actuators_Menu → parent = &Root_Menu.
- PortMenus[0..2] → parent = &Sensor_Menu.
- PortInterfaceMenus[port][i] → parent = &PortMenus[port].
- Show_Data_Sensor_Menu → parent = &Sensor_Menu.
- Actuator_IO1_Port1_Menu, ... → parent = &Actuators_Menu.

---

## 7. Khởi tạo menu động theo giao tiếp: `init_sensor_interface_port_menu_items(Data)`

Hàm này xây toàn bộ nhánh **Sensors → Port 1/2/3 → UART/I2C/SPI/ANALOG/PULSE → danh sách cảm biến**.

- Với mỗi cặp **(port, interface)**:
  - Gọi `sensor_registry_get_count_by_interface(iface)` để biết số cảm biến (ví dụ I2C có 1 là BME280).
  - Cấp phát:
    - `SensorSelectionByIface[port][i]`: mảng `SelectionParam_t` (data, port, sensor_type).
    - `SensorItemsByIface[port][i]`: mảng `menu_item_t` (name = driver->name, callback = `select_sensor_cb`, ctx = &SensorSelectionByIface[port][i][k]).
  - Gán `PortInterfaceMenus[port][i].items` / `.count` / `.parent` / `.port_index`.
- Với mỗi port: tạo 5 mục "UART", "I2C", "SPI", "ANALOG", "PULSE" (`PortInterfaceMenuItems[port][i]`), mỗi mục `.children = &PortInterfaceMenus[port][i]`.
- Gán `PortMenus[port].items = PortInterfaceMenuItems[port]`, `.count = NUM_INTERFACES`, `.parent = &Sensor_Menu`, `.port_index = port`.
- Cuối cùng gán 5 mục của **Sensor_Menu**: Port 1, Port 2, Port 3 (children = &PortMenus[PORT_1/2/3]), "Show data sensor" (children = &Show_Data_Sensor_Menu), "Reset All Ports" (callback = reset_all_ports_callback).

Nhờ đó menu Sensors **tự lấy danh sách cảm biến từ SensorRegistry** theo giao tiếp, không cần sửa code khi thêm cảm biến mới (chỉ cần đăng ký trong SensorRegistry).

---

## 8. Cập nhật tên Port và quay về menu Sensors (logic nằm ở FunctionManager)

**MenuSystem** chỉ export `extern menu_list_t Sensor_Menu` (trong MenuSystem.h). Toàn bộ logic cập nhật tên và quay về menu Sensors nằm trong **FunctionManager.c**:

- **update_sensor_menu_port_names(Data)**: theo `data->selectedSensor[0..2]` tạo chuỗi "Port 1" / "Port X - tên cảm biến" trong `g_port_label_buf`, rồi gán `Sensor_Menu.items[i].name = g_port_label_buf[i]` (i = 0, 1, 2).
- **go_to_sensor_menu(Data, selected_index)**: đặt `data->screen.current = &Sensor_Menu`, `data->screen.selected = selected_index`, gọi `MenuRender(...)`.

Trong **select_sensor_cb**: sau khi xử lý chọn cảm biến, gọi `update_sensor_menu_port_names(param->data)` rồi `go_to_sensor_menu(param->data, param->port)`. Trong **reset_all_ports_callback**: gọi `update_sensor_menu_port_names(data)` để đưa tên ba mục Port về "Port 1", "Port 2", "Port 3".

---

## 9. MenuSystemInit(Data)

- Lưu `Data = data`.
- Đặt màn hình tại Root: `Data->screen.current = &Root_Menu`, `selected = 0`, `prev_selected = 0`.
- `Data->MenuReturn[0]` = WiFi_Config_Menu, `Data->MenuReturn[1]` = Battery_Status_Menu (dùng khi thoát WiFi config / battery status).
- Khởi tạo `Data->selectedSensor[i] = SENSOR_NONE` cho cả 3 port.
- Gọi `init_sensor_interface_port_menu_items(Data)` (xây menu Sensors động).
- Gọi `init_show_data_sensor_selection(Data)` (điền ShowDataSensorSelection[port] với data, port, ShowDataScreen = false).
- Gán tên cho Show_Data_Sensor[0..2] = "Port 1", "Port 2", "Port 3".
- Gán ctx (Data) cho WiFi_Config_Items[0], Battery_Status_Items[0], Sensor_Menu_Items[3] (Show data sensor), Sensor_Menu_Items[4] (Reset All Ports).
- Gán `Root_Items[3].callback = battery_status_callback`, `Root_Items[3].ctx = Data` (Battery Status). Tên ban đầu "Port 1", "Port 2", "Port 3" đã được gán trong `init_sensor_interface_port_menu_items`; sau khi chọn cảm biến / reset thì FunctionManager cập nhật qua `Sensor_Menu`.

---

## 10. Task điều hướng: MenuNavigation_Task(pvParameter)

Tham số là `DataManager_t *data`. Task chạy vòng lặp vô hạn:

1. **Cập nhật tên cảm biến hiển thị**:  
   `data->objectInfo.selectedSensorName[p] = sensor_type_to_name(data->selectedSensor[p])` cho mọi port (phục vụ màn hình Show data sensor và các nơi dùng selectedSensorName).

2. **Đọc nút**: `switch (ReadButtonStatus())`

   - **BTN_UP**: giảm `data->screen.selected`, nếu &lt; 0 thì vòng về `count - 1`, gọi `MenuRender`.
   - **BTN_DOWN**: tăng `selected`, nếu ≥ count thì về 0, gọi `MenuRender`.
   - **BTN_SEL**:
     - Nếu mục hiện tại là **MENU_ACTION** và có `callback`:  
       Nếu đang ở Show_Data_Sensor_Menu và mục là một trong 3 port thì bật `ShowDataSensorSelection[selected].ShowDataScreen = true`.  
       Gọi `item->callback(item->ctx)`.
     - Nếu mục là **MENU_SUBMENU** và có `children`:  
       Chuyển vào submenu: `data->screen.current = item->children`, `selected = 0`, gọi `MenuRender`.
   - **BTN_BACK**:  
     Nếu `data->screen.current->parent` không NULL: nếu đang ở Show_Data_Sensor_Menu và mục là port thì tắt `ShowDataSensorSelection[selected].ShowDataScreen`.  
     Chuyển về menu cha: `data->screen.current = current->parent`, `selected = 0`, gọi `MenuRender`.

3. **Delay** 10 ms mỗi vòng để tránh chiếm CPU.

Nhờ cấu trúc menu (parent, children, items, count) và các callback (select_sensor_cb, reset_all_ports_callback, wifi_config_callback, ...), toàn bộ điều hướng và hành vi “chọn cảm biến → quay về Sensors với tên Port X - cảm biến” / “Reset → tên về Port 1, 2, 3” đều được xử lý nhất quán trong file này (và FunctionManager qua callback).
