# Wi-Fi Mesh Sensor Monitoring System (Engineering Project)

## 📋 Tổng quan (Overview)

**Wi-Fi Mesh Sensor Monitoring System** là hệ thống giám sát và theo dõi dữ liệu cảm biến thời gian thực dựa trên công nghệ mạng **Wi-Fi Mesh** (ESP-Mesh-Lite / No Router Mesh). Hệ thống cho phép các nút cảm biến vi điều khiển tự tổ chức mạng không dây, truyền dữ liệu cảm biến về Gateway trung tâm và hiển thị dữ liệu trực quan trên nhiều nền tảng: **Web Dashboard**, **Mobile App (React Native/Expo)** và **Python Tools/Simulator**.

---

## 🏗️ Kiến trúc hệ thống (System Architecture)

Luồng dữ liệu (Data Flow) trong hệ thống:

```text
       [ Cảm biến: BME280 / AHT10 / MQ / DS3231 ]
                           |
                           v
           [ Mesh Node (ESP32 / ESP32-C6) ]
               (MSMS_Project - OLED UI)
                           |
                     (Wi-Fi Mesh)
                           |
                           v
            [ Mesh Gateway / Root Node ]
                   (MeshGateWay)
                           |
              +------------+------------+
              | (WebSocket / HTTP / IP) |
              v                         v
   [ Web Dashboard & WS Server ]   [ Mobile App & Python Server ]
    (DashboardManagerWifiMesh)            (App_React)
         (Port: 3000 / 9090)          (Port: 8080 / 8765)
```

