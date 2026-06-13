# MRS_Project

## 📋 Tổng quan

MRS_Project là một ứng dụng ESP-IDF cho ESP32-C6 (hoặc ESP32 series) với các tính năng chính:

- **UI System**: Hiển thị trên màn hình OLED SSD1306 với hệ thống menu phân cấp
- **Sensor Management**: Hệ thống quản lý cảm biến linh hoạt với registry pattern
- **Wi-Fi**: Quản lý Wi-Fi AP/STA với giao diện cấu hình web
- **Network**: Hỗ trợ Mesh networking (đang phát triển)
- **Hardware**: LED RGB WS2812B, RTC DS3231, các cảm biến I2C (BME280, BMP280, ...)

Mã nguồn được tổ chức theo kiến trúc phân lớp (layered architecture) với các domain rõ ràng: `core`, `ui`, `drivers`, `network`, `sensors`, `utils`.

### Thông tin kỹ thuật
- **ESP-IDF Version**: v5.2.5
- **Target Chip**: ESP32-C6 (có thể hỗ trợ ESP32 series khác)
- **Build System**: CMake
- **Partition Table**: OTA dual partition + SPIFFS

### Mục lục
1. Giới thiệu nhanh
2. Cấu trúc thư mục
3. Yêu cầu và Build
4. Cấu hình (SPIFFS, I2C, Kconfig)
5. Quy ước mã nguồn
6. Thêm Cảm biến mới
7. Cách sử dụng Hệ thống Cảm biến
8. Thêm Component mới (không phải cảm biến)
9. Gợi ý mở rộng
10. Khắc phục sự cố nhanh

### 1) Giới thiệu nhanh
- **UI**: Menu điều hướng với `MenuNavigation_Task`, hiển thị dữ liệu cảm biến theo từng trường và đơn vị.
- **Sensors**: Tầng trung gian đọc/định tuyến dữ liệu tới UI, sử dụng registry pattern.
- **Drivers**: i2cdev (khởi tạo I2C idempotent), SSD1306, BME280/BMP280, DS3231/Time.
- **Network**: Wi‑Fi AP/STA với captive portal, cấu hình qua giao diện web (SPIFFS).
- **Error Handling**: Hệ thống quản lý lỗi thống nhất với `system_err_t` và mã lỗi theo module.

#### Khởi tạo hệ thống (main.c)
Hệ thống được khởi tạo theo thứ tự:
1. NVS flash
2. LED RGB (WS2812B)
3. ButtonManager
4. I2C (i2cdev)
5. SSD1306 OLED
6. ScreenManager
7. MenuSystem
8. Tạo tasks: `wifi_init_sta`, `MenuNavigation_Task`

### 2) Cấu trúc thư mục

Dự án được tổ chức theo kiến trúc phân lớp (layered architecture) với các domain rõ ràng:

