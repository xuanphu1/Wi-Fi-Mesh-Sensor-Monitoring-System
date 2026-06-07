# DashboardManagerWifiMesh

Dashboard giám sát mạng WiFi Mesh theo thời gian thực, gồm:

- Frontend React (`src`) hiển thị Dashboard / Nodes / Sensors / History / OTA.
- WebSocket server Node.js (`websocket-server`) nhận dữ liệu từ gateway/ESP, parse, lưu MongoDB và phát lại cho UI.

## Đọc ôn thi: 4 file cần đọc đầu tiên

Mục tiêu phần này: giúp bạn nắm luồng chạy React từ lúc mở trang đến lúc render đúng màn hình.

Thứ tự đọc để hiểu nhanh:

1. `public/index.html`
2. `src/index.js`
3. `src/App.js`
4. `src/routes.js`

---

### 1) `public/index.html` - khung HTML gốc

Đây là file HTML duy nhất được trình duyệt tải về lúc đầu.

- Chứa metadata căn bản:
  - `charset`, `viewport`, `manifest`, `favicon`, `title`.
- Chứa các external stylesheet:
  - Google Fonts (`Roboto`)
  - `leaflet.css` (để marker/tile/control của map hiển thị đúng)
  - Material Icons.
- Quan trọng nhất là:
  - `<div id="root"></div>`: điểm mount để React gắn toàn bộ app vào.
  - `<noscript>...</noscript>`: thông báo fallback nếu tắt JavaScript.

Bản chất: file này không chứa logic giao diện của app, nó chỉ là "vỏ HTML" để React đổ vào.

---

### 2) `src/index.js` - điểm khởi động React

File này là "entry point" của frontend:

- Tìm node gốc trong HTML:
  - `document.getElementById("root")`
- Tạo React root:
  - `createRoot(rootElement)`
- Render cây app vào root với thứ tự provider:
  1. `BrowserRouter` - cấp context URL/route cho toàn app.
  2. `VisionUIControllerProvider` - cấp state UI toàn cục (layout, sidenav, theme behavior).
  3. `MeshRealtimeProvider` - cấp state realtime toàn cục (ws/data mesh).
  4. `<App />` - component khung của ứng dụng.

Ý nghĩa: mọi page/component bên dưới `App` đều dùng được route + UI state + realtime state.

---

### 3) `src/App.js` - khung giao diện + runtime routing

Đây là nơi "điều phối" app:

- Import các thành phần chính:
  - React hooks (`useState`, `useEffect`, `useMemo`)
  - Router APIs (`Route`, `Switch`, `Redirect`, `useLocation`)
  - MUI (`ThemeProvider`, `CssBaseline`)
  - `Sidenav`, `theme`, `themeRTL`, Emotion RTL cache.
- Lấy state UI từ context:
  - `miniSidenav`, `direction`, `layout`, `sidenavColor`.
- Xử lý tương tác sidebar:
  - hover vào mini sidenav thì mở rộng, rồi thu gọn lại khi rời chuột.
- Xử lý side-effect:
  - đổi `direction` -> set `document.body.dir`.
  - đổi `pathname` -> cuộn trang về đầu.
- Dùng hàm `getRoutes(routes)` để map route config thành `<Route .../>`.
- Tạo fallback:
  - route không khớp -> `Redirect` về `/dashboard`.
- Render theo hướng giao diện:
  - nếu RTL: dùng `CacheProvider` + `themeRTL`.
  - nếu LTR: dùng `theme` mặc định.
- Chỉ hiện `Sidenav` khi `layout === "dashboard"`.

Ý nghĩa: `App.js` vừa lo layout/theme, vừa lo điều hướng runtime, vừa quyết định trang nào đang render.

---

### 4) `src/routes.js` - bảng cấu hình route + menu

File này khai báo danh sách page của hệ thống trong 1 mảng `routes`.

Mỗi item có thể chứa:

- `route`: URL path.
- `component`: component page sẽ render.
- `name`, `icon`, `type`, `key`: thông tin để vẽ menu `Sidenav`.
- `type: "title"`: tiêu đề nhóm menu (ví dụ OTA, System).
- `type: "collapse"`: item menu có route.

Các route hiện tại (tóm tắt):

- `/dashboard`
- `/nodes`
- `/sensors`
- `/history`
- `/alerts`
- `/ota/overview`
- `/ota/firmware`
- `/ota/update-nodes`
- `/ota/update-root`
- `/settings/system`
- `/about`
- hidden route: `/nodes/:nodeId` (không hiện menu, nhưng vẫn truy cập trực tiếp bằng URL)

Ý nghĩa: sửa `routes.js` là tác động đồng thời đến:

1. routing trong `App.js`
2. item hiển thị trong `Sidenav`.

---

### Luồng chạy tổng kết (để nhớ khi ôn thi)

