# Wi-Fi Mesh Sensor Monitoring System (Engineering Project)

## 📋 Tổng quan (Overview)
Đây là một dự án tổng thể về hệ thống theo dõi và giám sát dữ liệu cảm biến thông qua mạng **Wi-Fi Mesh**. Hệ thống kết hợp giữa phần cứng vi điều khiển (ESP32 series), các thiết bị đầu cuối đo đạc cảm biến, Gateway thu thập dữ liệu và các nền tảng hiển thị (Web Dashboard, Mobile App, Python Tools).

Dự án áp dụng mô hình mạng không sử dụng Router trung tâm (No Router Mesh) bằng công nghệ ESP-Mesh-Lite, giúp mở rộng vùng phủ sóng và tăng độ ổn định của mạng IoT.

---

## 🏗️ Kiến trúc hệ thống (System Architecture)

Luồng dữ liệu (Data Flow) trong hệ thống:
```text
[Cảm biến] -> [ESP32 Mesh Node] (MRS_Project - C3 / Node_MSMS) 
                    |
              (Wi-Fi Mesh)
                    |
                    v
             [Mesh Gateway] (Root Node)
                    |
             (WebSocket / IP)
                    |
    +---------------+---------------+
    |               |               |
    v               v               v
[Web Dashboard] [Mobile App]  [Python Scripts]
```

1. **Mesh Nodes (Các node con):** Đọc dữ liệu từ cảm biến qua I2C (BME280, MQ,...), hiển thị tại chỗ lên màn hình OLED và đóng gói dữ liệu gửi qua mạng Mesh.
2. **Mesh Gateway (Node trung tâm):** Thu thập toàn bộ dữ liệu từ các Node con và định tuyến gửi lên Server / Dashboard.
3. **Clients / Dashboards:** Hiển thị, vẽ biểu đồ dữ liệu thời gian thực và quản trị mạng lưới.

---

## 📂 Cấu trúc thư mục (Directory Structure)

Dưới đây là các thành phần chính của dự án:

### 1. Firmware (ESP-IDF)
*   **`MRS_Project - C3/`**: Mã nguồn dành cho các Node cảm biến sử dụng ESP32-C6. Bao gồm kiến trúc phân lớp rõ ràng: xử lý cảm biến (SensorRegistry), giao diện người dùng trên OLED, quản lý nút bấm, đèn LED và tính toán tỷ lệ mất gói (Packet Loss) theo thời gian thực.
*   **`MeshGateWay/`**: Mã nguồn đóng vai trò làm Root Node (Gateway) trong mạng Wi-Fi Mesh. Thu thập dữ liệu từ các Node bên dưới và đẩy ra mạng bên ngoài.
*   **`Node_MSMS/`**: Mã nguồn triển khai ESP-Mesh-Lite (dựa trên No Router Example). Giúp các ESP32 tự động kết nối và chọn parent node để hình thành mạng Mesh.

### 2. Giao diện (Frontend & Mobile)
*   **`DashboardManagerWifiMesh/`**: Dự án Web Dashboard được xây dựng bằng React (Web). Chứa cả một WebSocket Server (`websocket-server/`) đi kèm để nhận dữ liệu từ Gateway và hiển thị lên trình duyệt.
*   **`App_React/`**: Dự án Mobile App xây dựng bằng React Native (Expo Router, Expo 54). Dùng để giám sát hệ thống mạng Mesh và thông số cảm biến trên điện thoại (Android/iOS).

### 3. Công cụ kiểm thử (Testing Tools)
*   **`PythonStressTest/`**: Các kịch bản Python (`client_simulator.py`, `server_plot.py`) để giả lập lưu lượng mạng từ các Node, từ đó đánh giá mức độ ổn định và vẽ biểu đồ đo đạc tỷ lệ Packet Loss (%) theo thời gian thực thông qua `matplotlib`.

### 4. Tài liệu (Documentation)
*   **`Report/`**: Thư mục chứa các báo cáo, tài liệu thiết kế và phân tích của dự án.

---

## 🚀 Hướng dẫn khởi chạy cơ bản (Getting Started)

1. **Nạp Firmware cho các Node:**
   - Cài đặt ESP-IDF (v5.x).
   - Truy cập vào `MRS_Project - C3` hoặc `MeshGateWay` và chạy: `idf.py menuconfig` để cấu hình, sau đó `idf.py build flash monitor`.

2. **Khởi chạy Web Dashboard:**
   - Đi vào thư mục `DashboardManagerWifiMesh`.
   - Cài đặt thư viện: `npm install`
   - Chạy ứng dụng: `npm start` (hoặc khởi động riêng websocket server theo tài liệu bên trong).

3. **Khởi chạy Mobile App:**
   - Đi vào thư mục `App_React`.
   - Chạy lệnh: `npm install` và `npx expo start`.
   - Quét mã QR bằng ứng dụng Expo Go trên điện thoại.

4. **Chạy Python Stress Test:**
   - Đi vào `PythonStressTest`.
   - Cài đặt requirements: `pip install -r requirements.txt`.
   - Chạy server vẽ biểu đồ: `python server_plot.py`.
   - Chạy giả lập Node: `python client_simulator.py`.
