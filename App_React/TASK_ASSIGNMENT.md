# BẢNG PHÂN CÔNG CÔNG VIỆC CHI TIẾT
**Dự án:** Ứng dụng Giám sát Mạng Mesh (React Native / Expo)
**Thành phần:** 2 Người phát triển
**Nguyên tắc:** Người 1 xử lý Data, các thành phần cốt lõi và thuật toán lịch sử. Người 2 làm chủ toàn bộ mảng hiển thị, danh sách và biểu đồ Real-time.

---

## 👨‍💻 NGƯỜI THỨ NHẤT: Core Logic, Data, History & Settings

### 1. Viết cấu hình kết nối WebSocket (Thời gian thực)
**Mục tiêu:** Giữ kết nối ổn định liên tục giữa App và Server Node.js.
- **File phụ trách:** `src/components/WebSocketProvider.tsx` và `src/services/api.ts`.
- **Nhiệm vụ chi tiết:**
  - Khởi tạo kết nối WSS tới địa chỉ `wss://systemmsems.msems.click/ws`.
  - Bắt các sự kiện `onopen`, `onmessage`, `onerror`, `onclose`.
  - Xử lý cơ chế Tự động kết nối lại (Auto-reconnect) sử dụng `setTimeout` với thời gian lùi dần (Exponential backoff).
  - Phân loại luồng tin nhắn (`welcome`, `server_metrics`, `uart_rx`/`gateway_status`) để bắn vào các hàm xử lý tương ứng.

### 2. Phân tách Dữ liệu (Parsing JSON) & Store
**Mục tiêu:** Biến chuỗi JSON phức tạp từ phần cứng thành Object chuẩn mực để React vẽ được và lưu trữ làm Single Source of Truth.
- **File phụ trách:** `src/utils/meshParser.ts` và `src/store/useMeshStore.ts`.
- **Nhiệm vụ chi tiết:**
  - Viết logic bóc tách `sensorType` thành các trường dữ liệu thực tế (Ví dụ: `0` -> Temp, Hum, Press). Bắt lỗi Hex code.
  - Dùng Zustand viết các hàm: `ingestGateway`, `ingestServerMetrics`, `ingestPacket`.
  - Quản lý mảng Real-time (giới hạn 30 phần tử) cho RAM/CPU.
  - Quản lý bộ đếm Timeout: Tự động đổi trạng thái Node thành `Offline` nếu quá 7 giây không nhận được gói tin.

### 3. Thiết kế & Logic cho 2 Tab Menu
- **Menu 1: History (`app/(tabs)/history.tsx`)**
  - Đảm nhận màn hình siêu phức tạp này vì nó dính sát tới Cấu trúc dữ liệu API.
  - Xây dựng cụm Bộ Lọc Khó (Cascading Selectors): Chọn MAC -> Tự tìm mảng Sensors -> Chọn Sensor -> Tự tìm mảng Field.
  - Viết logic gọi REST API (`getHistorySeries`) kéo dữ liệu quá khứ về máy tính.
  - Render Biểu đồ Lịch sử cỡ lớn: Viết thuật toán (Downsampling) nếu mảng quá lớn thì gộp điểm lại để App khỏi giật. Tính toán vẽ 5 Thẻ tóm tắt (Avg, Max, Min).
- **Menu 2: Settings (`app/(tabs)/settings.tsx` & `edit-threshold.tsx`)**
  - Tạo Form nhập các thông số bảo mật / Giới hạn báo động.
  - Viết logic kiểm tra đầu vào (Validation), dùng `expo-secure-store` để lưu tạm cục bộ, sau đó dùng Axios gửi `POST` lên REST API để lưu vĩnh viễn trên Server.

---

## 👨‍🎨 NGƯỜI THỨ HAI: Giao diện Cảm biến, Real-time & 5 Menu Hệ thống

### 1. Menu 3: Dashboard (`app/(tabs)/index.tsx`)
**Mục tiêu:** Xây dựng màn hình Tổng quan đẹp mắt.
- **Nhiệm vụ chi tiết:**
  - Đọc toàn bộ biến trong Store để hiển thị con số tổng quan: Tổng số Node, Số Node Online, Số cảm biến đang active, Uptime của Gateway.
  - Xử lý Nút Thông Báo (Cái Chuông): Lắng nghe từ Store, hiển thị danh sách cảnh báo hoặc lỗi mạng mới nhất.

### 2. Menu 4: Nodes (`app/(tabs)/nodes.tsx`)
**Mục tiêu:** Danh sách trực quan quản lý phần cứng vật lý.
- **Nhiệm vụ chi tiết:**
  - Import mảng `nodes` từ Store để render bằng `ScrollView`.
  - Viết Thanh Tìm Kiếm (Search Bar) lọc theo tên hoặc địa chỉ MAC.
  - Nhận diện `wifi_rssi` (Cường độ sóng) để đổi màu icon (Xanh lá > -60dBm, Vàng > -80dBm, Đỏ < -80dBm). Hiển thị loại Chip vào góc thẻ.

### 3. Menu 5: Sensors (`app/(tabs)/sensors.tsx`)
**Mục tiêu:** Nơi hiển thị tức thời toàn bộ các cảm biến đang hoạt động.
- **Nhiệm vụ chi tiết:**
  - Bóc tách toàn bộ mảng `sensorEntries` từ tất cả các Node.
  - Thiết kế các Card Cảm biến: Hiển thị chữ to, thay đổi màu sắc theo mức độ cảnh báo (Ví dụ PM2.5 > 100 nháy màu đỏ).
  - Viết thuật toán vẽ Sparkline (biểu đồ đường mờ nhỏ xíu) ngay trong Card.

### 4. Menu 6: Gateway (`app/(tabs)/gateway.tsx`)
**Mục tiêu:** Màn hình soi "sức khoẻ" của con Chip tổng (ESP32 Gateway).
- **Nhiệm vụ chi tiết:**
  - Đọc state `gateway` từ Store. Hiển thị Pin (Battery Voltage), Trạng thái nguồn, Uptime.
  - Tích hợp biểu đồ LineChart chạy theo thời gian thực để vẽ 2 đường song song: CPU Load (%) và RAM Used (KB).

### 5. Menu 7: Server (`app/(tabs)/server.tsx`)
**Mục tiêu:** Màn hình soi "sức khoẻ" của Máy chủ Cloud Node.js.
- **Nhiệm vụ chi tiết:**
  - Đọc state `serverMetrics` từ Store.
  - Vẽ giao diện chia khối hiển thị Nhiệt độ hệ thống (Chip Temp), RAM Total/Used.
  - Vẽ biểu đồ thời gian thực giống hệt như Gateway, kết hợp hiệu ứng Animation cho mượt mắt.

---
**✅ Ghi chú hợp tác:** Bảng phân công này cực kỳ tối ưu. Người 1 làm mảng Logic Data sẽ gánh luôn Tab History (Vì History có logic xử lý mảng cực kỳ đau đầu). Chuyển Tab Dashboard dễ thở hơn qua cho Người 2 để bù lại khối lượng 5 Menu. Mọi người cứ bám sát gạch đầu dòng này mà code!