```
MRS_Project/
├── CMakeLists.txt              # Cấu hình build cấp dự án, khai báo EXTRA_COMPONENT_DIRS
├── sdkconfig                    # File cấu hình ESP-IDF (tự động tạo bởi menuconfig)
├── partitions.csv               # Bảng phân vùng flash (SPIFFS, app, etc.)
├── README.md                    # Tài liệu dự án (file này)
│
├── main/                        # Entry point của ứng dụng
│   ├── CMakeLists.txt          # Cấu hình build cho main component
│   ├── main.c                  # Hàm app_main(), khởi tạo hệ thống, tạo tasks
│   └── main.h                  # Header file cho main
│
└── component/                   # Thư mục chứa tất cả các component
    │
    ├── core/                    # ═══════════════════════════════════════
    │   │                        # LỚP CORE: Logic nghiệp vụ và quản lý dữ liệu
    │   │                        # ═══════════════════════════════════════
    │   ├── DataManager/         # Quản lý dữ liệu toàn cục
    │   │   ├── CMakeLists.txt
    │   │   ├── DataManager.h    # Định nghĩa DataManager_t, ScreenState_t, ObjectInfo_t
    │   │   └── DataManager.c    # Khởi tạo và quản lý trạng thái ứng dụng
    │   │
    │   └── FunctionManager/     # Xử lý callback và logic nghiệp vụ
    │       ├── CMakeLists.txt
    │       ├── FunctionManager.h # Khai báo các callback functions
    │       └── FunctionManager.c # Implement: select_sensor_cb, reset_all_ports_callback,
    │                              #           tạo task đọc cảm biến, xử lý menu actions
    │
    ├── ui/                      # ═══════════════════════════════════════
    │   │                        # LỚP UI: Giao diện người dùng và điều hướng
    │   │                        # ═══════════════════════════════════════
    │   ├── MenuSystem/          # Hệ thống menu phân cấp
    │   │   ├── CMakeLists.txt
    │   │   ├── MenuSystem.h     # Định nghĩa menu_t, menu_item_t, menu callback
    │   │   │                      # API: MenuSystemInit, MenuNavigation_Task
    │   │   └── MenuSystem.c     # Điều hướng menu, render menu items, pagination
    │   │                         # Tự động tạo menu từ SensorRegistry
    │   │                         # Task: MenuNavigation_Task - xử lý điều hướng menu
    │   │
    │   ├── ScreenManager/       # Quản lý hiển thị trên OLED SSD1306
    │   │   ├── CMakeLists.txt
    │   │   ├── ScreenManager.h  # API: ScreenManagerInit, ScreenShowDataSensor, MenuRender
    │   │   └── ScreenManager.c  # Render text, icons, hiển thị dữ liệu cảm biến tuần tự
    │   │                         # (mỗi field 300ms, font size 16, căn giữa)
    │   │
    │   └── ButtonManager/       # Đọc trạng thái nút bấm
    │       ├── CMakeLists.txt
    │       ├── ButtonManager.h  # API: ButtonManagerInit, ReadButtonStatus
    │       └── ButtonManager.c  # Đọc GPIO, phát sinh sự kiện BTN_UP/DOWN/SEL/BACK
    │                            # Tích hợp LED RGB: sáng LED khi bấm nút
    │
    ├── sensors/                 # ═══════════════════════════════════════
    │   │                        # LỚP SENSOR: Định nghĩa và quản lý cảm biến
    │   │                        # ═══════════════════════════════════════
    │   ├── SensorTypes/         # Định nghĩa kiểu dữ liệu cơ bản
    │   │   ├── CMakeLists.txt
    │   │   └── SensorTypes.h    # Enum: PortId_t, SensorType_t
    │   │                         # Struct: SensorData_t, sensor_driver_t
    │   │                         # Định nghĩa tất cả loại cảm biến (BME280, MQ series, ...)
    │   │
    │   ├── SensorConfig/        # Wrapper functions cho sensor drivers
    │   │   ├── CMakeLists.txt
    │   │   ├── SensorConfig.h   # API: SensorConfigInit, SensorConfigRead, SensorConfigDeinit
    │   │   │                      # Wrapper: sensor_bme280_init/read/deinit
    │   │   └── SensorConfig.c   # Implement wrapper functions, gọi driver thực tế
    │   │                         # Đồng bộ cấu hình trong lớp sensor
    │   │
    │   └── SensorRegistry/       # Đăng ký và quản lý danh sách cảm biến
    │       ├── CMakeLists.txt
    │       ├── SensorRegistry.h # API: sensor_registry_get_count, sensor_registry_get_drivers
    │       │                      #       sensor_registry_get_driver, sensor_type_to_name
    │       └── SensorRegistry.c # Mảng sensor_drivers[] chứa tất cả cảm biến đã đăng ký
    │                             # Hàm helper: sensor_type_to_name, get_driver, get_count
    │
    ├── drivers/                 # ═══════════════════════════════════════
    │   │                        # LỚP DRIVER: Driver phần cứng và wrapper
    │   │                        # ═══════════════════════════════════════
    │   ├── i2cdev/              # Driver I2C dùng chung (idempotent)
    │   │   ├── CMakeLists.txt
    │   │   ├── Kconfig.projbuild # Cấu hình GPIO SDA/SCL, port, clock
    │   │   ├── i2cdev.h         # API: i2cInitDevCommon, i2c_dev_t
    │   │   └── i2cdev.c         # Khởi tạo I2C master, tiện ích đọc/ghi I2C
    │   │
    │   ├── ssd1306/             # Driver màn hình OLED SSD1306
    │   │   ├── CMakeLists.txt
    │   │   ├── ssd1306.h        # API: ssd1306_create, ssd1306_draw_string, etc.
    │   │   ├── ssd1306.c        # Giao tiếp I2C với OLED, render pixel/text
    │   │   ├── ssd1306_fonts.h  # Font bitmap
    │   │   └── ssd1306_fonts.c  # Dữ liệu font
    │   │
    │   ├── BME280/              # Driver cảm biến BME280 (nhiệt độ, độ ẩm, áp suất)
    │   │   ├── CMakeLists.txt
    │   │   ├── Kconfig.projbuild
    │   │   ├── bme280.h
    │   │   └── bme280.c
    │   │
    │   ├── BMP280/              # Driver cảm biến BMP280 (nhiệt độ, áp suất)
    │   │   ├── CMakeLists.txt
    │   │   ├── bmp280.h
    │   │   └── bmp280.c
    │   │
    │   ├── DS3231/              # Driver RTC DS3231 (thời gian thực)
    │   │   ├── CMakeLists.txt
    │   │   ├── ds3231.h
    │   │   └── ds3231.c
    │   │
    │   ├── Time/                # Wrapper cho DS3231, quản lý thời gian
    │   │   ├── CMakeLists.txt
    │   │   ├── Kconfig.projbuild
    │   │   ├── DS3231Time.h
    │   │   └── DS3231Time.c
    │   │
    │   ├── LedRGB/              # Driver LED RGB WS2812B (ESP32-C6)
    │   │   ├── CMakeLists.txt
    │   │   ├── Kconfig.projbuild # ACTIVE_LED_RGB (default y cho ESP32-C6)
    │   │   ├── LedRGB.h         # API: LedRGB_Init, LedRGB_SetColor, LedRGB_SetButtonColor
    │   │   └── LedRGB.c         # Sử dụng RMT peripheral, ESP Timer để tắt LED sau delay
    │   │
    │   └── esp_idf_lib_helpers/ # Helper macros cho ESP-IDF lib
    │       ├── CMakeLists.txt
    │       ├── esp_idf_lib_helpers.h
    │       └── ets_sys.h
    │
    ├── network/                 # ═══════════════════════════════════════
    │   │                        # LỚP NETWORK: Wi-Fi, Web và Mesh
    │   │                        # ═══════════════════════════════════════
    │   ├── WifiManager/         # Quản lý Wi-Fi (AP/STA mode)
    │   │   ├── CMakeLists.txt
    │   │   ├── Kconfig.projbuild # Cấu hình SSID, password, AP/STA
    │   │   ├── WifiManager.h    # API: wifi_init_ap, wifi_init_sta (task),
    │   │   │                      #       wifi_connect_handler, update_wifi_status
    │   │   │                      # Tất cả hàm trả về system_err_t với error handling
    │   │   └── WifiManager.c    # Khởi tạo Wi-Fi, HTTP server, xử lý kết nối
    │   │                         # Captive portal, DNS server, web interface
    │   │
    │   ├── MeshManager/         # Quản lý Mesh Network (đang phát triển)
    │   │   └── (thư mục trống, sẵn sàng cho tích hợp ESP-Mesh-Lite)
    │   │
    │   └── WebConfigWifi/       # Tài nguyên web cho cấu hình Wi-Fi
    │       ├── index.html       # Trang cấu hình Wi-Fi (SPIFFS)
    │       └── redirect.html    # Trang redirect sau khi cấu hình
    │
    └── utils/                   # ═══════════════════════════════════════
        │                        # LỚP UTILS: Tiện ích chung
        │                        # ═══════════════════════════════════════
        ├── BitManager/          # Tiện ích xử lý bit/byte
        │   ├── CMakeLists.txt
        │   ├── BitManager.h
        │   └── BitManager.c
        │
        └── ErrorCodes/           # Hệ thống quản lý mã lỗi thống nhất
            ├── CMakeLists.txt
            ├── ErrorCodes.h     # Định nghĩa system_err_t, các mã lỗi theo module
            │                      # API: system_err_to_name, system_err_is_module
            └── ErrorCodes.c     # Implementation các helper functions
```

