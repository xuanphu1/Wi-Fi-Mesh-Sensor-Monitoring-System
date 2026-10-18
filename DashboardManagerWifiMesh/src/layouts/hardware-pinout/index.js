import React, { useState, useMemo } from "react";
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import Tooltip from "@mui/material/Tooltip";
import TextField from "@mui/material/TextField";
import Chip from "@mui/material/Chip";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

// Clean, Flat Card Style (No colorful glowing, no heavy gradients)
const cardSx = {
  background: "rgba(15, 21, 53, 0.75)",
  border: "1px solid rgba(255, 255, 255, 0.1)",
  borderRadius: "14px",
  boxShadow: "none",
  p: 2.5,
};

// Simplified flat color palette for pin categories
const PIN_TYPES = {
  POWER: {
    label: "Nguồn / Power",
    color: "#f87171",
    bg: "rgba(239, 68, 68, 0.14)",
    border: "rgba(239, 68, 68, 0.4)",
  },
  GND: {
    label: "Ground (0V)",
    color: "#94a3b8",
    bg: "rgba(148, 163, 184, 0.14)",
    border: "rgba(148, 163, 184, 0.35)",
  },
  BUS: {
    label: "Bus / Truyền thông",
    color: "#38bdf8",
    bg: "rgba(56, 189, 248, 0.14)",
    border: "rgba(56, 189, 248, 0.4)",
  },
  ACTUATOR: {
    label: "Cơ cấu chấp hành / Actuator",
    color: "#c084fc",
    bg: "rgba(192, 132, 252, 0.14)",
    border: "rgba(192, 132, 252, 0.4)",
  },
  PULSE: {
    label: "Xung & Ngắt / Pulse GPIO",
    color: "#34d399",
    bg: "rgba(52, 211, 153, 0.14)",
    border: "rgba(52, 211, 153, 0.4)",
  },
};

