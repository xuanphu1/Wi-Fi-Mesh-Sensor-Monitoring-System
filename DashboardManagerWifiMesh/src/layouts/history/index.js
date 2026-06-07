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
import { SENSOR_REGISTRY_FIELDS, sensorTypeToName } from "utils/meshUdpJsonSchema";
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
      transform: "translate(14px, 10px) scale(0.85) !important",
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

function normalizeIpRows(rows) {
  const map = new Map();
  (Array.isArray(rows) ? rows : []).forEach((r) => {
    const ip = String(r?.ip || "").trim();
    if (!ip) return;
    const lastSeen = r?.lastSeen || null;
    if (!map.has(ip)) map.set(ip, { ip, lastSeen });
  });
  return Array.from(map.values());
}

function buildPredefinedFieldOptions() {
  const out = [];
  Object.entries(SENSOR_REGISTRY_FIELDS).forEach(([typeStr, defs]) => {
    const type = Number(typeStr);
    if (!Number.isFinite(type) || type < 0) return;
    const sensor = sensorTypeToName(type);
    (defs || []).forEach((d) => {
      out.push({
        key: d.key,
        label: d.description || d.key,
        unit: d.unit || "",
        sensor,
      });
    });
  });
  const uniq = new Map();
  out.forEach((f) => {
    if (!uniq.has(f.key)) uniq.set(f.key, f);
  });
  return Array.from(uniq.values()).sort((a, b) => a.key.localeCompare(b.key));
}