#### Giải thích chi tiết các lớp:

**1. Lớp Core (`component/core/`)**
- **Mục đích**: Chứa logic nghiệp vụ và quản lý dữ liệu toàn cục
- **DataManager**: Lưu trữ trạng thái ứng dụng (selected sensors, screen state, object info)
- **FunctionManager**: Xử lý các callback từ menu, tạo tasks đọc cảm biến, reset ports

**2. Lớp UI (`component/ui/`)**
- **Mục đích**: Giao diện người dùng và điều hướng
- **MenuSystem**: Hệ thống menu phân cấp, tự động tạo menu từ SensorRegistry, hỗ trợ pagination
  - Task: `MenuNavigation_Task` - xử lý điều hướng menu và button events
- **ScreenManager**: Render UI lên OLED (text, icons, dữ liệu cảm biến)
  - Tất cả hàm trả về `system_err_t` với error handling
- **ButtonManager**: Đọc GPIO nút bấm, tích hợp LED RGB feedback

**3. Lớp Sensors (`component/sensors/`)**
- **Mục đích**: Định nghĩa và quản lý danh sách cảm biến
- **SensorTypes**: Định nghĩa enum và struct cơ bản (PortId_t, SensorType_t, SensorData_t, sensor_driver_t)
- **SensorConfig**: Wrapper functions cho các sensor driver cụ thể (sensor_bme280_init/read/deinit), đồng bộ cấu hình
- **SensorRegistry**: Đăng ký tất cả cảm biến, cung cấp API truy cập danh sách (get_count, get_drivers, get_driver)