// Data for the 3 IDC connectors (10 pins each, 2x5 layout)
const PORTS_DATA = [
  {
    id: "port1",
    name: "Port 1 (IDC2)",
    title: "Serial UART & I2C & Actuator Hub",
    badge: "UART2 / I2C / Relay 1",
    desc: "Cổng chuyên dụng cho cảm biến truyền thông nối tiếp UART tốc độ cao, cảm biến môi trường I2C và điều khiển cơ cấu chấp hành 1.",
    recommended: "Cảm biến bụi PM2.5, Cảm biến CO2 MH-Z14A, Modbus RS485, SHT30, BME280, Van/Relay 1",
    pins: [
      {
        pin: 1,
        isSquare: true,
        name: "ActivePower",
        type: "POWER",
        voltage: "3.3V / 5.0V (Switched)",
        desc: "Đường nguồn điều khiển đóng/ngắt qua transistor/MOSFET. Cho phép ESP32 ngắt nguồn cảm biến khi vào chế độ ngủ (Deep Sleep) để tối ưu hoá năng lượng pin.",
        warning: "Không cấp trực tiếp nguồn ngoài vào chân này. Tải dòng khuyến nghị < 500mA.",
      },
      {
        pin: 2,
        isSquare: false,
        name: "RX2",
        type: "BUS",
        voltage: "3.3V Logic (TTL)",
        desc: "Chân nhận tín hiệu UART2 của vi điều khiển. Dùng nhận dữ liệu từ cảm biến UART (PMS7003, MH-Z19B, GPS, IC RS485).",
        warning: "Mức logic tối đa 3.3V. Nếu nối với thiết bị 5V cần dùng mạch phân áp hoặc IC chuyển mức logic.",
      },
      {
        pin: 3,
        isSquare: false,
        name: "SDA",
        type: "BUS",
        voltage: "3.3V (I2C Data)",
        desc: "Tuyến dữ liệu nối tiếp I2C Data (nối chung bus với các cổng khác). Đã có trở kéo nội bộ / trên bo mạch.",
        warning: "Có thể mắc song song nhiều cảm biến I2C có địa chỉ khác nhau.",
      },
      {
        pin: 4,
        isSquare: false,
        name: "TX2",
        type: "BUS",
        voltage: "3.3V Logic (TTL)",
        desc: "Chân phát tín hiệu UART2 của vi điều khiển. Dùng gửi lệnh điều khiển xuống cảm biến hoặc giao tiếp Modbus.",
        warning: "Không đấu chập trực tiếp vào nguồn hoặc mass.",
      },
      {
        pin: 5,
        isSquare: false,
        name: "SCL",
        type: "BUS",
        voltage: "3.3V (I2C Clock)",
        desc: "Tuyến xung nhịp nối tiếp I2C Clock của vi điều khiển. Đồng bộ dữ liệu I2C tốc độ chuẩn 100kHz hoặc 400kHz.",
        warning: "Đảm bảo đường dây ngắn (< 50cm) để tránh suy hao xung nhịp.",
      },
      {
        pin: 6,
        isSquare: false,
        name: "Actuator1",
        type: "ACTUATOR",
        voltage: "3.3V Logic / PWM",
        desc: "Đường tín hiệu điều khiển cơ cấu chấp hành 1 (Relay 1, MOSFET bật tắt đèn/quạt, hoặc xung PWM điều khiển tốc độ).",
        warning: "Chân xuất dòng tín hiệu điều khiển, không nối trực tiếp vào cuộn dây Relay không có mạch bảo vệ diode/transistor.",
      },
      {
        pin: 7,
        isSquare: false,
        name: "GND",
        type: "GND",
        voltage: "0V",
        desc: "Mass đất chung hệ thống (Common Ground). Tất cả cảm biến và ngoại vi bắt buộc phải chung GND.",
        warning: "Đảm bảo tiếp xúc chắc chắn để tín hiệu đo đạt độ chính xác cao.",
      },
      {
        pin: 8,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Nguồn dương 5V cấp cho ngoại vi và cảm biến tải lớn (như quạt hút cảm biến bụi, module RS485, còi buzzer).",
        warning: "Dòng cực đại phụ thuộc nguồn cấp đầu vào mạch.",
      },
      {
        pin: 9,
        isSquare: false,
        name: "+3V3_Peri",
        type: "POWER",
        voltage: "+3.3V DC (Regulated)",
        desc: "Nguồn dương ổn áp 3.3V cấp cho các chip cảm biến kỹ thuật số bán dẫn (SHT3x, BME280, AHT10).",
        warning: "Tuyệt đối không chập chân 3V3 sang chân 5V vì sẽ gây cháy chip và hỏng cảm biến.",
      },
      {
        pin: 10,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Đường nguồn 5V phụ trợ nối song song với chân 8 nhằm tăng tiết diện dẫn dòng và chống sụt áp khi tải lớn.",
        warning: "Chung đường ray với chân 8.",
      },
    ],
  },
  {
    id: "port2",
    name: "Port 2",
    title: "Pulse Counter & GPIO & Actuator Hub",
    badge: "Pulse / Flow / Rain / Relay 2",
    desc: "Cổng chuyên dụng cho các loại cảm biến đếm xung tốc độ cao (lưu lượng nước, lượng mưa gầu lật), ngắt ngoài, I2C và điều khiển Actuator 2.",
    recommended: "Cảm biến lưu lượng YF-S201, Cảm biến đo mưa Tipping Bucket, Cảm biến phao từ, SHT30, BME280, Bơm/Van 2",
    pins: [
      {
        pin: 1,
        isSquare: true,
        name: "ActivePower",
        type: "POWER",
        voltage: "3.3V / 5.0V (Switched)",
        desc: "Nguồn điều khiển ngắt mở tiết kiệm năng lượng. Giúp tắt nguồn cảm biến xung khi không cần đo liên tục.",
        warning: "Tải dòng tối đa 500mA.",
      },
      {
        pin: 2,
        isSquare: false,
        name: "Actuator2",
        type: "ACTUATOR",
        voltage: "3.3V Logic / PWM",
        desc: "Đường tín hiệu điều khiển cơ cấu chấp hành 2 (Van điện từ tưới nước, bơm định lượng, còi cảnh báo).",
        warning: "Cần thông qua mạch đệm transistor / optocoupler cách ly.",
      },
      {
        pin: 3,
        isSquare: false,
        name: "SDA",
        type: "BUS",
        voltage: "3.3V (I2C Data)",
        desc: "Tuyến dữ liệu I2C Data nối chung bus toàn bo mạch.",
        warning: "Hỗ trợ mắc song song nhiều cảm biến I2C.",
      },
      {
        pin: 4,
        isSquare: false,
        name: "Pulse/GPIO_1",
        type: "PULSE",
        voltage: "3.3V Logic (Interrupt)",
        desc: "Kênh đếm xung phần cứng số 1 hoặc chân GPIO đa dụng. Tích hợp mạch lọc chống dội (debounce) và ngắt ngoài của vi điều khiển. Rất lý tưởng cho cảm biến lưu lượng nước (Flow sensor Hall effect).",
        warning: "Nếu cảm biến cấp ngõ ra xung 5V, bắt buộc dùng cầu phân áp hạ về 3.3V trước khi cắm vào chân này.",
      },
      {
        pin: 5,
        isSquare: false,
        name: "SCL",
        type: "BUS",
        voltage: "3.3V (I2C Clock)",
        desc: "Tuyến xung nhịp I2C Clock nối chung bus toàn bo mạch.",
        warning: "Chuẩn xung 100k - 400kHz.",
      },
      {
        pin: 6,
        isSquare: false,
        name: "Pulse/GPIO_2",
        type: "PULSE",
        voltage: "3.3V Logic (Interrupt)",
        desc: "Kênh đếm xung phần cứng số 2 hoặc chân GPIO phụ. Dành cho cảm biến đo lượng mưa (Rain Gauge), đếm vòng quay hoặc phao báo mức nước cao/thấp.",
        warning: "Điện áp tối đa 3.3V.",
      },
      {
        pin: 7,
        isSquare: false,
        name: "GND",
        type: "GND",
        voltage: "0V",
        desc: "Mass đất chung hệ thống (Common Ground).",
        warning: "Cần tiếp mass chắc chắn.",
      },
      {
        pin: 8,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Nguồn 5V công suất cấp cho cảm biến lưu lượng hoặc van từ nhỏ.",
        warning: "Kiểm tra dòng tiêu thụ của phụ tải.",
      },
      {
        pin: 9,
        isSquare: false,
        name: "+3V3_Peri",
        type: "POWER",
        voltage: "+3.3V DC (Regulated)",
        desc: "Nguồn 3.3V cấp cho các IC kỹ thuật số.",
        warning: "Không nối chập vào 5V.",
      },
      {
        pin: 10,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Đường nguồn 5V phụ trợ nối song song.",
        warning: "Chung ray nguồn với chân 8.",
      },
    ],
  },
  {
    id: "port3",
    name: "Port 3 (IDC3)",
    title: "High-Speed SPI & Actuator Hub",
    badge: "SPI / LoRa / SD / Relay 3",
    desc: "Cổng giao tiếp đồng bộ tốc độ cao SPI (dành cho module LoRa SX1278, thẻ nhớ MicroSD lưu log dữ liệu, màn hình SPI TFT) và điều khiển Actuator 3.",
    recommended: "Module LoRa SX1278 tầm xa, Thẻ nhớ MicroSD SPI, Màn hình TFT ST7789, Module mở rộng MCP2515 CAN-Bus, Van/Relay 3",
    pins: [
      {
        pin: 1,
        isSquare: true,
        name: "ActivePower",
        type: "POWER",
        voltage: "3.3V / 5.0V (Switched)",
        desc: "Nguồn điều khiển ngắt mở tiết kiệm năng lượng cho module LoRa / ngoại vi SPI.",
        warning: "Tải dòng tối đa 500mA.",
      },
      {
        pin: 2,
        isSquare: false,
        name: "MOSI",
        type: "BUS",
        voltage: "3.3V (SPI MOSI)",
        desc: "Tuyến Master Out Slave In - Vi điều khiển truyền dữ liệu tới module ngoại vi SPI (như LoRa, thẻ nhớ).",
        warning: "Mức logic 3.3V.",
      },
      {
        pin: 3,
        isSquare: false,
        name: "CS/NSS",
        type: "BUS",
        voltage: "3.3V (Chip Select)",
        desc: "Tuyến chọn chip ngoại vi SPI (Chip Select - tích cực mức thấp). Kích hoạt module khi cần giao tiếp.",
        warning: "Mỗi thiết bị trên đường SPI cần 1 chân CS riêng biệt.",
      },
      {
        pin: 4,
        isSquare: false,
        name: "SCK",
        type: "BUS",
        voltage: "3.3V (SPI Clock)",
        desc: "Tuyến xung nhịp đồng bộ SPI Clock do vi điều khiển phát ra (tốc độ có thể đạt từ 1MHz - 20MHz).",
        warning: "Tuyến xung cao tần, đường mạch cần ngắn và chống nhiễu tốt.",
      },
      {
        pin: 5,
        isSquare: false,
        name: "Actuator3",
        type: "ACTUATOR",
        voltage: "3.3V Logic / PWM",
        desc: "Đường tín hiệu điều khiển cơ cấu chấp hành 3 (Relay 3, còi hú báo sự cố hoặc ngõ ra cảnh báo).",
        warning: "Cần nối qua mạch đệm cách ly quang.",
      },
      {
        pin: 6,
        isSquare: false,
        name: "MISO",
        type: "BUS",
        voltage: "3.3V (SPI MISO)",
        desc: "Tuyến Master In Slave Out - Vi điều khiển nhận dữ liệu từ module SPI gửi về.",
        warning: "Mức logic 3.3V.",
      },
      {
        pin: 7,
        isSquare: false,
        name: "GND",
        type: "GND",
        voltage: "0V",
        desc: "Mass đất chung hệ thống (Common Ground).",
        warning: "Bắt buộc đấu chung GND để SPI không bị sai lệch dữ liệu.",
      },
      {
        pin: 8,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Nguồn 5V cấp cho màn hình hoặc module có ổn áp tích hợp.",
        warning: "Chú ý điện áp logic tương ứng.",
      },
      {
        pin: 9,
        isSquare: false,
        name: "+3V3_Peri",
        type: "POWER",
        voltage: "+3.3V DC (Regulated)",
        desc: "Nguồn 3.3V chuẩn cho module LoRa SX1278 (LoRa rất nhạy cảm và bắt buộc dùng nguồn 3.3V sạch).",
        warning: "Tuyệt đối không cấp nguồn 5V vào module LoRa SX1278 (sẽ hỏng chip RF).",
      },
      {
        pin: 10,
        isSquare: false,
        name: "5V_Peri",
        type: "POWER",
        voltage: "+5.0V DC",
        desc: "Nguồn 5V phụ trợ nối song song.",
        warning: "Chung ray nguồn với chân 8.",
      },
    ],
  },
];