1. Trình duyệt tải `public/index.html`.
2. React từ `src/index.js` mount vào `#root`.
3. `App.js` dùng `routes.js` để render page theo URL.
4. User bấm menu/đổi URL -> Router đổi component -> UI cập nhật.
5. Dữ liệu realtime được cấp qua context providers đã bọc từ `index.js`.

## Chạy dự án

### Yêu cầu

- Node.js 18+ (khuyến nghị)
- MongoDB local hoặc remote

### Cài dependencies

Tại thư mục gốc:

```bash
npm install
```

Tại `websocket-server`:

```bash
npm install --prefix websocket-server
```

### Chạy development

Chạy cả React + WebSocket server:

```bash
npm run dev
```

Hoặc chạy riêng:

```bash
npm run start:react
npm run ws-server
```

### Build frontend

```bash
npm run build
```

## Biến môi trường quan trọng

### Frontend (`.env` ở root)

- `REACT_APP_WS_URL`: URL websocket backend. Ví dụ: `ws://192.168.1.10:8080`
  - Nếu không set và đang `development`, app tự dùng `ws://<window.location.hostname>:8080`.

### WebSocket server

- `WS_PORT`: cổng websocket (mặc định `8080`)
- `WS_HOST`: host bind (mặc định `0.0.0.0`)
- `MONGO_URI`: URI MongoDB (mặc định `mongodb://127.0.0.1:27017/MeshData`)

## Cấu trúc thư mục chính

```text
DashboardManagerWifiMesh/
├─ public/                         # static assets cho React
├─ src/
│  ├─ api/                         # helper gọi API/poll (một phần cũ)
│  ├─ assets/                      # theme, images, style của Vision UI
│  ├─ components/                  # Vui* base components
│  ├─ context/                     # Vision UI controller state
│  ├─ examples/                    # layout/table/chart/nav dùng chung
│  ├─ hooks/                       # realtime hooks qua websocket
│  ├─ layouts/                     # các màn hình theo route
│  ├─ utils/                       # parser schema, ws config, error code map
│  ├─ App.js                       # router render + side nav
│  ├─ index.js                     # entrypoint React
│  └─ routes.js                    # khai báo route/mục menu
├─ websocket-server/
│  ├─ server.js                    # WS server + protocol handlers
│  ├─ mongodb-store.js             # schema + lưu/đọc MongoDB
│  └─ package.json                 # deps riêng cho backend WS
├─ build/                          # output build frontend
├─ package.json
└─ README.md
```

## Cách thức hoạt động

### 1) Frontend khởi tạo và định tuyến

- `src/index.js` mount ứng dụng với `BrowserRouter`.
- `src/App.js` lấy route từ `src/routes.js`, tạo `Route` động, hiển thị `Sidenav`.
- Các trang chính đang dùng:
  - `/dashboard`
  - `/nodes`
  - `/nodes/:nodeId`
  - `/sensors`
  - `/history`
  - `/alerts`
  - `/ota/overview`
  - `/ota/firmware`
  - `/ota/update-nodes`
  - `/ota/update-root`
  - `/settings/system`
  - `/about`

### 2) Kết nối realtime qua WebSocket

- `src/utils/wsConfig.js` tạo URL websocket.
- `src/hooks/useDashboardRealtime.js`:
  - mở websocket, tự reconnect khi mất kết nối,
  - parse payload mesh từ JSON trực tiếp hoặc `uart_rx` (cả `payload` string và `hex`),
  - tính throughput, độ trễ dữ liệu, trạng thái online/offline node,
  - cung cấp state cho Dashboard/Sensors.
- `src/hooks/useMeshNodesFromWebSocket.js`:
  - tích lũy registry node từ stream realtime,
  - dựng danh sách node cho trang `Nodes` và `Node Detail`.

### 3) Chuẩn hoá dữ liệu mesh

- `src/utils/meshUdpJsonSchema.js` parse schema compact từ firmware:
  - header: `v`, `n`, `i`, `t`, `ver`, `err`
  - data: `p` gồm nhiều row `[port, sensorType, v0, ...]`
- Mapping sensor type và field metadata (label/unit) được định nghĩa tập trung tại đây.

### 4) WebSocket backend + MongoDB

- `websocket-server/server.js`:
  - nhận message từ gateway/device/client,
  - phản hồi `welcome`, `history_meta_response`, `history_series_response`,
  - parse frame mesh, broadcast lại cho các client dashboard.
- `websocket-server/mongodb-store.js`:
  - lưu dữ liệu sensor vào collection `Data`,
  - truy vấn snapshot node, danh sách IP, và timeseries cho trang History.

### 5) Luồng dữ liệu tổng quát

1. Gateway/ESP gửi frame lên `websocket-server`.
2. Server parse + lưu MongoDB + broadcast.
3. Frontend nhận stream, parse và cập nhật UI realtime.
4. Trang `History` gửi request qua WS để lấy series lịch sử từ MongoDB.