**4. Lớp Drivers (`component/drivers/`)**
- **Mục đích**: Driver phần cứng trực tiếp giao tiếp với thiết bị
- **i2cdev**: Khởi tạo I2C dùng chung (idempotent)
- **ssd1306**: Driver màn hình OLED
- **BME280, BMP280, DS3231**: Driver cảm biến cụ thể
- **LedRGB**: Driver LED RGB WS2812B (ESP32-C6), sử dụng RMT peripheral

**5. Lớp Network (`component/network/`)**
- **Mục đích**: Quản lý Wi-Fi, web interface và mesh networking
- **WifiManager**: Khởi tạo Wi-Fi AP/STA, HTTP server, captive portal
  - Tất cả hàm trả về `system_err_t` với error handling đầy đủ
  - API: `wifi_init_ap()`, `wifi_init_sta()` (task), `wifi_connect_handler()`, `update_wifi_status()`
  - Hỗ trợ DNS server cho captive portal, web interface cấu hình Wi-Fi
- **MeshManager**: Quản lý mesh network (đang phát triển, có thể tích hợp ESP-Mesh-Lite)
- **WebConfigWifi**: Tài nguyên HTML cho cấu hình Wi-Fi qua web (lưu trong SPIFFS)

**6. Lớp Utils (`component/utils/`)**
- **Mục đích**: Tiện ích chung
- **BitManager**: Xử lý bit/byte operations
- **ErrorCodes**: Hệ thống quản lý mã lỗi thống nhất cho toàn dự án
  - Định nghĩa `system_err_t` (tương thích với `esp_err_t`)
  - Mã lỗi được tổ chức theo module (Core, Sensors, UI, Network, Drivers, Utils)
  - Helper functions: `system_err_to_name()`, `system_err_is_module()`, etc.

#### Luồng dữ liệu:

```
User Input (Button) 
    ↓
MenuButton → MenuSystem → FunctionManager
    ↓                              ↓
ScreenManager ← SensorRegistry ← SensorConfig ← Driver (BME280, etc.)
    ↓
SSD1306 OLED Display
```

#### Dependency Flow:

- **UI Layer** phụ thuộc vào **Core Layer** và **Sensors Layer**
- **Sensors Layer** phụ thuộc vào **Drivers Layer**
- **Core Layer** phụ thuộc vào **Sensors Layer** và **Drivers Layer**
- **Drivers Layer** độc lập, chỉ phụ thuộc vào ESP-IDF và hardware

### 3) Yêu cầu và Build

#### Yêu cầu hệ thống
- **ESP-IDF**: v5.2.5 hoặc mới hơn
- **Python**: 3.8+ (yêu cầu của ESP-IDF)
- **Git**: Để clone submodules nếu có
- **CMake**: 3.16+ (tự động đi kèm với ESP-IDF)
- **Toolchain**: ESP-IDF toolchain cho ESP32-C6

#### Thiết lập môi trường

1. **Cài đặt ESP-IDF v5.2.5**:
   ```bash
   # Clone ESP-IDF
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   git checkout v5.2.5
   git submodule update --init --recursive
   
   # Cài đặt toolchain
   ./install.sh esp32c6
   
   # Export environment
   . ./export.sh  # Linux/Mac
   # hoặc
   export.bat     # Windows
   ```

2. **Clone dự án**:
   ```bash
   git clone <repository-url>
   cd MRS_Project
   ```

#### Build và Flash

```bash
# Thiết lập target chip
idf.py set-target esp32c6

# Cấu hình dự án (tùy chọn)
idf.py menuconfig

# Build dự án
idf.py build

# Flash và monitor (thay COMx bằng port serial của bạn)
idf.py -p COMx flash monitor

# Hoặc flash riêng
idf.py -p COMx flash
idf.py -p COMx monitor
```