// Sensor Wiring Encyclopedia Database
const SENSOR_WIRING_GUIDES = [
  {
    sensorName: "SHT30 / SHT31 / BME280 / AHT10",
    category: "Cảm biến Nhiệt độ - Độ ẩm - Áp suất",
    recommendedPort: "Port 1 hoặc Port 2",
    wiring: [
      { pinName: "VCC", target: "Pin 9 (+3V3_Peri)", note: "Nguồn sạch 3.3V" },
      { pinName: "GND", target: "Pin 7 (GND)", note: "Mass chung" },
      { pinName: "SDA", target: "Pin 3 (SDA)", note: "Tuyến I2C Data" },
      { pinName: "SCL", target: "Pin 5 (SCL)", note: "Tuyến I2C Clock" },
    ],
    notes: "Có thể mắc song song tối đa nhiều cảm biến I2C (mỗi cảm biến phải khác địa chỉ I2C Hex, ví dụ SHT30 là 0x44, BME280 là 0x76).",
  },
  {
    sensorName: "YF-S201 / Hall Effect Water Flow",
    category: "Cảm biến Đo Lưu Lượng Nước (Đếm Xung)",
    recommendedPort: "Port 2 (Khuyên dùng)",
    wiring: [
      { pinName: "Dây Đỏ (VCC)", target: "Pin 8 (5V_Peri)", note: "Nguồn 5V cho cảm biến Hall" },
      { pinName: "Dây Đen (GND)", target: "Pin 7 (GND)", note: "Mass chung" },
      { pinName: "Dây Vàng (Signal)", target: "Pin 4 (Pulse/GPIO_1)", note: "Kênh đếm xung phần cứng ngắt ngoài" },
    ],
    notes: "Tần số xung f = 7.5 * Q (Q tính theo L/phút). Nếu ngõ ra xung là 5V, nên gắn thêm trở phân áp về 3.3V.",
  },
  {
    sensorName: "Tipping Bucket Rain Gauge",
    category: "Cảm biến Đo Lượng Mưa (Gầu Lật)",
    recommendedPort: "Port 2",
    wiring: [
      { pinName: "Dây 1", target: "Pin 6 (Pulse/GPIO_2)", note: "Đếm xung ngắt gầu lật" },
      { pinName: "Dây 2", target: "Pin 7 (GND)", note: "Mass chung" },
    ],
    notes: "Mỗi nhịp gầu lật tương ứng với 0.2mm lượng mưa. Phần mềm vi điều khiển tự động lọc dội xung (debounce 50ms).",
  },
  {
    sensorName: "PMS7003 / PMS5003 Plantower",
    category: "Cảm biến Bụi Mịn (PM1.0, PM2.5, PM10)",
    recommendedPort: "Port 1 (Serial UART)",
    wiring: [
      { pinName: "VCC (Pin 1, 2)", target: "Pin 8 (5V_Peri)", note: "Nguồn 5V để quạt hút laser hoạt động" },
      { pinName: "GND (Pin 3, 4)", target: "Pin 7 (GND)", note: "Mass chung" },
      { pinName: "TXD (Pin 9)", target: "Pin 2 (RX2)", note: "Cảm biến truyền kết quả UART vào ESP32" },
      { pinName: "RXD (Pin 7)", target: "Pin 4 (TX2)", note: "ESP32 gửi lệnh chuyển chế độ (tùy chọn)" },
    ],
    notes: "Baudrate mặc định 9600 bps. Có thể dùng chân Pin 1 (ActivePower) kích hoạt chế độ ngủ sâu cho quạt bụi.",
  },
  {
    sensorName: "SX1278 / SX1262 LoRa Module",
    category: "Truyền thông Không Dây Tầm Xa (Long Range)",
    recommendedPort: "Port 3 (SPI Port)",
    wiring: [
      { pinName: "VCC", target: "Pin 9 (+3V3_Peri)", note: "Bắt buộc 3.3V (Cực kỳ nhạy cảm)" },
      { pinName: "GND", target: "Pin 7 (GND)", note: "Mass chung" },
      { pinName: "MOSI", target: "Pin 2 (MOSI)", note: "SPI Data Out" },
      { pinName: "MISO", target: "Pin 6 (MISO)", note: "SPI Data In" },
      { pinName: "SCK", target: "Pin 4 (SCK)", note: "SPI Clock" },
      { pinName: "NSS / CS", target: "Pin 3 (CS)", note: "SPI Chip Select" },
    ],
    notes: "Module LoRa cho phép truyền tin xuyên vật cản từ 1-5km giữa các trạm Gateway hoặc Node ngoài tầm Wi-Fi Mesh.",
  },
  {
    sensorName: "Relay 12V/24V / Van Điện Từ / Còi Báo",
    category: "Cơ Cấu Chấp Hành (Actuators)",
    recommendedPort: "Port 1, 2 hoặc 3 (Tương ứng Actuator 1, 2, 3)",
    wiring: [
      { pinName: "Signal IN", target: "Pin 6 (Port 1) hoặc Pin 2 (Port 2) hoặc Pin 5 (Port 3)", note: "Lệnh đóng ngắt 3.3V" },
      { pinName: "VCC Logic", target: "Pin 8 (5V_Peri) hoặc Pin 9 (3V3)", note: "Nguồn nuôi mạch đệm" },
      { pinName: "GND", target: "Pin 7 (GND)", note: "Chung mass điều khiển" },
    ],
    notes: "Bắt buộc sử dụng mạch điều khiển có cách ly quang (Optocoupler) và Diode dập xung ngược để chống treo MCU.",
  },
];

