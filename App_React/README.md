# 🚀 Hệ thống Giám sát Cảm biến Mạng Mesh (Mesh Sensor Monitoring System)

Dự án này là một hệ thống giám sát cảm biến toàn diện, bao gồm một ứng dụng di động/web (được xây dựng bằng React Native & Expo) kết hợp với một máy chủ Backend (được xây dựng bằng Python) mô phỏng Gateway nhận dữ liệu thời gian thực từ các Node (ESP32, ESP8266...).

---

## 📁 Cấu trúc dự án
Dự án được chia làm 2 phần chính nằm chung trong thư mục `App_React`:
1. **Ứng dụng React Native (Frontend)**: Nằm ở ngoài cùng, sử dụng bộ khung Expo Router, Zustand (Quản lý trạng thái cục bộ) và Reanimated (Hiệu ứng).
2. **Python Server (Backend/Mock Gateway)**: Nằm trong thư mục `PythonServer/`, chứa kịch bản Python (`server.py`) đóng vai trò là một Node Gateway thu thập dữ liệu và SQLite (`history.db`) lưu trữ lịch sử cấu hình.

---

## 🛠️ Yêu cầu hệ thống (Prerequisites)
Để chạy được hệ thống này, máy tính của bạn cần cài đặt sẵn:
- **Node.js** (Phiên bản LTS - dùng cho Frontend)
- **Python 3.x** (Dùng cho Backend)
- Cài đặt App **Expo Go** trên điện thoại (Android/iOS) nếu bạn muốn chạy thật trên thiết bị di động.

---

## 🏃 Hướng dẫn khởi chạy hệ thống

Để hệ thống hoạt động hoàn chỉnh, bạn cần bật **cả 2 phần** (Server và App) chạy song song ở 2 cửa sổ Terminal (Command Prompt / PowerShell) khác nhau.

### Bước 1: Khởi động máy chủ Python (Backend/Gateway)
Máy chủ Python sẽ giả lập Gateway, mở cổng WebSocket (8765) để đẩy dữ liệu cảm biến thời gian thực và cổng HTTP (8080) để cung cấp lịch sử dữ liệu và file cấu hình Firmware.

1. Mở Terminal mới và trỏ vào thư mục PythonServer:
   ```bash
   cd "d:\Engineering Project\App_React\PythonServer"
   ```
2. Cài đặt các thư viện Python cần thiết (chỉ cần làm lần đầu):
   ```bash
   pip install websockets aiohttp aiosqlite
   ```
3. Chạy Server:
   ```bash
   python server.py
   ```
   *(Nếu thành công, terminal sẽ báo: "Server started. HTTP on 8080, WS on 8765...")*

---

### Bước 2: Khởi động ứng dụng React Native (Frontend)
Ứng dụng sẽ kết nối với máy chủ Python ở Bước 1 để vẽ biểu đồ và hiển thị thông số.

1. Mở một cửa sổ Terminal **mới** (giữ nguyên cửa sổ Python đang chạy) và trỏ vào thư mục App:
   ```bash
   cd "d:\Engineering Project\App_React"
   ```
2. Cài đặt các gói thư viện Node.js (chỉ cần làm lần đầu):
   ```bash
   npm install
   ```
3. Chạy ứng dụng Expo:
   ```bash
   npx expo start
   ```

### Bước 3: Xem và trải nghiệm ứng dụng
Khi chạy lệnh `npx expo start`, một mã QR sẽ xuất hiện trên Terminal. Bạn có các lựa chọn sau:

- **Chạy trên điện thoại thực tế (Khuyên dùng):** 
  Mở app **Expo Go** trên điện thoại, quét mã QR này. (Lưu ý: Điện thoại và Máy tính phải bắt *cùng một mạng Wi-Fi*). App trên điện thoại sẽ tự động tìm và kết nối với Backend Python thông qua IP LAN của máy tính.
- **Chạy trên máy ảo Android (Emulator):** Nhấn phím `a` trên Terminal.
- **Chạy trên trình duyệt Web:** Nhấn phím `w` trên Terminal.

---

## 💡 Các tính năng nổi bật
- **Bảng điều khiển (Dashboard):** Hiển thị tổng quan các mạng lưới cảm biến, lưu lượng truy cập và tài nguyên Gateway theo thời gian thực (Cập nhật 5s/lần).
- **Trạm nút (Nodes & Sensors):** Quản lý chi tiết từng cảm biến, vẽ biểu đồ Sparkline dao động sống động.
- **Lịch sử (History):** Tra cứu dữ liệu cảm biến cũ lấy từ cơ sở dữ liệu SQLite, hỗ trợ xuất file `.csv` trực tiếp trên điện thoại.
- **Cảnh báo (Threshold & Alert):** Đặt ngưỡng giới hạn cho từng chỉ số cảm biến. Khi chỉ số vượt ngưỡng, hệ thống sẽ lập tức bật khung cảnh báo rơi (Dropdown Toast) báo động.
- **Cập nhật phần mềm (OTA Firmware):** Kiểm tra và lấy danh sách các bản cập nhật Firmware mới nhất từ Backend giả lập.

---

## 🔧 Xử lý lỗi thường gặp
- **App không nhận được dữ liệu cảm biến:** Đảm bảo cửa sổ Python `server.py` đang chạy. Nếu bạn quét bằng điện thoại, hãy chắc chắn tường lửa (Firewall) trên máy tính không chặn cổng `8765` và `8080`.
- **Lỗi thiếu thư viện Python:** Đọc kỹ cảnh báo trên terminal, dùng lệnh `pip install <tên-thư-viện>` để tải bổ sung.
- **Xóa bộ nhớ đệm App:** Nếu ứng dụng Expo bị lỗi màn hình trắng, hãy tắt Terminal của Expo và chạy lại bằng lệnh `npx expo start -c`.