#### Các lệnh hữu ích khác

```bash
# Xóa build
idf.py fullclean

# Flash SPIFFS partition
idf.py -p COMx spiffs-flash

# Xem kích thước binary
idf.py size

# Xem cấu trúc component
idf.py show_efuse_table
```

### 4) Cấu hình

#### 4.1) Partition Table

Dự án sử dụng partition table với OTA dual partition và SPIFFS:

```
nvs      : 0x9000  - 0x5000  (20 KB)  - Non-volatile storage
otadata  : 0xe000  - 0x2000  (8 KB)   - OTA data
ota_0    : 0x10000 - 0x1C0000 (1.75 MB) - App partition 0
ota_1    :         - 0x1C0000 (1.75 MB) - App partition 1
spiffs   :         - 0x20000  (128 KB) - SPIFFS filesystem
```

File cấu hình: `partitions.csv`

#### 4.2) SPIFFS Configuration

SPIFFS được tự động tạo từ `component/network/WebConfigWifi` trong `CMakeLists.txt`:

```cmake
spiffs_create_partition_image(spiffs component/network/WebConfigWifi FLASH_IN_PROJECT)
```

Để flash SPIFFS riêng:
```bash
idf.py -p COMx spiffs-flash
```

#### 4.3) I2C Configuration

I2C được quản lý bởi `drivers/i2cdev` với khởi tạo idempotent (có thể gọi nhiều lần an toàn).

Cấu hình qua `idf.py menuconfig`:
- **Component config** → **i2cdev** → Cấu hình GPIO SDA, SCL, Port, Clock speed

Mặc định:
- **Port**: I2C_NUM_0
- **SDA**: GPIO 8 (ESP32-C6)
- **SCL**: GPIO 9 (ESP32-C6)
- **Clock**: 100kHz

#### 4.4) Wi-Fi Configuration

Cấu hình Wi-Fi qua `idf.py menuconfig`:
- **Component config** → **WifiManager** → Cấu hình SSID, password, AP/STA mode

Hoặc cấu hình qua web interface:
1. Kết nối vào AP của thiết bị
2. Mở trình duyệt tại `192.168.4.1`
3. Cấu hình SSID và password Wi-Fi

#### 4.5) LED RGB Configuration

LED RGB WS2812B được cấu hình qua `idf.py menuconfig`:
- **Component config** → **LedRGB** → Cấu hình GPIO pin, số lượng LED

Mặc định cho ESP32-C6:
- **GPIO**: GPIO 2
- **Active**: Enabled (y)

#### 4.6) Kconfig

Mỗi component có thể cung cấp `Kconfig.projbuild` để thêm cấu hình vào menuconfig:
- `drivers/i2cdev/Kconfig.projbuild` - Cấu hình I2C
- `drivers/LedRGB/Kconfig.projbuild` - Cấu hình LED RGB
- `network/WifiManager/Kconfig.projbuild` - Cấu hình Wi-Fi
- `drivers/Time/Kconfig.projbuild` - Cấu hình RTC

Chạy `idf.py menuconfig` để điều chỉnh các cấu hình này.

### 5) Quy ước mã nguồn
- Phân lớp rõ ràng: `ui` (hiển thị/điều hướng), `sensors` (điều phối đọc), `drivers` (driver thiết bị), `core` (dữ liệu/chức năng), `network` (Wi‑Fi/Web), `utils` (tiện ích).
- Callback UI tập trung ở `FunctionManager` (core); `MenuSystem` chỉ gán callback và context.
- Tránh overflow/ghi vượt mảng; luôn kiểm tra giới hạn chỉ số trước khi truy cập.
- Khởi tạo idempotent cho mọi hạ tầng (I2C, Wi‑Fi, …) để gọi lặp an toàn.
- **Error Handling**: Sử dụng `system_err_t` thay vì `esp_err_t` cho các hàm trong dự án
  - Tất cả hàm khởi tạo và xử lý quan trọng trả về `system_err_t`
  - Kiểm tra và xử lý lỗi đầy đủ, sử dụng `system_err_to_name()` để log lỗi
  - Mã lỗi được định nghĩa trong `ErrorCodes.h` theo từng module

### 6) Thêm Cảm biến mới

Hệ thống sử dụng kiến trúc 3 lớp để quản lý cảm biến:
- **SensorTypes**: Định nghĩa enum và struct cơ bản
- **SensorRegistry**: Đăng ký và quản lý danh sách cảm biến
- **SensorConfig**: Wrapper functions cho các driver cụ thể