1. **Mesh Sensor Nodes ([MSMS_Project](file:///d:/Engineering%20Project/MSMS_Project)):** Đọc chỉ số cảm biến (Nhiệt độ, Độ ẩm, Áp suất, Time,...), hiển thị trực tiếp lên màn hình OLED SSD1306, điều khiển LED WS2812B và đóng gói gửi dữ liệu qua mạng Mesh.
2. **Mesh Gateway ([MeshGateWay](file:///d:/Engineering%20Project/MeshGateWay)):** Node trung tâm (Root Node) thu thập dữ liệu từ tất cả các Node con và định tuyến dữ liệu ra mạng ngoài (LAN/WAN) qua giao thức WebSocket / HTTP.
3. **Web Dashboard & Server ([DashboardManagerWifiMesh](file:///d:/Engineering%20Project/DashboardManagerWifiMesh)):** Giao diện Web hiển thị Dashboard, quản lý Node, biểu đồ realtime và WebSocket backend (Node.js/Express) lưu trữ SQLite/MongoDB.
4. **Mobile App & Backend Simulator ([App_React](file:///d:/Engineering%20Project/App_React)):** Ứng dụng di động đa nền tảng (Android/iOS/Web) xây dựng bằng Expo Router & Zustand, kết nối song song với máy chủ [PythonServer](file:///d:/Engineering%20Project/App_React/PythonServer).

---

## 📂 Cấu trúc thư mục (Directory Structure)

| Thư mục / Thành phần | Công nghệ / Nền tảng | Chức năng chính |
| :--- | :--- | :--- |
| 🔌 **[MSMS_Project](file:///d:/Engineering%20Project/MSMS_Project)** | ESP-IDF (v5.2+) / C | Firmware cho các Mesh Sensor Node (ESP32-C6 / ESP32). Xử lý cảm biến, OLED UI, WS2812B, Mesh-Lite. |
| 📡 **[MeshGateWay](file:///d:/Engineering%20Project/MeshGateWay)** | ESP-IDF (v5.2+) / C | Firmware cho Root Gateway Node. Nhận dữ liệu Mesh và truyền tải lên máy chủ. |
| 💻 **[DashboardManagerWifiMesh](file:///d:/Engineering%20Project/DashboardManagerWifiMesh)** | React (MUI) / Node.js | Web Dashboard theo dõi mạng lưới + WebSocket server (`websocket-server/` Port 9090). |
| 📱 **[App_React](file:///d:/Engineering%20Project/App_React)** | React Native / Expo 54 | App di động giám sát cảm biến, cảnh báo ngưỡng và quản lý lịch sử. |
| 🐍 **[App_React/PythonServer](file:///d:/Engineering%20Project/App_React/PythonServer)** | Python 3.x / asyncio | Backend & Mock Gateway Server (WebSocket port 8765, HTTP API port 8080, SQLite database `history.db`). |
| 📄 **[Report](file:///d:/Engineering%20Project/Report)** | Documentation | Báo cáo chi tiết, sơ đồ thiết kế và tài liệu kĩ thuật của đề tài. |

---

## 🛠️ Yêu cầu môi trường (Prerequisites)

Trước khi khởi chạy các thành phần, vui lòng đảm bảo máy tính đã cài đặt các công cụ sau:

1. **ESP-IDF (v5.2 trở lên):** Dùng để nạp firmware cho vi điều khiển ESP32 / ESP32-C6.
2. **Node.js (v18+ LTS hoặc v20+):** Dùng để chạy Web Dashboard và Mobile App.
3. **Python (v3.9 trở lên):** Dùng để chạy Backend giả lập & các kịch bản kiểm thử.
4. **Ứng dụng Expo Go:** Cài đặt trên điện thoại (Android hoặc iOS) để test app di động.

---

## 🚀 Hướng dẫn khởi chạy chi tiết (Step-by-Step Guide)

### 1️⃣ Khởi chạy Web Dashboard & WebSocket Server (`DashboardManagerWifiMesh`)

Thành phần này gồm Web React (Frontend) và WebSocket Server Node.js (Backend).

#### Cách A: Chạy nhanh bằng File Script (Khuyên dùng trên Windows)
1. Truy cập thư mục [DashboardManagerWifiMesh](file:///d:/Engineering%20Project/DashboardManagerWifiMesh).
2. Tải và cài thư viện (chỉ cần làm lần đầu):
   ```bash
   cd DashboardManagerWifiMesh
   npm install
   ```
3. Chạy file batch:
   ```cmd
   execute.bat
   ```
   *Script sẽ khởi tạo cả Web Frontend (`http://localhost:3000`) và WebSocket Server (`ws://localhost:9090/ws`). Để dừng dịch vụ, chạy `stop.bat`.*

#### Cách B: Khởi chạy thủ công bằng Lệnh NPM
```bash
cd DashboardManagerWifiMesh

# Cài đặt phụ thuộc (lần đầu)
npm install

# Khởi chạy đồng thời cả Web Frontend và WebSocket Server:
npm run dev
```
*Giao diện Web sẽ mở tại: `http://localhost:3000`.*

---

### 2️⃣ Khởi chạy Mobile App & Python Backend Simulator (`App_React`)

Hệ thống Mobile App đi kèm một Python Backend Server giúp mô phỏng Gateway đẩy dữ liệu cảm biến thời gian thực và quản lý cơ sở dữ liệu SQLite.

#### Bước 2.1: Khởi chạy Python Backend Server (Gateway Simulator)
Mở cửa sổ Terminal thứ nhất:
```bash
# 1. Trỏ vào thư mục PythonServer
cd "App_React/PythonServer"

# 2. Cài đặt các thư viện Python cần thiết (lần đầu)
pip install websockets aiohttp aiosqlite aiohttp_cors

# 3. Chạy máy chủ Python Backend
python server.py
```
*Khi thành công, terminal sẽ báo:*
- HTTP API running on `http://0.0.0.0:8080`
- WebSocket running on `ws://0.0.0.0:8765`

#### Bước 2.2: Khởi chạy App React Native (Expo)
Mở cửa sổ Terminal thứ hai (giữ Terminal Python tiếp tục chạy):
```bash
# 1. Trỏ vào thư mục App_React
cd App_React

# 2. Cài đặt các gói thư viện Node.js (lần đầu)
npm install

# 3. Khởi chạy ứng dụng Expo
npx expo start
```

#### Bước 2.3: Trải nghiệm App
Mã QR sẽ hiển thị trên Terminal Expo. Bạn có các lựa chọn:
* **Trên điện thoại thực (Khuyên dùng):** Dùng ứng dụng **Expo Go** quét mã QR (Điện thoại và Máy tính cần kết nối chung một mạng Wi-Fi LAN).
* **Trên trình duyệt Web:** Bấm phím `w` trên Terminal.
* **Trên máy ảo Android:** Bấm phím `a` trên Terminal (yêu cầu Android Studio Emulator).

---

### 3️⃣ Nạp Firmware cho các Node vi điều khiển (ESP-IDF)

> 💡 **Lưu ý:** Các lệnh `idf.py` dưới đây được chuẩn bị sẵn để bạn copy và dán vào môi trường **ESP-IDF Command Prompt** hoặc Terminal đã cấu hình môi trường ESP-IDF (`export.bat` / `export.sh`).

#### A. Nạp Firmware cho Mesh Sensor Node ([MSMS_Project](file:///d:/Engineering%20Project/MSMS_Project))
Mở ESP-IDF Command Prompt:
```bash
cd MSMS_Project

# Chọn chip mục tiêu (VD: esp32c6 hoặc esp32)
idf.py set-target esp32c6

# Mở menu cấu hình (Wi-Fi SSID, Pass, Sensor Config, ...)
idf.py menuconfig

# Biên dịch, nạp flash và theo dõi Serial log
idf.py build flash monitor
```

#### B. Nạp Firmware cho Mesh Gateway Root Node ([MeshGateWay](file:///d:/Engineering%20Project/MeshGateWay))
Mở ESP-IDF Command Prompt:
```bash
cd MeshGateWay

# Chọn chip mục tiêu (VD: esp32)
idf.py set-target esp32

# Mở menu cấu hình Gateway
idf.py menuconfig

# Biên dịch, nạp flash và theo dõi Serial log
idf.py build flash monitor
```

---

## ⚡ Kiểm thử & Mô phỏng (Testing & Simulation Mode)

Để kiểm thử hệ thống mà không cần phần cứng ESP32 thực tế:
1. Chạy **Python Backend Server** (`python App_React/PythonServer/server.py`). Server sẽ tự động sinh dữ liệu giả lập (Mock Telemetry) cho các cảm biến BME280, AHT10 và đẩy qua WebSocket.
2. Mở **Mobile App** hoặc **Web Dashboard** để theo dõi biểu đồ biến động sống động thời gian thực và thử nghiệm tính năng đặt ngưỡng cảnh báo (Threshold Alerts).

---

## ❓ Xử lý lỗi thường gặp (Troubleshooting)

1. **App di động không nhận được dữ liệu cảm biến:**
   - Kiểm tra xem `python server.py` có đang chạy hay không.
   - Đảm bảo điện thoại và máy tính kết nối **cùng một mạng Wi-Fi**.
   - Kiểm tra Tường lửa (Windows Firewall) không chặn các cổng `8080` (HTTP) và `8765` / `9090` (WebSocket).

2. **Lỗi bộ nhớ đệm (Cache) của Expo / React Native:**
   - Xóa cache bằng cách khởi chạy: `npx expo start -c`.

3. **Không tìm thấy lệnh `idf.py`:**
   - Mở đúng công cụ **ESP-IDF 5.x CMD** hoặc chạy lệnh `export.bat` trong thư mục cài đặt ESP-IDF trước khi gõ lệnh.

4. **Trùng Port khi khởi chạy Web Dashboard:**
   - Đảm bảo chưa có tiến trình cũ chiếm dụng cổng 3000 hoặc 9090. Sử dụng `stop.bat` trong `DashboardManagerWifiMesh` để dọn dẹp các tiến trình nền.