export default function HardwarePinout() {
  const [selectedPortId, setSelectedPortId] = useState("port1");
  const [selectedPinNum, setSelectedPinNum] = useState(1);
  const [searchQuery, setSearchQuery] = useState("");

  const activePort = useMemo(() => {
    return PORTS_DATA.find((p) => p.id === selectedPortId) || PORTS_DATA[0];
  }, [selectedPortId]);

  const activePin = useMemo(() => {
    return activePort.pins.find((p) => p.pin === selectedPinNum) || activePort.pins[0];
  }, [activePort, selectedPinNum]);

  // Check if a pin matches search query
  const isPinMatched = (pinObj) => {
    if (!searchQuery.trim()) return false;
    const q = searchQuery.toLowerCase().trim();
    return (
      pinObj.name.toLowerCase().includes(q) ||
      pinObj.type.toLowerCase().includes(q) ||
      pinObj.voltage.toLowerCase().includes(q) ||
      pinObj.desc.toLowerCase().includes(q)
    );
  };

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={2}>
        {/* Title & Search Header - Clean & Minimalist */}
        <Card sx={{ ...cardSx, mb: 2.5 }}>
          <Grid container spacing={2} alignItems="center">
            <Grid item xs={12} md={7}>
              <VuiTypography variant="h5" color="white" fontWeight="bold" mb={0.5}>
                Hardware Pinout & Sơ Đồ Chân Cắm
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                Tra cứu sơ đồ kết nối chân mạch và cẩm nang đấu nối cảm biến cho MSMS Node / Root PCB
              </VuiTypography>
            </Grid>

            {/* Clean Quick Search Bar without icon */}
            <Grid item xs={12} md={5}>
              <TextField
                fullWidth
                size="small"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                placeholder="Tìm nhanh: I2C, UART, 5V, Pulse, Actuator..."
                sx={{
                  "& .MuiOutlinedInput-root": {
                    borderRadius: "10px",
                    background: "rgba(10, 14, 35, 0.6)",
                    border: "1px solid rgba(255, 255, 255, 0.12)",
                    color: "#fff",
                    "& fieldset": { borderColor: "transparent" },
                  },
                }}
              />
            </Grid>
          </Grid>

          {/* Quick Filter Tags - Clean Flat Style */}
          <VuiBox display="flex" flexWrap="wrap" gap={1} mt={1.8}>
            <VuiTypography variant="caption" color="text" sx={{ display: "flex", alignItems: "center", mr: 0.5 }}>
              Lọc nhanh:
            </VuiTypography>
            {["I2C", "UART", "SPI", "Pulse", "Actuator", "5V", "3.3V", "GND"].map((tag) => (
              <Chip
                key={tag}
                label={tag}
                size="small"
                onClick={() => setSearchQuery(tag)}
                sx={{
                  backgroundColor: searchQuery.toLowerCase() === tag.toLowerCase() ? "rgba(0, 117, 255, 0.35)" : "rgba(255,255,255,0.06)",
                  border: searchQuery.toLowerCase() === tag.toLowerCase() ? "1px solid #00d4ff" : "1px solid rgba(255,255,255,0.1)",
                  color: "#fff",
                  cursor: "pointer",
                  fontSize: "11px",
                  borderRadius: "6px",
                  boxShadow: "none",
                }}
              />
            ))}
            {searchQuery && (
              <Chip
                label="Xóa lọc"
                size="small"
                onDelete={() => setSearchQuery("")}
                sx={{ backgroundColor: "rgba(239, 68, 68, 0.15)", color: "#f87171", border: "1px solid rgba(239,68,68,0.3)" }}
              />
            )}
          </VuiBox>
        </Card>

        {/* Port Selection Tabs - Flat & Clean */}
        <VuiBox display="flex" gap={1.5} mb={2.5} flexWrap="wrap">
          {PORTS_DATA.map((p) => {
            const isSelected = p.id === selectedPortId;
            return (
              <Button
                key={p.id}
                onClick={() => {
                  setSelectedPortId(p.id);
                  setSelectedPinNum(1);
                }}
                sx={{
                  flex: "1 1 200px",
                  py: 1.2,
                  px: 2,
                  borderRadius: "10px",
                  background: isSelected ? "rgba(0, 117, 255, 0.2)" : "rgba(15, 21, 55, 0.5)",
                  border: isSelected ? "1px solid #00d4ff" : "1px solid rgba(255, 255, 255, 0.1)",
                  color: "#fff",
                  display: "flex",
                  flexDirection: "column",
                  alignItems: "flex-start",
                  boxShadow: "none",
                  textTransform: "none",
                  "&:hover": {
                    background: "rgba(0, 117, 255, 0.12)",
                    borderColor: "rgba(0, 212, 255, 0.6)",
                  },
                }}
              >
                <VuiBox display="flex" justifyContent="space-between" width="100%" alignItems="center" mb={0.4}>
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    {p.name}
                  </VuiTypography>
                  <Chip
                    size="small"
                    label={p.badge}
                    sx={{
                      height: 18,
                      fontSize: "0.68rem",
                      fontWeight: "bold",
                      color: isSelected ? "#00d4ff" : "#a0aec0",
                      background: "rgba(255, 255, 255, 0.06)",
                      border: "1px solid rgba(255, 255, 255, 0.1)",
                    }}
                  />
                </VuiBox>
                <VuiTypography variant="caption" color="text" textAlign="left" sx={{ fontSize: "11px" }}>
                  {p.title}
                </VuiTypography>
              </Button>
            );
          })}
        </VuiBox>

        {/* Main Pinout Visualizer Grid */}
        <Grid container spacing={2.5} mb={2.5}>
          {/* Left Column: Visual IDC Connector (2x5 Grid) */}
          <Grid item xs={12} lg={6}>
            <Card sx={{ ...cardSx, height: "100%" }}>
              <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1.5}>
                <VuiBox>
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    {activePort.name} - Header IDC (2x5 Pins)
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" display="block">
                    Sơ đồ chân nhìn từ mặt trên PCB. Click vào từng chân để xem chi tiết.
                  </VuiTypography>
                </VuiBox>
                <Chip
                  size="small"
                  label={activePort.badge}
                  sx={{
                    height: 22,
                    fontSize: "0.72rem",
                    color: "#00d4ff",
                    background: "rgba(0, 212, 255, 0.12)",
                    border: "1px solid rgba(0, 212, 255, 0.3)",
                  }}
                />
              </VuiBox>

              {/* Clean Flat PCB Header Box without neon glow */}
              <VuiBox
                sx={{
                  p: 2.5,
                  borderRadius: "12px",
                  background: "rgba(10, 15, 38, 0.9)",
                  border: "1px solid rgba(255, 255, 255, 0.12)",
                  my: 1.5,
                }}
              >
                {/* PCB Silk Label */}
                <VuiBox display="flex" justifyContent="space-between" mb={1.8} px={0.5}>
                  <VuiTypography variant="caption" sx={{ color: "#38bdf8", fontWeight: "bold", letterSpacing: "0.5px" }}>
                    PCB SILK: {activePort.name.toUpperCase()} (TOP VIEW)
                  </VuiTypography>
                  <VuiTypography variant="caption" sx={{ color: "#94a3b8" }}>
                    PIN 1: SQUARE PAD [■]
                  </VuiTypography>
                </VuiBox>

                {/* 2x5 IDC Pin Header Grid Layout */}
                <VuiBox display="flex" flexDirection="column" gap={1.8}>
                  {/* Row 1: Even Pins (2, 4, 6, 8, 10) */}
                  <VuiBox display="flex" justifyContent="space-around" alignItems="center">
                    {[2, 4, 6, 8, 10].map((pNum) => {
                      const pObj = activePort.pins.find((x) => x.pin === pNum);
                      const isSelected = selectedPinNum === pNum;
                      const matched = isPinMatched(pObj);
                      const style = PIN_TYPES[pObj.type] || PIN_TYPES.BUS;

                      return (
                        <Tooltip key={pNum} title={`${pObj.name} (${pObj.voltage})`} arrow>
                          <VuiBox
                            onClick={() => setSelectedPinNum(pNum)}
                            sx={{
                              width: 54,
                              height: 54,
                              borderRadius: "50%",
                              display: "flex",
                              flexDirection: "column",
                              alignItems: "center",
                              justifyContent: "center",
                              cursor: "pointer",
                              background: isSelected ? style.border : style.bg,
                              border: isSelected
                                ? "2px solid #ffffff"
                                : matched
                                ? "2px solid #38bdf8"
                                : `1px solid ${style.border}`,
                              boxShadow: "none",
                              transition: "all 0.15s ease",
                              "&:hover": {
                                borderColor: "#ffffff",
                              },
                            }}
                          >
                            <VuiTypography
                              variant="caption"
                              fontWeight="bold"
                              sx={{
                                color: isSelected ? "#ffffff" : style.color,
                                fontSize: "11px",
                                lineHeight: 1,
                              }}
                            >
                              P{pNum}
                            </VuiTypography>
                            <VuiTypography
                              variant="caption"
                              sx={{
                                color: "#ffffff",
                                fontSize: "9px",
                                mt: "2px",
                                textAlign: "center",
                                maxWidth: "48px",
                                overflow: "hidden",
                                textOverflow: "ellipsis",
                                whiteSpace: "nowrap",
                              }}
                            >
                              {pObj.name}
                            </VuiTypography>
                          </VuiBox>
                        </Tooltip>
                      );
                    })}
                  </VuiBox>

                  {/* Divider line representing IDC Key notch */}
                  <VuiBox
                    sx={{
                      height: "1px",
                      background: "rgba(255, 255, 255, 0.12)",
                      my: 0.5,
                    }}
                  />

                  {/* Row 2: Odd Pins (1, 3, 5, 7, 9) */}
                  <VuiBox display="flex" justifyContent="space-around" alignItems="center">
                    {[1, 3, 5, 7, 9].map((pNum) => {
                      const pObj = activePort.pins.find((x) => x.pin === pNum);
                      const isSelected = selectedPinNum === pNum;
                      const matched = isPinMatched(pObj);
                      const style = PIN_TYPES[pObj.type] || PIN_TYPES.BUS;

                      return (
                        <Tooltip key={pNum} title={`${pObj.name} (${pObj.voltage})`} arrow>
                          <VuiBox
                            onClick={() => setSelectedPinNum(pNum)}
                            sx={{
                              width: 54,
                              height: 54,
                              // Square pad for Pin 1 like physical PCB
                              borderRadius: pObj.isSquare ? "8px" : "50%",
                              display: "flex",
                              flexDirection: "column",
                              alignItems: "center",
                              justifyContent: "center",
                              cursor: "pointer",
                              background: isSelected ? style.border : style.bg,
                              border: isSelected
                                ? "2px solid #ffffff"
                                : matched
                                ? "2px solid #38bdf8"
                                : `1px solid ${style.border}`,
                              boxShadow: "none",
                              transition: "all 0.15s ease",
                              "&:hover": {
                                borderColor: "#ffffff",
                              },
                            }}
                          >
                            <VuiTypography
                              variant="caption"
                              fontWeight="bold"
                              sx={{
                                color: isSelected ? "#ffffff" : style.color,
                                fontSize: "11px",
                                lineHeight: 1,
                              }}
                            >
                              P{pNum} {pObj.isSquare ? "■" : ""}
                            </VuiTypography>
                            <VuiTypography
                              variant="caption"
                              sx={{
                                color: "#ffffff",
                                fontSize: "9px",
                                mt: "2px",
                                textAlign: "center",
                                maxWidth: "48px",
                                overflow: "hidden",
                                textOverflow: "ellipsis",
                                whiteSpace: "nowrap",
                              }}
                            >
                              {pObj.name}
                            </VuiTypography>
                          </VuiBox>
                        </Tooltip>
                      );
                    })}
                  </VuiBox>
                </VuiBox>
              </VuiBox>

              {/* Clean Flat Legend */}
              <VuiBox display="flex" flexWrap="wrap" gap={2} mt={1.5}>
                {Object.entries(PIN_TYPES).map(([k, v]) => (
                  <VuiBox key={k} display="flex" alignItems="center" gap={0.8}>
                    <VuiBox
                      sx={{
                        width: 10,
                        height: 10,
                        borderRadius: "2px",
                        backgroundColor: v.color,
                      }}
                    />
                    <VuiTypography variant="caption" sx={{ color: "rgba(255,255,255,0.7)", fontSize: "11px" }}>
                      {v.label}
                    </VuiTypography>
                  </VuiBox>
                ))}
              </VuiBox>
            </Card>
          </Grid>

          {/* Right Column: Pin Inspector Details - Flat & Clean */}
          <Grid item xs={12} lg={6}>
            <Card sx={{ ...cardSx, height: "100%" }}>
              {/* Header Details */}
              <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={2}>
                <VuiBox display="flex" alignItems="center" gap={1.5}>
                  <VuiBox
                    sx={{
                      width: 42,
                      height: 42,
                      borderRadius: activePin.isSquare ? "8px" : "50%",
                      background: PIN_TYPES[activePin.type]?.color || "#38bdf8",
                      display: "grid",
                      placeItems: "center",
                      color: "#060b28",
                      fontWeight: "bold",
                      fontSize: "17px",
                    }}
                  >
                    {activePin.pin}
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="h5" color="white" fontWeight="bold">
                      {activePin.name}
                    </VuiTypography>
                    <VuiTypography variant="caption" sx={{ color: PIN_TYPES[activePin.type]?.color, fontWeight: "bold" }}>
                      {PIN_TYPES[activePin.type]?.label}
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>
                <Chip
                  label={activePin.voltage}
                  size="small"
                  sx={{
                    backgroundColor: "rgba(255, 255, 255, 0.08)",
                    color: "#fff",
                    fontWeight: "bold",
                    border: "1px solid rgba(255, 255, 255, 0.15)",
                  }}
                />
              </VuiBox>

              {/* Technical Description Box */}
              <VuiBox
                sx={{
                  p: 2,
                  borderRadius: "10px",
                  background: "rgba(0, 0, 0, 0.2)",
                  border: "1px solid rgba(255, 255, 255, 0.08)",
                  mb: 1.8,
                }}
              >
                <VuiTypography variant="caption" color="text" display="block" mb={0.4} fontWeight="bold">
                  CHỨC NĂNG KỸ THUẬT:
                </VuiTypography>
                <VuiTypography variant="button" color="white" display="block" sx={{ lineHeight: 1.6, fontSize: "12px" }}>
                  {activePin.desc}
                </VuiTypography>
              </VuiBox>

              {/* Safety & Warning Box - Clean Minimalist Style */}
              <VuiBox
                sx={{
                  p: 1.8,
                  borderRadius: "10px",
                  background: "rgba(239, 68, 68, 0.08)",
                  border: "1px solid rgba(239, 68, 68, 0.25)",
                  mb: 1.8,
                }}
              >
                <VuiTypography variant="caption" sx={{ color: "#f87171", fontWeight: "bold", display: "block", mb: 0.3 }}>
                  LƯU Ý ĐẤU DÂY & BẢO VỆ CHỐNG CHÁY MẠCH:
                </VuiTypography>
                <VuiTypography variant="caption" sx={{ color: "#fca5a5", display: "block", fontSize: "11.5px" }}>
                  {activePin.warning}
                </VuiTypography>
              </VuiBox>

              {/* Port Summary & Recommended sensors */}
              <VuiBox mt="auto" pt={1}>
                <VuiTypography variant="caption" color="text" display="block" mb={0.3}>
                  ỨNG DỤNG KHUYÊN DÙNG CHO {activePort.name.toUpperCase()}:
                </VuiTypography>
                <VuiTypography variant="caption" sx={{ color: "#34d399", fontWeight: "bold" }}>
                  {activePort.recommended}
                </VuiTypography>
              </VuiBox>
            </Card>
          </Grid>
        </Grid>

        {/* Section: Wiring Encyclopedia Guide - Clean Table/List without icon overload */}
        <Card sx={{ ...cardSx, mb: 2.5 }}>
          <VuiBox mb={2}>
            <VuiTypography variant="button" color="white" fontWeight="bold">
              Cẩm Nang Đấu Dây Cảm Biến Chuẩn (Wiring Encyclopedia)
            </VuiTypography>
            <VuiTypography variant="caption" color="text" display="block">
              Chi tiết các chân nối cho từng dòng cảm biến thực tế trong dự án
            </VuiTypography>
          </VuiBox>

          <Grid container spacing={2}>
            {SENSOR_WIRING_GUIDES.map((guide, idx) => (
              <Grid item xs={12} md={6} lg={4} key={idx}>
                <VuiBox
                  sx={{
                    p: 2,
                    borderRadius: "12px",
                    background: "rgba(10, 15, 38, 0.6)",
                    border: "1px solid rgba(255, 255, 255, 0.08)",
                    height: "100%",
                    display: "flex",
                    flexDirection: "column",
                  }}
                >
                  <VuiBox mb={1}>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                      {guide.sensorName}
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                      {guide.category}
                    </VuiTypography>
                  </VuiBox>

                  <Chip
                    label={`Cổng: ${guide.recommendedPort}`}
                    size="small"
                    sx={{
                      backgroundColor: "rgba(56, 189, 248, 0.12)",
                      color: "#38bdf8",
                      fontSize: "10.5px",
                      mb: 1.5,
                      alignSelf: "flex-start",
                      height: 20,
                    }}
                  />

                  {/* Wiring List */}
                  <VuiBox display="flex" flexDirection="column" gap={0.8} mb={1.5}>
                    {guide.wiring.map((w, wIdx) => (
                      <VuiBox
                        key={wIdx}
                        display="flex"
                        justifyContent="space-between"
                        alignItems="center"
                        sx={{
                          py: 0.4,
                          px: 1,
                          borderRadius: "6px",
                          background: "rgba(255, 255, 255, 0.03)",
                        }}
                      >
                        <VuiTypography variant="caption" sx={{ color: "#fbbf24", fontWeight: "bold" }}>
                          {w.pinName}
                        </VuiTypography>
                        <VuiTypography variant="caption" color="white">
                          ➔ {w.target}
                        </VuiTypography>
                      </VuiBox>
                    ))}
                  </VuiBox>

                  <VuiTypography variant="caption" sx={{ color: "rgba(255,255,255,0.6)", mt: "auto", fontSize: "11px" }}>
                    • {guide.notes}
                  </VuiTypography>
                </VuiBox>
              </Grid>
            ))}
          </Grid>
        </Card>

        {/* Section: Jumpers & ActivePower Energy Guide */}
        <Card sx={{ ...cardSx }}>
          <VuiBox mb={2}>
            <VuiTypography variant="button" color="white" fontWeight="bold">
              Hướng Dẫn Cấu Hình Jumper & Cơ Chế Tiết Kiệm Năng Lượng
            </VuiTypography>
            <VuiTypography variant="caption" color="text" display="block">
              Tối ưu nguồn nuôi và cơ chế ngắt mở ActivePower cho các trạm chạy pin / năng lượng mặt trời
            </VuiTypography>
          </VuiBox>

          <Grid container spacing={2}>
            {/* ActivePower Explanation */}
            <Grid item xs={12} md={6}>
              <VuiBox
                sx={{
                  p: 2,
                  borderRadius: "10px",
                  background: "rgba(245, 158, 11, 0.06)",
                  border: "1px solid rgba(245, 158, 11, 0.2)",
                  height: "100%",
                }}
              >
                <VuiTypography variant="button" sx={{ color: "#fbbf24", fontWeight: "bold", display: "block", mb: 0.8 }}>
                  CƠ CHẾ NGUỒN TIẾT KIỆM ACTIVE POWER (PIN 1)
                </VuiTypography>
                <VuiTypography variant="caption" color="white" sx={{ lineHeight: 1.6, display: "block", mb: 0.8 }}>
                  Chân số 1 (ActivePower) trên cả 3 cổng IDC được mắc qua một tầng công tắc điện tử MOSFET điều khiển bởi vi điều khiển.
                </VuiTypography>
                <VuiTypography variant="caption" color="text" sx={{ lineHeight: 1.6, display: "block" }}>
                  • <b>Khi hoạt động đo</b>: MCU kéo tín hiệu ON để cấp nguồn nuôi cảm biến.<br />
                  • <b>Khi ngủ sâu (Deep Sleep)</b>: MCU kéo tín hiệu OFF để ngắt hoàn toàn dòng tĩnh của cảm biến, đưa dòng tiêu thụ về dưới <b>20µA</b>, giúp trạm hoạt động bền bỉ bằng pin nhiều tháng.
                </VuiTypography>
              </VuiBox>
            </Grid>

            {/* Solder Jumpers Explanation */}
            <Grid item xs={12} md={6}>
              <VuiBox
                sx={{
                  p: 2,
                  borderRadius: "10px",
                  background: "rgba(56, 189, 248, 0.06)",
                  border: "1px solid rgba(56, 189, 248, 0.2)",
                  height: "100%",
                }}
              >
                <VuiTypography variant="button" sx={{ color: "#38bdf8", fontWeight: "bold", display: "block", mb: 0.8 }}>
                  CẤU HÌNH JUMPER HÀN NGUỒN (P1 - P5) TRÊN PCB
                </VuiTypography>
                <VuiTypography variant="caption" color="white" sx={{ lineHeight: 1.6, display: "block", mb: 0.8 }}>
                  Khu vực giữa các cổng IDC trên PCB có các pad hàn jumper nguồn:
                </VuiTypography>
                <VuiTypography variant="caption" color="text" sx={{ lineHeight: 1.6, display: "block" }}>
                  • <b>Jumper P1, P2, P3 (+5V)</b>: Kết nối đường nguồn dương 5V đầu vào vào ray nguồn `5V_Peri` của các port.<br />
                  • <b>Jumper P4, P5 (GND)</b>: Nối mass chung công suất cao chống nhiễu ground loop.<br />
                  • <b>Nút bấm BOOT</b>: Dùng đưa vi điều khiển về chế độ nạp Firmware UART qua cổng debug khi nạp bằng cáp USB-TTL.
                </VuiTypography>
              </VuiBox>
            </Grid>
          </Grid>
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}
