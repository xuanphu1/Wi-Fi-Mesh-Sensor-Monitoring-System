import { useEffect, useMemo, useRef, useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import InputLabel from "@mui/material/InputLabel";
import Select from "@mui/material/Select";
import TextField from "@mui/material/TextField";
import InputAdornment from "@mui/material/InputAdornment";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";
import Button from "@mui/material/Button";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

// Charts (re-use existing examples)
import LineChart from "examples/Charts/LineCharts/LineChart";

import { getWebSocketUrl } from "utils/wsConfig";
import { SENSOR_REGISTRY_FIELDS, sensorTypeToName, SensorType } from "utils/meshUdpJsonSchema";
import { lineChartOptionsDashboard } from "layouts/dashboard/data/lineChartOptions";

import {
  IoDesktopOutline,
  IoAnalyticsOutline,
  IoCalendarOutline,
  IoClose,
  IoReload,
  IoDownloadOutline,
  IoExpand,
  IoBarChart,
  IoTrendingDown,
  IoTrendingUp,
  IoCube,
  IoTimerOutline,
  IoChevronDown,
} from "react-icons/io5";

const historyFilterSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "16px !important",
    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
    color: "#ffffff !important",
    height: "64px",
    border: "1px solid rgba(255, 255, 255, 0.08) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#4318ff !important", border: "1px solid #4318ff !important" },
  },
  "& .MuiInputLabel-root": {
    color: "rgba(255, 255, 255, 0.45) !important",
    "&.Mui-focused, &.MuiInputLabel-shrink": {
      color: "#ffffff !important",
    },
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    padding: "24px 40px 8px 14px !important",
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    padding: "24px 40px 8px 14px !important",
  },
  "& input[type='datetime-local']::-webkit-calendar-picker-indicator": {
    opacity: 0,
    width: "100%",
    height: "100%",
    position: "absolute",
    top: 0,
    left: 0,
    cursor: "pointer",
  },
};

const historySelectMenuProps = {
  PaperProps: {
    sx: {
      mt: 1,
      borderRadius: "12px",
      background: "linear-gradient(127deg, rgba(20, 21, 55, 0.97) 0%, rgba(25, 26, 65, 0.98) 100%)",
      border: "1px solid rgba(255, 255, 255, 0.12)",
      "& .MuiMenuItem-root": { color: "#fff", fontSize: "0.875rem" },
      "& .MuiMenuItem-root:hover": { backgroundColor: "rgba(67, 24, 255, 0.22)" },
      "& .MuiMenuItem-root.Mui-selected": { backgroundColor: "rgba(255, 255, 255, 0.35)" },
    },
  },
};

function SummaryCard({ title, value, sub, icon, iconBg, iconColor }) {
  return (
    <VuiBox
      flex={1}
      minWidth="180px"
      p={2}
      sx={{
        background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
        borderRadius: "16px",
        border: "1px solid rgba(255,255,255,0.05)",
      }}
    >
      <VuiBox display="flex" alignItems="center" gap={2}>
        <VuiBox
          sx={{
            minWidth: 40,
            width: 40,
            height: 40,
            borderRadius: "10px",
            background: iconBg,
            color: iconColor,
            display: "grid",
            placeItems: "center",
          }}
        >
          {icon}
        </VuiBox>
        <VuiBox overflow="hidden">
          <VuiTypography variant="caption" color="text" display="block" noWrap>
            {title}
          </VuiTypography>
          <VuiTypography variant="button" color="white" fontWeight="bold" display="block" noWrap>
            {value}
          </VuiTypography>
          <VuiTypography variant="caption" color="text" display="block" sx={{ opacity: 0.5 }}>
            {sub}
          </VuiTypography>
        </VuiBox>
      </VuiBox>
    </VuiBox>
  );
}

function buildRangeStartIso(range) {
  const now = Date.now();
  const deltaMs =
    range === "15m"
      ? 15 * 60_000
      : range === "1h"
      ? 60 * 60_000
      : range === "6h"
      ? 6 * 60 * 60_000
      : range === "24h"
      ? 24 * 60 * 60_000
      : 7 * 24 * 60 * 60_000;
  return new Date(now - deltaMs).toISOString();
}