#### 6.1) Thêm enum vào SensorTypes.h

Mở `component/sensors/SensorTypes/SensorTypes.h` và thêm enum mới vào `SensorType_t`:

```c
typedef enum {
  SENSOR_NONE = -1,
  SENSOR_BME280 = 0,
  // ... các cảm biến khác
  SENSOR_MY_NEW_SENSOR = 13,  // Thêm enum mới
} SensorType_t;
```

#### 6.2) Tạo Wrapper Functions trong SensorConfig

Mở `component/sensors/SensorConfig/SensorConfig.h` và thêm khai báo:

```c
void sensor_my_new_sensor_init(void);
void sensor_my_new_sensor_read(SensorData_t *data);
void sensor_my_new_sensor_deinit(void);
```

Mở `component/sensors/SensorConfig/SensorConfig.c` và implement:

```c
void sensor_my_new_sensor_init(void) {
  // Khởi tạo sensor của bạn
  // Ví dụ: i2c_init, gpio_config, etc.
}

void sensor_my_new_sensor_read(SensorData_t *data) {
  // Đọc dữ liệu từ sensor
  // Lưu vào data->data_fl[0], data->data_fl[1], ...
  // Hoặc data->data_uint32[], data->data_uint16[], tùy loại dữ liệu
}

void sensor_my_new_sensor_deinit(void) {
  // Giải phóng tài nguyên nếu cần
}
```

#### 6.3) Đăng ký vào SensorRegistry

Mở `component/sensors/SensorRegistry/SensorRegistry.c` và thêm vào mảng `sensor_drivers[]`:

```c
static sensor_driver_t sensor_drivers[] = {
    // ... các cảm biến hiện có
    {
        .name = "My New Sensor",           // Tên hiển thị trong menu
        .init = sensor_my_new_sensor_init, // Hàm khởi tạo
        .read = sensor_my_new_sensor_read, // Hàm đọc dữ liệu
        .deinit = sensor_my_new_sensor_deinit, // Hàm giải phóng (có thể NULL)
        .description = {"Field1", "Field2"},   // Tên các trường dữ liệu
        .unit = {"unit1", "unit2"},           // Đơn vị đo
        .unit_count = 2,                       // Số lượng trường
        .is_init = false,                      // Trạng thái khởi tạo
    },
};
```

Cập nhật hàm `sensor_type_to_name()` để thêm case mới:

```c
const char *sensor_type_to_name(SensorType_t t) {
  switch (t) {
    // ... các case hiện có
    case SENSOR_MY_NEW_SENSOR:
      return "My New Sensor";
    default:
      return "Unknown";
  }
}
```

#### 6.4) Ví dụ: Thêm cảm biến với hàm NULL

Nếu cảm biến chưa có implementation, có thể đăng ký với các hàm NULL:

```c
{
    .name = "MQ-2",
    .init = NULL,      // Chưa implement
    .read = NULL,      // Chưa implement
    .deinit = NULL,    // Chưa implement
    .description = {"Gas"},
    .unit = {"ppm"},
    .unit_count = 1,
    .is_init = false,
},
```

Cảm biến này sẽ xuất hiện trong menu nhưng chưa thể sử dụng cho đến khi implement các hàm.

### 7) Cách sử dụng Hệ thống Cảm biến

#### 7.1) Lấy danh sách cảm biến

```c
#include "SensorRegistry.h"

// Lấy số lượng cảm biến đã đăng ký
size_t count = sensor_registry_get_count();

// Lấy mảng các driver
sensor_driver_t *drivers = sensor_registry_get_drivers();

// Lấy driver cụ thể theo loại
sensor_driver_t *driver = sensor_registry_get_driver(SENSOR_BME280);
if (driver != NULL) {
    // Sử dụng driver
}
```

#### 7.2) Chọn cảm biến cho Port

Hệ thống tự động tạo menu cho việc chọn cảm biến:
1. Vào menu "Sensors" → "Port Config"
2. Chọn Port (Port 1, Port 2, Port 3)
3. Chọn cảm biến từ danh sách (tự động từ SensorRegistry)
4. Hệ thống sẽ tự động khởi tạo cảm biến nếu chưa được init

#### 7.3) Đọc dữ liệu cảm biến

