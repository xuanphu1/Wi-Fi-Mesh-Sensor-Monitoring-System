# Hướng dẫn tích hợp ESP-MDF vào MRS_Project

## ⚠️ LƯU Ý QUAN TRỌNG
- ESP-MDF đang ở trạng thái "limited maintenance"
- ESP-MDF được thiết kế cho ESP-IDF v4.x, có thể không tương thích tốt với ESP-IDF v5.2.5
- Espressif khuyến nghị sử dụng **ESP-Mesh-Lite** cho các dự án mới

## Các bước tích hợp ESP-MDF

### Bước 1: Clone ESP-MDF repository

```bash
# Clone ESP-MDF vào thư mục bên ngoài dự án
cd D:\esp_5_2
git clone --recursive https://github.com/espressif/esp-mdf.git
```

### Bước 2: Cập nhật CMakeLists.txt

Thêm đường dẫn đến ESP-MDF vào `CMakeLists.txt` của dự án:

```cmake
cmake_minimum_required(VERSION 3.16)

# Thêm đường dẫn đến ESP-MDF components
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/component/core
    ${CMAKE_CURRENT_LIST_DIR}/component/ui
    ${CMAKE_CURRENT_LIST_DIR}/component/drivers
    ${CMAKE_CURRENT_LIST_DIR}/component/network
    ${CMAKE_CURRENT_LIST_DIR}/component/sensors
    ${CMAKE_CURRENT_LIST_DIR}/component/utils
    D:/esp_5_2/esp-mdf/components  # Đường dẫn đến ESP-MDF components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MRS_Project)
```

### Bước 3: Cấu trúc ESP-MDF

ESP-MDF bao gồm các components chính:
- `mdf` - Core framework
- `mwifi` - WiFi Mesh wrapper
- `mconfig` - Configuration management
- `mupgrade` - OTA upgrade
- `mdebug` - Debug tools
- `mcommon` - Common utilities

### Bước 4: Sử dụng ESP-MDF trong code

```c
#include "mdf_common.h"
#include "mwifi.h"
#include "mconfig.h"

void app_main(void) {
    // Khởi tạo ESP-MDF
    mdf_err_t ret = mdf_info_init();
    MDF_ERROR_ASSERT(ret != MDF_OK);
    
    // Khởi tạo WiFi Mesh
    ret = mwifi_init();
    MDF_ERROR_ASSERT(ret != MDF_OK);
    
    // Bắt đầu Mesh
    ret = mwifi_start();
    MDF_ERROR_ASSERT(ret != MDF_OK);
    
    // ... rest of your code
}
```

### Bước 5: Cấu hình ESP-MDF

Chạy menuconfig để cấu hình ESP-MDF:
```bash
idf.py menuconfig
```

Tìm menu:
- `Component config` -> `MDF Configuration`
- `Component config` -> `MDF WiFi Configuration`

## Vấn đề tương thích với ESP-IDF v5.2

ESP-MDF có thể gặp các vấn đề sau với ESP-IDF v5.2:
1. API changes trong ESP-IDF v5.x
2. Thay đổi trong WiFi API
3. Thay đổi trong event system
4. Thay đổi trong NVS API

## Giải pháp thay thế được khuyến nghị

### Option 1: Sử dụng ESP-Mesh-Lite (Khuyến nghị)
- Tương thích tốt với ESP-IDF v5.x
- Nhẹ hơn và đơn giản hơn
- Đang được phát triển tích cực

### Option 2: Sử dụng ESP-WIFI-MESH trực tiếp
- Sử dụng API `esp_mesh.h` từ ESP-IDF
- Không cần framework bên ngoài
- Hoàn toàn tương thích với ESP-IDF v5.2

## Tài liệu tham khảo

- ESP-MDF GitHub: https://github.com/espressif/esp-mdf
- ESP-MDF Documentation: https://docs.espressif.com/projects/esp-mdf/en/latest/
- ESP-Mesh-Lite: https://github.com/espressif/esp-mesh-lite
- ESP-WIFI-MESH API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp-wifi-mesh.html