function toDateTimeLocalValue(ms) {
  const d = new Date(ms);
  const pad = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function openNativeDateTimePicker(inputEl) {
  if (!inputEl) return;
  if (typeof inputEl.showPicker === "function") {
    try {
      inputEl.showPicker();
      return;
    } catch {
      // Some browsers throw when picker can't be programmatically opened.
    }
  }
  inputEl.focus();
}

function normalizeMacRows(rows) {
  const map = new Map();
  (Array.isArray(rows) ? rows : []).forEach((r) => {
    const mac = String(r?.mac || r?.ip || "").trim(); // fallback to ip if needed
    if (!mac) return;
    const lastSeen = r?.lastSeen || null;
    const sensors = r?.sensors || [];
    if (!map.has(mac)) map.set(mac, { mac, lastSeen, sensors });
  });
  return Array.from(map.values());
}

const dbSensorNames = Object.fromEntries(
  Object.entries(SensorType).map(([k, v]) => [v, k])
);

function buildSensorOptions(availableSensors = null) {
  const sensors = new Map();
  Object.entries(SENSOR_REGISTRY_FIELDS).forEach(([typeStr, defs]) => {
    const type = Number(typeStr);
    if (!Number.isFinite(type) || type < 0) return;
    const dbName = dbSensorNames[type] || `UNKNOWN_${type}`;
    
    // If availableSensors is provided, only include sensors in that list
    if (availableSensors && !availableSensors.includes(dbName)) return;

    const sensorLabel = sensorTypeToName(type);
    if (!sensors.has(dbName)) {
      sensors.set(dbName, { label: sensorLabel, fields: [] });
    }
    (defs || []).forEach((d) => {
      sensors.get(dbName).fields.push({
        key: d.key,
        label: d.description || d.key,
        unit: d.unit || "",
      });
    });
  });
  
  if (!availableSensors || availableSensors.includes("System")) {
    if (!sensors.has("System")) {
      sensors.set("System", { label: "System", fields: [] });
    }
    sensors.get("System").fields.push({
      key: "packetloss",
      label: "Packet Loss",
      unit: "%"
    });
  }

  if (!availableSensors || availableSensors.includes("Gateway")) {
    if (!sensors.has("Gateway")) {
      sensors.set("Gateway", { label: "Gateway", fields: [] });
    }
    sensors.get("Gateway").fields.push(
      { key: "cpu_load", label: "CPU Load", unit: "%" },
      { key: "ram_used", label: "RAM Used", unit: "%" },
      { key: "wifi_rssi", label: "Wi-Fi RSSI", unit: "dBm" },
      { key: "battery_v", label: "Battery Voltage", unit: "V" },
      { key: "uptime_s", label: "Uptime", unit: "s" }
    );
  }

  return Array.from(sensors.entries()).map(([name, data]) => ({
    name,
    label: data.label,
    fields: data.fields
  })).sort((a, b) => a.label.localeCompare(b.label));
}

function History() {
  const wsUrl = getWebSocketUrl();
  const [nodeMac, setNodeMac] = useState("");
  const [sensor, setSensor] = useState("");
  const [field, setField] = useState("");
  const [fromLocal, setFromLocal] = useState(() => toDateTimeLocalValue(Date.now() - 60 * 60_000));
  const [macs, setMacs] = useState([]);
  const [samples, setSamples] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [retryCnt, setRetryCnt] = useState(0);
  const [exportProgress, setExportProgress] = useState(null);

  const wsRef = useRef(null);
  const fromInputRef = useRef(null);
  const requestSeqRef = useRef(1);
  const metaReqIdRef = useRef(null);
  const seriesReqIdRef = useRef(null);

  const handleDownloadCsv = () => {
    if (!samples || samples.length === 0) return;
    const header = `Time,Value\n`;
    const rows = samples.map(s => {
      // s.time might be a valid date string
      let timeStr = s.time;
      try { timeStr = new Date(s.time).toISOString(); } catch(e){}
      return `${timeStr},${s.value}`;
    }).join('\n');
    const blob = new Blob([header + rows], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    const safeMac = (nodeMac || "unknown").replace(/:/g, "-");
    a.download = `history_${safeMac}_${sensor}_${field}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleDownloadAllMacCsv = () => {
    if (!nodeMac || !wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    setExportProgress({ current: 0, total: 1 });
    const requestId = `export-${requestSeqRef.current++}`;
    wsRef.current.send(JSON.stringify({
      type: "history_export_mac_request",
      requestId,
      mac: nodeMac,
      from: null,
      to: null,
    }));
  };

  const availableSensorsForMac = useMemo(() => {
    if (!nodeMac) return null;
    const macData = macs.find(m => m.mac === nodeMac);
    return macData && Array.isArray(macData.sensors) && macData.sensors.length > 0 ? macData.sensors : null;
  }, [macs, nodeMac]);

  const sensorConfig = useMemo(() => buildSensorOptions(availableSensorsForMac), [availableSensorsForMac]);
  
  const currentSensorFields = useMemo(() => {
    const match = sensorConfig.find(s => s.name === sensor);
    return match ? match.fields : [];
  }, [sensorConfig, sensor]);

  // If the currently selected sensor is no longer in the list of available sensors, select the first available one
  useEffect(() => {
    if (sensorConfig.length > 0 && (!sensor || !sensorConfig.find(s => s.name === sensor))) {
      setSensor(sensorConfig[0].name);
    }
  }, [sensorConfig, sensor]);

  useEffect(() => {
    if (currentSensorFields.length > 0 && (!field || !currentSensorFields.find(f => f.key === field))) {
      setField(currentSensorFields[0].key);
    }
  }, [currentSensorFields, field]);

  useEffect(() => {
    if (!nodeMac && macs.length > 0) {
      setNodeMac(macs[0].mac);
    }
  }, [macs, nodeMac]);

  useEffect(() => {
    if (!wsUrl) {
      setError("WebSocket URL is empty");
      return undefined;
    }
    let disposed = false;
    let reconnectTimer;
    let metaTimer;

    const requestMeta = () => {
      if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
      const requestId = `meta-${requestSeqRef.current++}`;
      metaReqIdRef.current = requestId;
      wsRef.current.send(JSON.stringify({ type: "history_meta_request", requestId, limit: 300 }));
    };

    const open = () => {
      if (disposed) return;
      try {
        wsRef.current = new WebSocket(wsUrl);
      } catch {
        reconnectTimer = setTimeout(open, 3000);
        return;
      }

      wsRef.current.onopen = () => {
        requestMeta();
        metaTimer = setInterval(requestMeta, 5000);
      };

      wsRef.current.onmessage = (ev) => {
        if (typeof ev.data !== "string") return;
        let msg;
        try {
          msg = JSON.parse(ev.data);
        } catch {
          return;
        }
        if (msg && msg.type === "history_meta_response") {
          if (msg.requestId && metaReqIdRef.current && msg.requestId !== metaReqIdRef.current) return;
          if (msg.error) {
            setError(String(msg.error));
            return;
          }
          const list = normalizeMacRows(msg.macs || msg.ips);
          setMacs(list);
          setNodeMac((prev) => (prev ? String(prev).trim() : "") || list[0]?.mac || "");
          return;
        }

        if (msg && msg.type === "history_export_mac_response") {
          if (msg.error) {
            alert(`Export failed: ${msg.error}`);
            return;
          }
          if (msg.csv) {
            const blob = new Blob([msg.csv], { type: 'text/csv;charset=utf-8;' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            const safeMac = (msg.mac || "unknown").replace(/:/g, "-");
            a.download = `history_all_${safeMac}.csv`;
            a.click();
            URL.revokeObjectURL(url);
          }
          return;
        }

        if (msg && msg.type === "welcome" && Array.isArray(msg.nodeSnapshot)) {
          const fallbackMacs = normalizeMacRows(
            msg.nodeSnapshot.map((n) => ({ mac: n?.mac || n?.ip, lastSeen: n?.lastSeen || null }))
          );
          if (fallbackMacs.length > 0) {
            setMacs((prev) => (prev.length > 0 ? prev : fallbackMacs));
            setNodeMac((prev) => (prev ? String(prev).trim() : "") || fallbackMacs[0]?.mac || "");
          }
          return;
        }
        if (msg && msg.type === "history_series_response") {
          if (msg.requestId && seriesReqIdRef.current && msg.requestId !== seriesReqIdRef.current) return;
          if (msg.error) {
            setError(String(msg.error));
            setLoading(false);
            return;
          }
          setSamples(Array.isArray(msg.items) ? msg.items : []);
          setLoading(false);
          setError("");
        } else if (msg && msg.type === "history_export_mac_progress") {
          setExportProgress({ current: msg.progress, total: msg.total || 1 });
        } else if (msg && msg.type === "history_export_mac_response") {
          setExportProgress(null);
          if (msg.error) {
            alert(`Export failed: ${msg.error}`);
            return;
          }
          if (msg.csv) {
            const blob = new Blob([msg.csv], { type: 'text/csv;charset=utf-8;' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            const safeMac = (msg.mac || "unknown").replace(/:/g, "-");
            a.download = `export_all_${safeMac}.csv`;
            a.click();
            URL.revokeObjectURL(url);
          }
        }
      };

      wsRef.current.onclose = () => {
        if (disposed) return;
        if (metaTimer) clearInterval(metaTimer);
        reconnectTimer = setTimeout(open, 3000);
      };

      wsRef.current.onerror = () => {
        setError("Failed to fetch");
        setLoading(false);
      };
    };

    open();

    return () => {
      disposed = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      if (metaTimer) clearInterval(metaTimer);
      if (wsRef.current) {
        wsRef.current.onopen = null;
        wsRef.current.onmessage = null;
        wsRef.current.onclose = null;
        wsRef.current.onerror = null;
        wsRef.current.close();
      }
    };
  }, [wsUrl]);

  useEffect(() => {
    if (!nodeMac || !sensor || !field) {
      setSamples([]);
      return;
    }
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) {
      setError("Waiting for connection...");
      return;
    }
    setLoading(true);
    setError(""); // Clear error before fetching
    const from = fromLocal ? new Date(fromLocal).toISOString() : null;
    const requestId = `series-${requestSeqRef.current++}`;
    seriesReqIdRef.current = requestId;
    wsRef.current.send(
      JSON.stringify({
        type: "history_series_request",
        requestId,
        mac: nodeMac,
        sensorName: sensor,
        field,
        from,
        to: null,
        limit: 2000,
      })
    );
  }, [nodeMac, sensor, field, fromLocal, retryCnt]);

  const chartPoints = useMemo(
    () =>
      samples
        .map((s) => ({ x: new Date(s.time).getTime(), y: Number(s.value) }))
        .filter((p) => Number.isFinite(p.x) && Number.isFinite(p.y)),
    [samples]
  );
  const selectedFieldMeta = useMemo(() => currentSensorFields.find((f) => f.key === field) || null, [currentSensorFields, field]);
  const unit = selectedFieldMeta?.unit || "";

  const selectedFieldLabel = selectedFieldMeta?.label;

  const chartData = useMemo(
    () => [
      {
        name: sensor ? `${sensor} · ${selectedFieldLabel || field}` : field || "value",
        data: chartPoints,
      },
    ],
    [sensor, selectedFieldLabel, field, chartPoints]
  );

  const chartOptions = useMemo(
    () => ({
      ...lineChartOptionsDashboard,
      xaxis: {
        ...lineChartOptionsDashboard.xaxis,
        type: "datetime",
        categories: undefined,
        labels: {
          ...lineChartOptionsDashboard.xaxis?.labels,
          datetimeUTC: false,
          formatter: function(val) {
            if (!val) return "";
            const d = new Date(val);
            return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
          }
        },
      },
      yaxis: {
        ...lineChartOptionsDashboard.yaxis,
        title: {
          text: unit ? `Value (${unit})` : "Value",
          style: { color: "#c8cfca", fontSize: "11px" },
        },
      },
    }),
    [unit]
  );

  const summary = useMemo(() => {
    const avg = chartPoints.length
      ? (chartPoints.reduce((a, b) => a + b.y, 0) / chartPoints.length).toFixed(2)
      : "0.00";
    const minVal = chartPoints.length ? Math.min(...chartPoints.map((s) => s.y)).toFixed(2) : "0.00";
    const maxVal = chartPoints.length ? Math.max(...chartPoints.map((s) => s.y)).toFixed(2) : "0.00";
    const durationMs =
      chartPoints.length >= 2 ? chartPoints[chartPoints.length - 1].x - chartPoints[0].x : 0;
    const hrs = Math.floor(durationMs / 3600000);
    const mins = Math.floor((durationMs % 3600000) / 60000);
    const durationStr = durationMs ? `${hrs}h ${mins}m` : "0h 0m";

    return { avg, min: minVal, max: maxVal, pts: chartPoints.length, dur: durationStr };
  }, [chartPoints]);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <Grid container spacing={3} mb={3}>
          <Grid item xs={12} md={4}>
            <FormControl fullWidth variant="outlined" sx={historyFilterSx}>
              <InputLabel id="history-ip-label">
                Select MAC
              </InputLabel>
              <Select
                labelId="history-ip-label"
                id="history-ip"
                value={nodeMac}
                onChange={(e) => setNodeMac(String(e.target.value || "").trim())}
                MenuProps={historySelectMenuProps}
                label="Select MAC"
              >
                {macs.length === 0 ? (
                  <MenuItem value="" disabled>
                    No MAC found in database
                  </MenuItem>
                ) : null}
                {macs.map((n) => {
                  const isGateway = n.sensors?.includes("Gateway");
                  return (
                    <MenuItem key={n.mac} value={n.mac}>
                      {isGateway ? `🌟 ${n.mac} (Gateway)` : n.mac}
                    </MenuItem>
                  );
                })}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} md={4}>
            <FormControl fullWidth variant="outlined" sx={historyFilterSx}>
              <InputLabel id="history-sensor-label">
                Select Sensor
              </InputLabel>
              <Select
                labelId="history-sensor-label"
                value={sensor}
                onChange={(e) => setSensor(e.target.value)}
                sx={{ borderRadius: "8px", background: "rgba(10, 14, 35, 0.5)", color: "white" }}
              >
                {sensorConfig.map((s) => (
                  <MenuItem key={s.name} value={s.name}>
                    {s.label}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} md={4}>
            <FormControl fullWidth variant="outlined" sx={historyFilterSx}>
              <InputLabel id="history-field-label">
                Select Field
              </InputLabel>
              <Select
                labelId="history-field-label"
                id="history-field"
                value={field}
                onChange={(e) => setField(e.target.value)}
                MenuProps={historySelectMenuProps}
                label="Select Field"
              >
                {currentSensorFields.map((s) => (
                  <MenuItem key={s.key} value={s.key}>
                    {`${s.label} (${s.key}${s.unit ? `, ${s.unit}` : ""})`}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} md={12}>
            <TextField
              fullWidth
              type="datetime-local"
              label="Start time"
              value={fromLocal}
              onChange={(e) => setFromLocal(e.target.value)}
              inputRef={fromInputRef}
              onClick={() => openNativeDateTimePicker(fromInputRef.current)}
              onFocus={() => openNativeDateTimePicker(fromInputRef.current)}
              InputLabelProps={{ shrink: true }}
              inputProps={{ step: 60 }}
              sx={historyFilterSx}
              InputProps={{
                endAdornment: (
                  <InputAdornment position="end" sx={{ pointerEvents: "none" }}>
                    <IoCalendarOutline color="rgba(255,255,255,0.4)" size="16px" />
                  </InputAdornment>
                ),
              }}
            />
          </Grid>
        </Grid>

        <Card
          sx={{
            padding: "24px 20px",
            background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
            border: "1px solid rgba(255, 255, 255, 0.08)",
            boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
            borderRadius: "16px",
            backdropFilter: "blur(42px)",
          }}
        >
          <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start" mb={3}>
            <VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold" mb={1}>
                History / Charts
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                MAC: {nodeMac || "-"} • Sensor: {sensor || "-"} • Field: {selectedFieldMeta?.label || field || "-"} • Start:{" "}
                {fromLocal ? fromLocal.replace("T", " ") : "Latest"} •
                Points: {samples.length}
              </VuiTypography>
            </VuiBox>
            <VuiBox display="flex" gap={1}>
              <Button
                variant="outlined"
                color="white"
                size="small"
                onClick={handleDownloadCsv}
                sx={{ borderRadius: "8px", borderColor: "rgba(255,255,255,0.1)" }}
              >
                <IoDownloadOutline style={{ marginRight: 6 }} size="14px" /> Download Chart CSV
              </Button>
              <Button
                variant="outlined"
                color="white"
                size="small"
                onClick={handleDownloadAllMacCsv}
                disabled={exportProgress !== null}
                sx={{ borderRadius: "8px", borderColor: "rgba(255,255,255,0.1)", position: "relative", overflow: "hidden" }}
              >
                {exportProgress !== null && (
                  <div style={{
                    position: "absolute", left: 0, top: 0, bottom: 0, 
                    width: `${Math.max(5, Math.round((exportProgress.current / exportProgress.total) * 100))}%`, 
                    background: "rgba(56, 189, 248, 0.2)", zIndex: 0,
                    transition: "width 0.2s"
                  }} />
                )}
                <span style={{ position: "relative", zIndex: 1, display: "flex", alignItems: "center" }}>
                  <IoDownloadOutline style={{ marginRight: 6 }} size="14px" /> 
                  {exportProgress !== null 
                    ? `Exporting... ${Math.round((exportProgress.current / exportProgress.total) * 100)}%`
                    : "Export ALL for MAC"}
                </span>
              </Button>
              <Button
                variant="outlined"
                color="white"
                size="small"
                sx={{ borderRadius: "8px", minWidth: "32px", p: 1, borderColor: "rgba(255,255,255,0.1)" }}
              >
                <IoExpand size="14px" />
              </Button>
            </VuiBox>
          </VuiBox>

          {error && !loading ? (
            <VuiBox
              sx={{
                background: "rgba(255,40,92,0.05)",
                border: "1px solid rgba(255,40,92,0.15)",
                borderRadius: "12px",
                p: 2.5,
                display: "flex",
                justifyContent: "space-between",
                alignItems: "center",
                mb: 3,
              }}
            >
              <VuiBox display="flex" alignItems="center" gap={2.5}>
                <VuiBox
                  sx={{
                    width: 36,
                    height: 36,
                    borderRadius: "50%",
                    background: "#ff285c",
                    color: "#fff",
                    display: "grid",
                    placeItems: "center",
                    boxShadow: "0 0 12px rgba(255,40,92,0.6)",
                  }}
                >
                  <IoClose size="18px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="button" color="error" fontWeight="bold" display="block" mb={0.5}>
                    Failed to fetch data
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text">
                    {error.includes("Waiting")
                      ? "Waiting for connection to the backend server..."
                      : "Unable to load chart data. Please check the connection or try again."}
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
              <Button
                variant="outlined"
                color="info"
                size="small"
                sx={{ borderRadius: "8px", borderColor: "rgba(0,117,255,0.3)" }}
                onClick={() => setRetryCnt((c) => c + 1)}
              >
                <IoReload style={{ marginRight: 6 }} /> Retry
              </Button>
            </VuiBox>
          ) : null}

          {loading ? (
            <VuiTypography variant="button" color="text" mb="10px" display="block">
              Loading chart data...
            </VuiTypography>
          ) : null}

          <VuiBox sx={{ height: "320px", mt: 2 }}>
            <LineChart lineChartData={chartData} lineChartOptions={chartOptions} />
          </VuiBox>

          <VuiBox display="flex" flexWrap="wrap" gap={2} mt={3}>
            <SummaryCard
              title="Average"
              value={`${summary.avg} ${unit}`}
              sub="-"
              icon={<IoBarChart size="18px" />}
              iconBg="rgba(138,44,255,0.2)"
              iconColor="#8a2cff"
            />
            <SummaryCard
              title="Min"
              value={`${summary.min} ${unit}`}
              sub="-"
              icon={<IoTrendingDown size="18px" />}
              iconBg="rgba(0,117,255,0.2)"
              iconColor="#0075ff"
            />
            <SummaryCard
              title="Max"
              value={`${summary.max} ${unit}`}
              sub="-"
              icon={<IoTrendingUp size="18px" />}
              iconBg="rgba(1,247,167,0.2)"
              iconColor="#01f7a7"
            />
            <SummaryCard
              title="Total Points"
              value={summary.pts}
              sub="-"
              icon={<IoCube size="18px" />}
              iconBg="rgba(255,181,71,0.2)"
              iconColor="#ffb547"
            />
            <SummaryCard
              title="Displayed Timespan"
              value={summary.dur}
              sub="-"
              icon={<IoTimerOutline size="18px" />}
              iconBg="rgba(255,40,92,0.2)"
              iconColor="#ff285c"
            />
          </VuiBox>
        </Card>

        <VuiTypography variant="caption" color="text" display="block" mt={3} mb={1}>
          Showing data from {fromLocal ? new Date(fromLocal).toLocaleString() : "Latest"}
        </VuiTypography>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default History;