function History() {
  const wsUrl = getWebSocketUrl();
  const [nodeIp, setNodeIp] = useState("");
  const [field, setField] = useState("");
  const [fromLocal, setFromLocal] = useState(() => toDateTimeLocalValue(Date.now() - 60 * 60_000));
  const [toLocal, setToLocal] = useState(() => toDateTimeLocalValue(Date.now()));
  const [ips, setIps] = useState([]);
  const [samples, setSamples] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [retryCnt, setRetryCnt] = useState(0);

  const wsRef = useRef(null);
  const fromInputRef = useRef(null);
  const toInputRef = useRef(null);
  const requestSeqRef = useRef(1);
  const metaReqIdRef = useRef(null);
  const seriesReqIdRef = useRef(null);

  const fields = useMemo(() => buildPredefinedFieldOptions(), []);

  useEffect(() => {
    if (!fields.length) return;
    setField((prev) => prev || fields[0].key);
  }, [fields]);

  useEffect(() => {
    if (!nodeIp && ips.length > 0) {
      setNodeIp(ips[0].ip);
    }
  }, [ips, nodeIp]);

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
          const list = normalizeIpRows(msg.ips);
          setIps(list);
          setNodeIp((prev) => (prev ? String(prev).trim() : "") || list[0]?.ip || "");
          return;
        }
        if (msg && msg.type === "welcome" && Array.isArray(msg.nodeSnapshot)) {
          const fallbackIps = normalizeIpRows(
            msg.nodeSnapshot.map((n) => ({ ip: n?.ip, lastSeen: n?.lastSeen || null }))
          );
          if (fallbackIps.length > 0) {
            setIps((prev) => (prev.length > 0 ? prev : fallbackIps));
            setNodeIp((prev) => (prev ? String(prev).trim() : "") || fallbackIps[0]?.ip || "");
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
    if (!nodeIp || !field) {
      setSamples([]);
      return;
    }
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) {
      setError("Waiting for connection...");
      return;
    }
    setLoading(true);
    setError(""); // Clear error before fetching
    const from = fromLocal ? new Date(fromLocal).toISOString() : buildRangeStartIso("1h");
    const to = toLocal ? new Date(toLocal).toISOString() : new Date().toISOString();
    const requestId = `series-${requestSeqRef.current++}`;
    seriesReqIdRef.current = requestId;
    wsRef.current.send(
      JSON.stringify({
        type: "history_series_request",
        requestId,
        ip: nodeIp,
        field,
        from,
        to,
        limit: 2000,
      })
    );
  }, [nodeIp, field, fromLocal, toLocal, retryCnt]);

  const chartPoints = useMemo(
    () =>
      samples
        .map((s) => ({ x: new Date(s.time).getTime(), y: Number(s.value) }))
        .filter((p) => Number.isFinite(p.x) && Number.isFinite(p.y)),
    [samples]
  );
  const selectedFieldMeta = useMemo(() => fields.find((f) => f.key === field) || null, [fields, field]);
  const unit = selectedFieldMeta?.unit || "";

  const chartData = useMemo(
    () => [
      {
        name: selectedFieldMeta?.sensorName ? `${selectedFieldMeta.sensorName} · ${field}` : field || "value",
        data: chartPoints,
      },
    ],
    [selectedFieldMeta, field, chartPoints]
  );

  const chartOptions = useMemo(
    () => ({
      ...lineChartOptionsDashboard,
      xaxis: {
        ...lineChartOptionsDashboard.xaxis,
        type: "datetime",
        categories: undefined,
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
          <Grid item xs={12} md={3}>
            <FormControl fullWidth variant="outlined" sx={historyFilterSx}>
              <InputLabel id="history-ip-label" shrink>
                Select IP
              </InputLabel>
              <Select
                labelId="history-ip-label"
                id="history-ip"
                value={nodeIp}
                onChange={(e) => setNodeIp(String(e.target.value || "").trim())}
                MenuProps={historySelectMenuProps}
                IconComponent={(props) => (
                  <div
                    {...props}
                    style={{
                      right: 14,
                      position: "absolute",
                      pointerEvents: "none",
                      display: "flex",
                      gap: "8px",
                      color: "rgba(255,255,255,0.4)",
                    }}
                  >
                    <IoDesktopOutline size="16px" />
                    <IoChevronDown size="16px" />
                  </div>
                )}
              >
                {ips.length === 0 ? (
                  <MenuItem value="" disabled>
                    No IP found in database
                  </MenuItem>
                ) : null}
                {ips.map((n) => (
                  <MenuItem key={n.ip} value={n.ip}>
                    {n.ip}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} md={3}>
            <FormControl fullWidth variant="outlined" sx={historyFilterSx}>
              <InputLabel id="history-field-label" shrink>
                Select field
              </InputLabel>
              <Select
                labelId="history-field-label"
                id="history-field"
                value={field}
                onChange={(e) => setField(e.target.value)}
                MenuProps={historySelectMenuProps}
                IconComponent={(props) => (
                  <div
                    {...props}
                    style={{
                      right: 14,
                      position: "absolute",
                      pointerEvents: "none",
                      display: "flex",
                      gap: "8px",
                      color: "rgba(255,255,255,0.4)",
                    }}
                  >
                    <IoAnalyticsOutline size="16px" />
                    <IoChevronDown size="16px" />
                  </div>
                )}
              >
                {fields.map((s) => (
                  <MenuItem key={s.key} value={s.key}>
                    {`${s.label} (${s.key}${s.unit ? `, ${s.unit}` : ""})`}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} md={3}>
            <TextField
              fullWidth
              type="datetime-local"
              label="From time"
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
          <Grid item xs={12} md={3}>
            <TextField
              fullWidth
              type="datetime-local"
              label="To time"
              value={toLocal}
              onChange={(e) => setToLocal(e.target.value)}
              inputRef={toInputRef}
              onClick={() => openNativeDateTimePicker(toInputRef.current)}
              onFocus={() => openNativeDateTimePicker(toInputRef.current)}
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
                IP: {nodeIp || "-"} • Field: {selectedFieldMeta?.label || field || "-"} • From:{" "}
                {fromLocal ? fromLocal.replace("T", " ") : "-"} • To: {toLocal ? toLocal.replace("T", " ") : "-"} •
                Points: {samples.length}
              </VuiTypography>
            </VuiBox>
            <VuiBox display="flex" gap={1}>
              <Button
                variant="outlined"
                color="white"
                size="small"
                sx={{ borderRadius: "8px", borderColor: "rgba(255,255,255,0.1)" }}
              >
                <IoDownloadOutline style={{ marginRight: 6 }} size="14px" /> Download{" "}
                <IoChevronDown style={{ marginLeft: 6 }} size="14px" />
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
              title="Duration"
              value={summary.dur}
              sub="-"
              icon={<IoTimerOutline size="18px" />}
              iconBg="rgba(255,40,92,0.2)"
              iconColor="#ff285c"
            />
          </VuiBox>
        </Card>

        <VuiTypography variant="caption" color="text" display="block" mt={3} mb={1}>
          Showing data from {fromLocal ? new Date(fromLocal).toLocaleString() : "-"} to{" "}
          {toLocal ? new Date(toLocal).toLocaleString() : "-"}
        </VuiTypography>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default History;

