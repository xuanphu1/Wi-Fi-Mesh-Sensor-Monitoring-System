import React, { useState, useEffect, useMemo } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import TextField from "@mui/material/TextField";
import Button from "@mui/material/Button";
import Switch from "@mui/material/Switch";
import Checkbox from "@mui/material/Checkbox";
import FormControlLabel from "@mui/material/FormControlLabel";
import LinearProgress from "@mui/material/LinearProgress";
import MenuItem from "@mui/material/MenuItem";
import Chip from "@mui/material/Chip";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

// ApexCharts
import ReactApexChart from "react-apexcharts";

// Realtime Hook
import { useDashboardRealtime } from "hooks/useDashboardRealtime";

// Icons
import {
  IoSaveOutline,
  IoReloadOutline,
  IoTimeOutline,
  IoHardwareChipOutline,
  IoServerOutline,
  IoDocumentTextOutline,
  IoReloadCircleOutline,
  IoColorWandOutline,
  IoDownloadOutline,
  IoScanCircleOutline,
  IoMailOutline,
  IoSettingsOutline,
  IoCheckmarkCircle,
  IoPulseOutline,
  IoStatsChartOutline,
  IoGlobeOutline
} from "react-icons/io5";

// Glassmorphism card styling
const glassCardSx = {
  padding: "24px",
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.10)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
  borderRadius: "16px",
  backdropFilter: "blur(18px)",
};

// Custom Input styling for the translucent inputs with large, clear font
const solidInputSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "14px !important",
    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.35) 0%, rgba(10, 14, 35, 0.25) 100%) !important",
    backdropFilter: "blur(18px)",
    color: "#ffffff !important",
    height: "50px",
    border: "1px solid rgba(255, 255, 255, 0.14) !important",
    boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#0075ff !important", border: "1.5px solid #0075ff !important" },
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    padding: "0 16px !important",
    fontSize: "16.5px !important",
    fontWeight: "700 !important",
    letterSpacing: "0.3px",
  },
  "& .MuiSelect-select": {
    fontSize: "16px !important",
    fontWeight: "700 !important",
    display: "flex",
    alignItems: "center",
  },
  "& .MuiSelect-icon": {
    color: "rgba(255, 255, 255, 0.85) !important",
    fontSize: "24px",
  },
};

const actionBoxSx = {
  p: 2.5,
  border: "1px solid rgba(255, 255, 255, 0.05)",
  borderRadius: "14px",
  background: "rgba(255, 255, 255, 0.02)",
  height: "100%",
  display: "flex",
  flexDirection: "column",
  justifyContent: "space-between",
  transition: "all 0.3s",
  "&:hover": {
    background: "rgba(255, 255, 255, 0.05)",
    border: "1px solid rgba(255, 255, 255, 0.12)",
  },
};

const SENSOR_METRICS = {
  0: [ // SENSOR_BME280
    { key: "temp", label: "Temperature Threshold (°C)" },
    { key: "hum", label: "Humidity Threshold (%)" },
    { key: "press", label: "Pressure Threshold (hPa)" },
  ],
  1: [ // SENSOR_MHZ14A
    { key: "co2", label: "CO₂ Threshold (ppm)" },
  ],
  2: [ // SENSOR_PMS7003
    { key: "pm1", label: "PM1.0 Threshold (µg/m³)" },
    { key: "pm25", label: "PM2.5 Threshold (µg/m³)" },
    { key: "pm10", label: "PM10 Threshold (µg/m³)" },
  ],
  3: [ // SENSOR_DHT22
    { key: "temp", label: "Temperature Threshold (°C)" },
    { key: "hum", label: "Humidity Threshold (%)" },
  ],
  4: [ // SENSOR_AHT10
    { key: "temp", label: "Temperature Threshold (°C)" },
    { key: "hum", label: "Humidity Threshold (%)" },
  ],
};

