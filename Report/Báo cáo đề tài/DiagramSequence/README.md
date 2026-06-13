# Diagram Sequence – Quá trình hoạt động hệ thống MSMS

Thư mục chứa sơ đồ chuỗi (PlantUML) mô tả luồng hoạt động của firmware.

## Các file

| File | Mô tả |
|------|--------|
| **system_sequence.puml** | Sơ đồ chi tiết: khởi động, tạo task, vòng lặp menu, xử lý nút bấm và các callback (WiFi, cảm biến, pin, reset). |
| **overview_sequence.puml** | Sơ đồ tổng quan ngắn gọn: người dùng – menu – nút bấm – callback – phần cứng. |

## Cách xuất hình (PNG/SVG)

### 1. Dùng Java + PlantUML JAR (command line)

- Cần cài Java. Tải `plantuml.jar` từ [plantuml.com](https://plantuml.com/download).
- Trong thư mục `Report/DiagramSequence`:

```batch
java -jar plantuml.jar -tpng system_sequence.puml
java -jar plantuml.jar -tsvg system_sequence.puml
java -jar plantuml.jar -tpng overview_sequence.puml
```

### 2. Dùng VS Code / Cursor

- Cài extension **PlantUML** (jebbs.plantuml).
- Mở file `.puml`, nhấn `Alt+D` (Preview) hoặc chuột phải → Export.

### 3. Chèn vào LaTeX báo cáo

Sau khi xuất PNG/SVG, đặt ảnh vào `Report/Images/` và trong `main.tex`:

```latex
\begin{figure}[H]
    \centering
    \includegraphics[width=\textwidth]{Images/system_sequence.png}
    \caption{Quá trình hoạt động hệ thống (sequence diagram).}
\end{figure}
```

## Tóm tắt luồng trong diagram

1. **Khởi động**: `app_main` khởi tạo NVS, GPIO, ButtonManager, ScreenManager, MenuSystem, BatteryManager; tạo task `wifi_init_sta` và `MenuNavigation_Task`.
2. **WiFi**: Task WiFi khởi tạo STA, đợi kết nối hoặc thoát; khi người dùng chọn “WiFi Config”, `wifi_config_task` chạy (AP + captive portal), cập nhật trạng thái và gọi `MenuRender` khi đã kết nối.
3. **Menu**: `MenuNavigation_Task` lặp đọc `ReadButtonStatus()`, xử lý UP/DOWN/SEL/BACK, gọi `MenuRender` hoặc callback (FunctionManager / MenuSystem).
4. **Callback**: Tùy mục chọn – cấu hình WiFi, chọn cảm biến cho port, xem dữ liệu cảm biến (tạo `readDataSensorTask`), trạng thái pin, reset port – và cập nhật hiển thị qua ScreenManager.