```c
#include "SensorConfig.h"
#include "SensorTypes.h"

SensorData_t data;
sensor_driver_t *driver = sensor_registry_get_driver(SENSOR_BME280);

if (driver != NULL && driver->read != NULL) {
    driver->read(&data);
    // Dữ liệu được lưu trong:
    // - data.data_fl[] cho số thực (float)
    // - data.data_uint32[] cho số nguyên 32-bit
    // - data.data_uint16[] cho số nguyên 16-bit
    // - data.data_uint8[] cho số nguyên 8-bit
}
```

#### 7.4) Hiển thị dữ liệu trên màn hình

```c
#include "ScreenManager.h"

sensor_driver_t *driver = sensor_registry_get_driver(SENSOR_BME280);
SensorData_t data;
driver->read(&data);

// Hiển thị với tên trường, giá trị, và đơn vị
ScreenShowDataSensor(
    driver->description,  // Tên các trường
    data.data_fl,         // Mảng giá trị (float)
    driver->unit,         // Đơn vị
    driver->unit_count    // Số lượng trường
);
```

#### 7.5) Pagination trong Menu

Khi có nhiều hơn 4 cảm biến, menu tự động hỗ trợ phân trang:
- **Items 0-3**: Hiển thị cảm biến 0, 1, 2, 3
- **Khi chọn item 4**: Màn hình tự động scroll để hiển thị từ cảm biến 4 trở đi
- **Tiếp tục scroll**: Mỗi lần chọn item mới >= 4, màn hình sẽ hiển thị 4 items tiếp theo

### 8) Thêm Component mới (không phải cảm biến)
1. Tạo thư mục dưới domain phù hợp, ví dụ `component/ui/MyWidget`.
2. Thêm `CMakeLists.txt` với `idf_component_register(...)` và `REQUIRES` chính xác.
3. Nếu cần cấu hình, thêm `Kconfig.projbuild` trong thư mục component.
4. Sử dụng ở nơi khác qua `REQUIRES` và include header tương ứng.

### 9) Gợi ý mở rộng

#### 9.1) Tài liệu Component
- Viết `README.md` ngắn trong mỗi component mô tả API, dependency, Kconfig
- Thêm comments trong code theo chuẩn Doxygen

#### 9.2) Code Quality
- Thêm `clang-format`/`clang-tidy` và pre-commit hook để chuẩn hóa style
- Thiết lập `.clang-format` và `.clang-tidy` trong root project
- Sử dụng ESP-IDF coding style guide

#### 9.3) Testing
- Thêm unit test cho logic thuần (mock hardware) nếu có CI
- Sử dụng Unity framework (đã có trong ESP-IDF)
- Test các hàm utility và business logic

#### 9.4) Mesh Networking
- Tích hợp ESP-Mesh-Lite cho ESP-IDF v5.x (xem `INTEGRATE_ESP_MDF.md` để biết thêm)
- Hoặc sử dụng ESP-WIFI-MESH API trực tiếp từ ESP-IDF
- Implement MeshManager component trong `component/network/MeshManager/`

#### 9.5) OTA Updates
- Dự án đã có partition table hỗ trợ OTA
- Có thể tích hợp ESP HTTPS OTA hoặc custom OTA server
- Thêm menu item để kiểm tra và cập nhật firmware

#### 9.6) Logging và Debugging
- Sử dụng ESP_LOG với các mức độ phù hợp (ERROR, WARN, INFO, DEBUG, VERBOSE)
- Thêm remote logging qua Wi-Fi hoặc serial
- Implement debug menu trong MenuSystem

#### 9.7) Power Management
- Thêm deep sleep mode cho tiết kiệm năng lượng
- Implement wake-up từ button hoặc RTC
- Cấu hình CPU frequency scaling

### 10) Khắc phục sự cố nhanh

#### 10.1) Build Errors

**Lỗi "không tìm thấy component"**:
- Kiểm tra đã thêm đường dẫn domain trong `CMakeLists.txt` → `EXTRA_COMPONENT_DIRS`
- Kiểm tra `REQUIRES` trong `CMakeLists.txt` của component đúng tên component
- Chạy `idf.py fullclean` và build lại

**Lỗi "undefined reference"**:
- Kiểm tra component đã được thêm vào `REQUIRES` trong `CMakeLists.txt`
- Kiểm tra header file được include đúng cách
- Kiểm tra implementation file (.c) được compile

#### 10.2) Runtime Errors

**Lỗi I2C driver "not installed"**:
- Đảm bảo gọi `i2cInitDevCommon()` trước khi sử dụng sensor/OLED
- Không khởi tạo I2C trùng lặp (đã idempotent nhưng nên gọi một lần)
- Kiểm tra GPIO SDA/SCL đã cấu hình đúng trong menuconfig