function formatUptime(secs) {
  if (!secs || isNaN(secs)) return "0s";
  const d = Math.floor(secs / 86400);
  const h = Math.floor((secs % 86400) / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = Math.floor(secs % 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

function SystemSettings() {
  const { serverMetrics, serverMetricsSeries } = useDashboardRealtime();
  const [initialHwInfo, setInitialHwInfo] = useState(null);

  // Settings State
  const [systemTimeout, setSystemTimeout] = useState(60);
  const [refreshMs, setRefreshMs] = useState(1000);
  const [selectedSensor, setSelectedSensor] = useState(1);
  const [sensorThresholds, setSensorThresholds] = useState({
    0: { temp: 35, hum: 80, press: 1013 },
    1: { co2: 800 },
    2: { pm1: 35, pm25: 35, pm10: 50 },
    3: { temp: 35, hum: 80 },
    4: { temp: 35, hum: 80 },
  });

  const [savedSuccess, setSavedSuccess] = useState(false);
  const [emailEnabled, setEmailEnabled] = useState(true);

  // Fetch initial host hardware info once on mount
  useEffect(() => {
    fetch("/api/system/info")
      .then((res) => (res.ok ? res.json() : null))
      .then((data) => {
        if (data) setInitialHwInfo(data);
      })
      .catch(() => {});
  }, []);

  const handleSaveSettings = () => {
    setSavedSuccess(true);
    setTimeout(() => setSavedSuccess(false), 3000);
  };

  const handleResetSettings = () => {
    setSystemTimeout(60);
    setRefreshMs(1000);
    setSelectedSensor(1);
    setSensorThresholds({
      0: { temp: 35, hum: 80, press: 1013 },
      1: { co2: 800 },
      2: { pm1: 35, pm25: 35, pm10: 50 },
      3: { temp: 35, hum: 80 },
      4: { temp: 35, hum: 80 },
    });
    setSavedSuccess(false);
  };

  // Resolve active hardware and metrics
  const hw = serverMetrics?.hardware || initialHwInfo?.hardware || {};
  const currentMetrics = serverMetrics || initialHwInfo?.metrics || null;

  // Real-time chart data
  const chartData = useMemo(() => {
    if (!serverMetricsSeries || serverMetricsSeries.length === 0) {
      const now = Date.now();
      const initialCpu = currentMetrics ? currentMetrics.cpuLoadPercent : 15;
      const initialRam = currentMetrics ? currentMetrics.ramUsedPercent : 45;
      return [
        { name: "CPU Load (%)", data: [{ x: now, y: initialCpu }] },
        { name: "RAM Used (%)", data: [{ x: now, y: initialRam }] },
      ];
    }
    return [
      {
        name: "CPU Load (%)",
        data: serverMetricsSeries.map((s) => ({
          x: s.t,
          y: Number((s.cpu || 0).toFixed(1)),
        })),
      },
      {
        name: "RAM Used (%)",
        data: serverMetricsSeries.map((s) => ({
          x: s.t,
          y: Number((s.ram || 0).toFixed(1)),
        })),
      },
    ];
  }, [serverMetricsSeries, currentMetrics]);

  const chartOptions = useMemo(() => ({
    chart: {
      toolbar: { show: false },
      animations: {
        enabled: true,
        easing: "linear",
        dynamicAnimation: { speed: 800 },
      },
      background: "transparent",
      fontFamily: "Plus Jakarta Display, Roboto, Helvetica, Arial, sans-serif",
    },
    tooltip: {
      theme: "dark",
      x: {
        format: "HH:mm:ss",
      },
      y: {
        formatter: (val) => `${val}%`,
      },
    },
    dataLabels: { enabled: false },
    stroke: { curve: "smooth", width: 2.5 },
    xaxis: {
      type: "datetime",
      labels: {
        style: { colors: "#c8cfca", fontSize: "11.5px" },
        formatter: (val) => {
          if (!val) return "";
          const d = new Date(val);
          return `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}:${String(d.getSeconds()).padStart(2, "0")}`;
        },
      },
      axisBorder: { show: false },
      axisTicks: { show: false },
    },
    yaxis: {
      min: 0,
      max: 100,
      tickAmount: 4,
      labels: {
        style: { colors: "#c8cfca", fontSize: "11.5px" },
        formatter: (val) => `${Math.round(val)}%`,
      },
    },
    legend: {
      show: true,
      position: "top",
      horizontalAlign: "right",
      labels: { colors: "#ffffff" },
      markers: { radius: 6 },
      fontSize: "13px",
    },
    grid: {
      strokeDashArray: 4,
      borderColor: "rgba(255, 255, 255, 0.08)",
    },
    fill: {
      type: "gradient",
      gradient: {
        shade: "dark",
        type: "vertical",
        shadeIntensity: 0,
        opacityFrom: 0.55,
        opacityTo: 0.05,
        stops: [0, 100],
      },
    },
    colors: ["#0075FF", "#8A2CFF"],
  }), []);

  const cpuPercent = currentMetrics ? Math.round(currentMetrics.cpuLoadPercent) : 0;
  const ramPercent = currentMetrics ? Math.round(currentMetrics.ramUsedPercent) : 0;
  const ramUsedGb = currentMetrics ? (currentMetrics.ramUsedMb / 1024).toFixed(1) : "0";
  const ramTotalGb = currentMetrics ? (currentMetrics.ramTotalMb / 1024).toFixed(1) : "0";

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>

        {/* Row 1: Side-by-Side System Settings & Server Monitor */}
        <Grid container spacing={3} mb={3} alignItems="stretch">
          
          {/* LEFT: System Settings Card */}
          <Grid item xs={12} lg={5} xl={5} sx={{ display: "flex" }}>
            <Card sx={{ ...glassCardSx, width: "100%", display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
              <VuiBox>
                {/* Header */}
                <VuiBox display="flex" alignItems="center" gap={1.5} mb={1}>
                  <VuiBox
                    sx={{
                      width: 42,
                      height: 42,
                      borderRadius: "12px",
                      background: "rgba(0, 117, 255, 0.15)",
                      border: "1px solid rgba(0, 117, 255, 0.35)",
                      color: "#0075ff",
                      display: "grid",
                      placeItems: "center",
                    }}
                  >
                    <IoSettingsOutline size="22px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="h5" color="white" fontWeight="bold" sx={{ fontSize: "20px" }}>
                      System Settings
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "13px" }}>
                      Tune system timeouts & sensor alert thresholds
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>

                {/* Main Inputs with Large Clear Fonts */}
                <VuiBox mt={3}>
                  <Grid container spacing={2.5}>
                    <Grid item xs={12} sm={6}>
                      <VuiTypography
                        variant="button"
                        color="white"
                        fontWeight="bold"
                        display="block"
                        mb={1}
                        sx={{ fontSize: "14.5px", letterSpacing: "0.2px" }}
                      >
                        System Timeout (s)
                      </VuiTypography>
                      <TextField
                        fullWidth
                        type="number"
                        value={systemTimeout}
                        onChange={(e) => setSystemTimeout(Number(e.target.value))}
                        sx={solidInputSx}
                      />
                    </Grid>
                    <Grid item xs={12} sm={6}>
                      <VuiTypography
                        variant="button"
                        color="white"
                        fontWeight="bold"
                        display="block"
                        mb={1}
                        sx={{ fontSize: "14.5px", letterSpacing: "0.2px" }}
                      >
                        Web Refresh (ms)
                      </VuiTypography>
                      <TextField
                        fullWidth
                        type="number"
                        value={refreshMs}
                        onChange={(e) => setRefreshMs(Number(e.target.value))}
                        sx={solidInputSx}
                      />
                    </Grid>

                    {/* Sensor Threshold Header */}
                    <Grid item xs={12} mt={1.5}>
                      <VuiBox
                        pt={2}
                        borderTop="1px solid rgba(255, 255, 255, 0.08)"
                        display="flex"
                        alignItems="center"
                        justifyContent="space-between"
                      >
                        <VuiTypography variant="h6" color="white" fontWeight="bold" sx={{ fontSize: "16.5px" }}>
                          Sensor Alert Thresholds
                        </VuiTypography>
                        <VuiTypography variant="caption" color="text" sx={{ fontSize: "13px" }}>
                          Per-node alerts
                        </VuiTypography>
                      </VuiBox>
                    </Grid>

                    <Grid item xs={12}>
                      <VuiTypography
                        variant="button"
                        color="white"
                        fontWeight="bold"
                        display="block"
                        mb={1}
                        sx={{ fontSize: "14.5px", letterSpacing: "0.2px" }}
                      >
                        Select Sensor
                      </VuiTypography>
                      <TextField
                        select
                        fullWidth
                        value={selectedSensor}
                        onChange={(e) => setSelectedSensor(Number(e.target.value))}
                        sx={solidInputSx}
                        SelectProps={{
                          MenuProps: {
                            sx: {
                              "& .MuiPaper-root": {
                                background: "#0f1535",
                                color: "white",
                                border: "1px solid rgba(255, 255, 255, 0.15)",
                                borderRadius: "12px",
                              },
                              "& .MuiMenuItem-root": {
                                fontSize: "15.5px",
                                fontWeight: "600",
                                py: 1.2,
                                "&:hover": { background: "rgba(0, 117, 255, 0.2)" },
                                "&.Mui-selected": { background: "rgba(0, 117, 255, 0.35) !important" },
                              },
                            },
                          },
                        }}
                      >
                        <MenuItem value={0}>🌿 SENSOR_BME280 (Temp / Hum / Press)</MenuItem>
                        <MenuItem value={1}>💨 SENSOR_MHZ14A (CO₂ ppm)</MenuItem>
                        <MenuItem value={2}>🌪️ SENSOR_PMS7003 (Dust PM1 / PM2.5 / PM10)</MenuItem>
                        <MenuItem value={3}>🌡️ SENSOR_DHT22 (Temp / Hum)</MenuItem>
                        <MenuItem value={4}>💧 SENSOR_AHT10 (Temp / Hum)</MenuItem>
                      </TextField>
                    </Grid>

                    {SENSOR_METRICS[selectedSensor].map((metric) => (
                      <Grid item xs={12} sm={SENSOR_METRICS[selectedSensor].length > 1 ? 6 : 12} key={metric.key}>
                        <VuiTypography
                          variant="button"
                          color="white"
                          fontWeight="bold"
                          display="block"
                          mb={1}
                          sx={{ fontSize: "14.5px", letterSpacing: "0.2px" }}
                        >
                          {metric.label}
                        </VuiTypography>
                        <TextField
                          fullWidth
                          type="number"
                          value={sensorThresholds[selectedSensor][metric.key]}
                          onChange={(e) => {
                            const val = Number(e.target.value);
                            setSensorThresholds((prev) => ({
                              ...prev,
                              [selectedSensor]: {
                                ...prev[selectedSensor],
                                [metric.key]: val,
                              },
                            }));
                          }}
                          sx={solidInputSx}
                        />
                      </Grid>
                    ))}
                  </Grid>
                </VuiBox>
              </VuiBox>

              {/* Action Buttons & Feedback */}
              <VuiBox mt={3.5} pt={2} borderTop="1px solid rgba(255, 255, 255, 0.08)">
                {savedSuccess && (
                  <VuiBox display="flex" alignItems="center" gap={1} mb={2}>
                    <IoCheckmarkCircle size="20px" color="#01f7a7" />
                    <VuiTypography variant="button" color="success" fontWeight="bold" sx={{ fontSize: "14px" }}>
                      Settings saved successfully!
                    </VuiTypography>
                  </VuiBox>
                )}
                <VuiBox display="flex" gap={1.5} flexWrap="wrap">
                  <Button
                    variant="contained"
                    onClick={handleSaveSettings}
                    sx={{
                      background: "linear-gradient(135deg, #0075FF 0%, #00B2FE 100%)",
                      "&:hover": { background: "linear-gradient(135deg, #0060d4 0%, #0099db 100%)" },
                      px: 3,
                      height: 44,
                      borderRadius: "10px",
                      fontWeight: "bold",
                      fontSize: "14.5px",
                      textTransform: "none",
                      boxShadow: "0 4px 14px rgba(0, 117, 255, 0.4)",
                    }}
                  >
                    <IoSaveOutline style={{ marginRight: 8 }} size="18px" /> Save Changes
                  </Button>
                  <Button
                    variant="outlined"
                    onClick={handleResetSettings}
                    sx={{
                      borderColor: "rgba(255,255,255,0.25)",
                      color: "#fff",
                      px: 2.5,
                      height: 44,
                      borderRadius: "10px",
                      fontSize: "14.5px",
                      textTransform: "none",
                      "&:hover": { borderColor: "rgba(255,255,255,0.5)", background: "rgba(255,255,255,0.06)" },
                    }}
                  >
                    <IoReloadOutline style={{ marginRight: 8 }} size="18px" /> Reset
                  </Button>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          {/* RIGHT: Host Server Info, Specs & Real-time Resource Charts */}
          <Grid item xs={12} lg={7} xl={7} sx={{ display: "flex" }}>
            <Card sx={{ ...glassCardSx, width: "100%", display: "flex", flexDirection: "column" }}>
              {/* Header */}
              <VuiBox display="flex" justifyContent="space-between" alignItems="center" flexWrap="wrap" gap={1} mb={2.5}>
                <VuiBox display="flex" alignItems="center" gap={1.5}>
                  <VuiBox
                    sx={{
                      width: 42,
                      height: 42,
                      borderRadius: "12px",
                      background: "rgba(1, 247, 167, 0.15)",
                      border: "1px solid rgba(1, 247, 167, 0.35)",
                      color: "#01f7a7",
                      display: "grid",
                      placeItems: "center",
                    }}
                  >
                    <IoServerOutline size="22px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="h5" color="white" fontWeight="bold" sx={{ fontSize: "20px" }}>
                      Host Server & Performance
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "13px" }}>
                      {hw.hostname || "MSMS-SERVER"} • {hw.osPlatform || "Windows"} ({hw.osArch || "x64"})
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>

                <VuiBox display="flex" alignItems="center" gap={1}>
                  <Chip
                    icon={<IoPulseOutline style={{ color: "#01f7a7" }} />}
                    label="ONLINE"
                    size="small"
                    sx={{
                      background: "rgba(1, 247, 167, 0.12)",
                      border: "1px solid rgba(1, 247, 167, 0.35)",
                      color: "#01f7a7",
                      fontWeight: "bold",
                      fontSize: "12px",
                      height: 28,
                    }}
                  />
                  <Chip
                    label={`Port ${hw.port || 9090}`}
                    size="small"
                    sx={{
                      background: "rgba(255, 255, 255, 0.06)",
                      border: "1px solid rgba(255, 255, 255, 0.15)",
                      color: "rgba(255, 255, 255, 0.9)",
                      fontSize: "12px",
                      height: 28,
                    }}
                  />
                </VuiBox>
              </VuiBox>

              {/* Resource Metrics Mini Cards */}
              <Grid container spacing={2} mb={2.5}>
                {/* CPU Card */}
                <Grid item xs={6} sm={3}>
                  <VuiBox
                    p={2}
                    borderRadius="12px"
                    border="1px solid rgba(0, 117, 255, 0.3)"
                    background="linear-gradient(135deg, rgba(0, 117, 255, 0.15) 0%, rgba(0, 117, 255, 0.05) 100%)"
                  >
                    <VuiTypography variant="button" color="text" display="block" mb={0.5} sx={{ fontSize: "13px", fontWeight: "600" }}>
                      CPU Usage
                    </VuiTypography>
                    <VuiTypography variant="h4" color="white" fontWeight="bold" sx={{ fontSize: "24px" }}>
                      {cpuPercent}%
                    </VuiTypography>
                    <VuiBox mt={1}>
                      <LinearProgress
                        variant="determinate"
                        value={cpuPercent}
                        sx={{
                          height: 5,
                          borderRadius: 2,
                          background: "rgba(255,255,255,0.1)",
                          "& .MuiLinearProgress-bar": {
                            background: cpuPercent > 80 ? "#ff285c" : cpuPercent > 60 ? "#ffb547" : "#0075ff",
                          },
                        }}
                      />
                    </VuiBox>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", mt: 0.75, display: "block" }}>
                      {hw.cpuCores || 8} Cores active
                    </VuiTypography>
                  </VuiBox>
                </Grid>

                {/* RAM Card */}
                <Grid item xs={6} sm={3}>
                  <VuiBox
                    p={2}
                    borderRadius="12px"
                    border="1px solid rgba(138, 44, 255, 0.3)"
                    background="linear-gradient(135deg, rgba(138, 44, 255, 0.15) 0%, rgba(138, 44, 255, 0.05) 100%)"
                  >
                    <VuiTypography variant="button" color="text" display="block" mb={0.5} sx={{ fontSize: "13px", fontWeight: "600" }}>
                      RAM Usage
                    </VuiTypography>
                    <VuiTypography variant="h4" color="white" fontWeight="bold" sx={{ fontSize: "24px" }}>
                      {ramPercent}%
                    </VuiTypography>
                    <VuiBox mt={1}>
                      <LinearProgress
                        variant="determinate"
                        value={ramPercent}
                        sx={{
                          height: 5,
                          borderRadius: 2,
                          background: "rgba(255,255,255,0.1)",
                          "& .MuiLinearProgress-bar": { background: "linear-gradient(90deg, #8a2cff 0%, #d946ef 100%)" },
                        }}
                      />
                    </VuiBox>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", mt: 0.75, display: "block" }}>
                      {ramUsedGb} / {ramTotalGb} GB
                    </VuiTypography>
                  </VuiBox>
                </Grid>

                {/* System Uptime */}
                <Grid item xs={6} sm={3}>
                  <VuiBox
                    p={2}
                    borderRadius="12px"
                    border="1px solid rgba(1, 247, 167, 0.3)"
                    background="linear-gradient(135deg, rgba(1, 247, 167, 0.15) 0%, rgba(1, 247, 167, 0.05) 100%)"
                  >
                    <VuiTypography variant="button" color="text" display="block" mb={0.5} sx={{ fontSize: "13px", fontWeight: "600" }}>
                      System Uptime
                    </VuiTypography>
                    <VuiTypography variant="h6" color="white" fontWeight="bold" display="block" sx={{ fontSize: "16.5px", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                      {formatUptime(currentMetrics?.uptimeS)}
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", mt: 1.2, display: "block" }}>
                      Node: {formatUptime(currentMetrics?.nodeUptimeS)}
                    </VuiTypography>
                  </VuiBox>
                </Grid>

                {/* Node & Architecture */}
                <Grid item xs={6} sm={3}>
                  <VuiBox
                    p={2}
                    borderRadius="12px"
                    border="1px solid rgba(255, 255, 255, 0.15)"
                    background="linear-gradient(135deg, rgba(255, 255, 255, 0.06) 0%, rgba(255, 255, 255, 0.02) 100%)"
                  >
                    <VuiTypography variant="button" color="text" display="block" mb={0.5} sx={{ fontSize: "13px", fontWeight: "600" }}>
                      Environment
                    </VuiTypography>
                    <VuiTypography variant="h6" color="white" fontWeight="bold" display="block" sx={{ fontSize: "16px" }}>
                      Node {hw.nodeVersion || process.version || "v20+"}
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", mt: 1.2, display: "block" }}>
                      Clients: {currentMetrics?.connectedClients ?? 1} connected
                    </VuiTypography>
                  </VuiBox>
                </Grid>
              </Grid>

              {/* Hardware Spec Ribbon */}
              <VuiBox
                p={1.75}
                mb={2}
                borderRadius="10px"
                background="rgba(0, 0, 0, 0.25)"
                border="1px solid rgba(255, 255, 255, 0.08)"
                display="flex"
                alignItems="center"
                justifyContent="space-between"
                flexWrap="wrap"
                gap={1}
              >
                <VuiBox display="flex" alignItems="center" gap={1.2}>
                  <IoHardwareChipOutline size="20px" color="#0075ff" />
                  <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "14.5px" }}>
                    {hw.cpuBrand || "Generic CPU"}
                  </VuiTypography>
                  {hw.cpuSpeed && (
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "13px" }}>
                      @{hw.cpuSpeed}
                    </VuiTypography>
                  )}
                </VuiBox>
                <VuiTypography variant="caption" color="text" sx={{ fontSize: "13px" }}>
                  OS: {hw.osDistro || hw.osPlatform || "Windows"} {hw.osRelease || ""}
                </VuiTypography>
              </VuiBox>

              {/* Real-time ApexChart Area */}
              <VuiBox flexGrow={1} sx={{ minHeight: "210px" }}>
                <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={1}>
                  <VuiBox display="flex" alignItems="center" gap={1}>
                    <IoStatsChartOutline size="18px" color="#0075ff" />
                    <VuiTypography variant="h6" color="white" fontWeight="bold" sx={{ fontSize: "16px" }}>
                      Live CPU & RAM Load Chart
                    </VuiTypography>
                  </VuiBox>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "12px" }}>
                    Updated every 3s
                  </VuiTypography>
                </VuiBox>
                <VuiBox sx={{ height: "190px" }}>
                  <ReactApexChart
                    options={chartOptions}
                    series={chartData}
                    type="area"
                    height="100%"
                  />
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

        </Grid>

        {/* Row 2: System Actions & Notification Preferences */}
        <Grid container spacing={3} mb={3}>
          
          {/* System Actions */}
          <Grid item xs={12} lg={6}>
            <Card sx={glassCardSx}>
              <VuiTypography variant="h6" color="white" fontWeight="bold" mb={2.5} sx={{ fontSize: "18px" }}>
                System Maintenance
              </VuiTypography>

              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <VuiBox sx={actionBoxSx}>
                    <VuiBox>
                      <VuiBox display="flex" alignItems="center" gap={1.2} mb={1.5}>
                        <IoReloadCircleOutline size="24px" color="#0075ff" />
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "15px" }}>
                          Restart Server
                        </VuiTypography>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" mb={2} sx={{ fontSize: "12.5px" }}>
                        Restart the backend websocket server safely.
                      </VuiTypography>
                    </VuiBox>
                    <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(0,117,255,0.5)", width: "100px", borderRadius: "8px", fontSize: "13px" }}>
                      Restart
                    </Button>
                  </VuiBox>
                </Grid>
                
                <Grid item xs={12} sm={6}>
                  <VuiBox sx={actionBoxSx}>
                    <VuiBox>
                      <VuiBox display="flex" alignItems="center" gap={1.2} mb={1.5}>
                        <IoColorWandOutline size="24px" color="#01f7a7" />
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "15px" }}>
                          Clear Cache
                        </VuiTypography>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" mb={2} sx={{ fontSize: "12.5px" }}>
                        Clear temporary SQLite cache and logs.
                      </VuiTypography>
                    </VuiBox>
                    <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(1,247,167,0.5)", color: "#01f7a7", width: "100px", borderRadius: "8px", fontSize: "13px" }}>
                      Clear
                    </Button>
                  </VuiBox>
                </Grid>

                <Grid item xs={12} sm={6}>
                  <VuiBox sx={actionBoxSx}>
                    <VuiBox>
                      <VuiBox display="flex" alignItems="center" gap={1.2} mb={1.5}>
                        <IoDownloadOutline size="24px" color="#8a2cff" />
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "15px" }}>
                          Backup Database
                        </VuiTypography>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" mb={2} sx={{ fontSize: "12.5px" }}>
                        Download SQLite database & OTA configs.
                      </VuiTypography>
                    </VuiBox>
                    <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(138,44,255,0.5)", color: "#8a2cff", width: "100px", borderRadius: "8px", fontSize: "13px" }}>
                      Backup
                    </Button>
                  </VuiBox>
                </Grid>

                <Grid item xs={12} sm={6}>
                  <VuiBox sx={actionBoxSx}>
                    <VuiBox>
                      <VuiBox display="flex" alignItems="center" gap={1.2} mb={1.5}>
                        <IoScanCircleOutline size="24px" color="#ff285c" />
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "15px" }}>
                          Factory Reset
                        </VuiTypography>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" mb={2} sx={{ fontSize: "12.5px" }}>
                        Reset all node thresholds to initial defaults.
                      </VuiTypography>
                    </VuiBox>
                    <Button variant="outlined" color="error" size="small" sx={{ borderColor: "rgba(255,40,92,0.5)", color: "#ff285c", width: "100px", borderRadius: "8px", fontSize: "13px" }}>
                      Reset
                    </Button>
                  </VuiBox>
                </Grid>
              </Grid>
            </Card>
          </Grid>

          {/* Notification Preferences */}
          <Grid item xs={12} lg={6}>
            <Card sx={{ ...glassCardSx, height: "100%", display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
              <VuiBox>
                <VuiTypography variant="h6" color="white" fontWeight="bold" mb={2.5} sx={{ fontSize: "18px" }}>
                  Notification Preferences
                </VuiTypography>

                <VuiBox display="flex" gap={2} mb={3}>
                  <VuiBox
                    sx={{
                      width: 46,
                      height: 46,
                      borderRadius: "12px",
                      background: "rgba(138,44,255,0.15)",
                      color: "#8a2cff",
                      display: "grid",
                      placeItems: "center",
                      flexShrink: 0,
                    }}
                  >
                    <IoMailOutline size="24px" />
                  </VuiBox>
                  <VuiBox width="100%">
                    <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={0.5}>
                      <VuiTypography variant="h6" color="white" fontWeight="bold" sx={{ fontSize: "16px" }}>
                        Email & Webhook Alerts
                      </VuiTypography>
                      <Switch checked={emailEnabled} onChange={() => setEmailEnabled(!emailEnabled)} color="primary" />
                    </VuiBox>
                    <VuiTypography variant="caption" color="text" display="block" mb={2} sx={{ fontSize: "13px" }}>
                      Receive instant notifications when node sensors exceed configured thresholds or nodes go offline.
                    </VuiTypography>

                    <VuiBox display="flex" alignItems="center" gap={2} flexWrap="wrap">
                      <VuiTypography variant="button" color="text" sx={{ fontSize: "13.5px", fontWeight: "600" }}>
                        Severity Filters:
                      </VuiTypography>
                      <FormControlLabel
                        control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#0075ff" } }} />}
                        label={<VuiTypography variant="button" color="white" sx={{ fontSize: "14px" }}>Info</VuiTypography>}
                      />
                      <FormControlLabel
                        control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#ffb547" } }} />}
                        label={<VuiTypography variant="button" color="white" sx={{ fontSize: "14px" }}>Warning</VuiTypography>}
                      />
                      <FormControlLabel
                        control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#ff285c" } }} />}
                        label={<VuiTypography variant="button" color="white" sx={{ fontSize: "14px" }}>Critical</VuiTypography>}
                      />
                    </VuiBox>
                  </VuiBox>
                </VuiBox>
              </VuiBox>

              <VuiBox display="flex" gap={2} pt={2} borderTop="1px solid rgba(255,255,255,0.08)">
                <Button
                  component="a"
                  href="http://meshgateway.local"
                  target="_blank"
                  rel="noreferrer"
                  variant="outlined"
                  color="info"
                  sx={{ borderColor: "rgba(0,117,255,0.5)", color: "#0075ff", px: 2.5, borderRadius: "8px", textTransform: "none", fontSize: "14px" }}
                >
                  <IoGlobeOutline style={{ marginRight: 8 }} size="18px" /> Open Gateway Web Portal
                </Button>
              </VuiBox>
            </Card>
          </Grid>

        </Grid>

      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default SystemSettings;