**OLED không hiển thị**:
- Kiểm tra kết nối I2C (SDA, SCL, VCC, GND)
- Kiểm tra địa chỉ I2C của SSD1306 (thường là 0x3C hoặc 0x3D)
- Kiểm tra `MainScreen` đã được khởi tạo thành công

**Wi-Fi không kết nối**:
- Kiểm tra SSID và password đã cấu hình đúng
- Kiểm tra signal strength của AP
- Xem log qua serial monitor để debug

#### 10.3) UI Issues

**UI hiển thị sai sau thao tác menu**:
- Kiểm tra chỉ số mảng bám theo `NUM_PORTS`/`sensor_registry_get_count()` trước khi ghi
- Kiểm tra bounds checking trong MenuSystem
- Kiểm tra pagination logic khi có nhiều items

**Menu không phản hồi button**:
- Kiểm tra GPIO button đã cấu hình đúng
- Kiểm tra ButtonManager đã được khởi tạo
- Kiểm tra callback đã được gán đúng trong MenuSystem

#### 10.4) Sensor Issues

**Sensor không đọc được dữ liệu**:
- Kiểm tra sensor đã được khởi tạo (`driver->init()`)
- Kiểm tra kết nối I2C và địa chỉ I2C của sensor
- Kiểm tra sensor đã được đăng ký trong SensorRegistry
- Xem log để kiểm tra lỗi cụ thể

**Sensor hiển thị giá trị sai**:
- Kiểm tra hàm `read()` đã implement đúng
- Kiểm tra dữ liệu được lưu vào đúng field trong `SensorData_t`
- Kiểm tra unit và description đã khai báo đúng

#### 10.5) SPIFFS Issues

**Web interface không load**:
- Kiểm tra SPIFFS đã được flash: `idf.py -p COMx spiffs-flash`
- Kiểm tra file HTML có trong `component/network/WebConfigWifi/`
- Kiểm tra partition table có partition SPIFFS

#### 10.6) Debug Tips

- Sử dụng `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` để debug
- Xem log qua serial monitor: `idf.py -p COMx monitor`
- Sử dụng `idf.py size` để kiểm tra kích thước binary
- Kiểm tra heap memory: `esp_get_free_heap_size()`

### 11) Tài liệu tham khảo

- **ESP-IDF Programming Guide**: https://docs.espressif.com/projects/esp-idf/en/latest/
- **ESP-IDF API Reference**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/
- **ESP32-C6 Datasheet**: https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf
- **ESP-Mesh-Lite**: https://github.com/espressif/esp-mesh-lite (khuyến nghị cho ESP-IDF v5.x)
- **ESP-WIFI-MESH**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp-wifi-mesh.html

### 12) Tích hợp ESP-MDF (Không khuyến nghị)

⚠️ **Lưu ý**: ESP-MDF đang ở trạng thái "limited maintenance" và được thiết kế cho ESP-IDF v4.x. Espressif khuyến nghị sử dụng **ESP-Mesh-Lite** cho các dự án mới với ESP-IDF v5.x.

Xem file `INTEGRATE_ESP_MDF.md` để biết hướng dẫn tích hợp ESP-MDF (nếu cần thiết).

---

**Phiên bản**: 1.1  
**Cập nhật lần cuối**: 2024

### Changelog

#### Version 1.1
- ✅ Đổi tên `NavigationScreen_Task` → `MenuNavigation_Task` để phản ánh đúng chức năng điều hướng menu
- ✅ Chuyển tất cả hàm trong `WifiManager` sang `system_err_t` với error handling đầy đủ
  - `wifi_init_ap()`: Trả về `system_err_t` với kiểm tra lỗi chi tiết
  - `wifi_connect_handler()`: Thêm validation SSID/password, error handling
  - `update_wifi_status()`: Thêm kiểm tra tham số và trạng thái
- ✅ Thêm hệ thống quản lý lỗi thống nhất (`ErrorCodes` component)
  - Định nghĩa `system_err_t` (tương thích với `esp_err_t`)
  - Mã lỗi được tổ chức theo module (Core, Sensors, UI, Network, Drivers, Utils)
  - Helper functions: `system_err_to_name()`, `system_err_is_module()`, etc.
- ✅ Cập nhật `ButtonManager` (đổi tên từ `MenuButton`)
- ✅ Cải thiện error handling trong `ScreenManager` và `FunctionManager`
- ✅ Cập nhật tài liệu API và cấu trúc component


