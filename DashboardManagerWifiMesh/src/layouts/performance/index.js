import React, { useEffect, useMemo, useRef, useState } from "react";

// @mui material components
import Grid from "@mui/material/Grid";
import { Card, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, ButtonGroup, Button } from "@mui/material";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import InputLabel from "@mui/material/InputLabel";
import Select from "@mui/material/Select";
import TextField from "@mui/material/TextField";
import InputAdornment from "@mui/material/InputAdornment";
import ReactApexChart from "react-apexcharts";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

// Icons
import {
  IoPulse,
  IoHardwareChip,
  IoWifi,
  IoAnalytics,
  IoCalendarOutline,
  IoDesktopOutline,
  IoChevronDown,
  IoReload,
  IoClose,
  IoArrowUpOutline,
  IoShieldCheckmarkOutline,
  IoWarningOutline,
  IoOpenOutline,
  IoServer,
  IoTimeOutline,
  IoSwapVertical,
} from "react-icons/io5";
import { TbBulb } from "react-icons/tb";

import { useDashboardRealtime } from "hooks/useDashboardRealtime";
import { getWebSocketUrl } from "utils/wsConfig";
import { lineChartOptionsDashboard } from "layouts/dashboard/data/lineChartOptions";

// Glassmorphism styling from existing dashboard
const glassCardSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.76) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(50, 105, 255, 0.35)",
  boxShadow: "0 0 24px rgba(0, 106, 255, 0.22), inset 0 1px 0 rgba(255,255,255,0.04)",
  borderRadius: "16px",
  backdropFilter: "blur(42px)",
  height: "100%",
};

const filterSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    background: "rgba(10, 14, 35, 0.5) !important",
    color: "#ffffff !important",
    height: "44px",
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
    background: "transparent !important",
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    paddingLeft: "14px !important",
    background: "transparent !important",
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

const selectMenuProps = {
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

function PerformanceCard({ title, value, detail, icon, color = "#0075ff", trend, trendLabel }) {
  const isUp = trend > 0;
  const trendColor = trend !== undefined ? (isUp ? "#01f7a7" : "#ff285c") : "transparent";

  return (
    <Card sx={{ ...glassCardSx, position: "relative", overflow: "hidden" }}>
      <VuiBox p={3} display="flex" alignItems="center" justifyContent="space-between">
        <VuiBox>
          <VuiTypography variant="caption" color="text" display="block" mb={1}>
            {title}
          </VuiTypography>
          <VuiBox display="flex" alignItems="baseline" gap={1} flexWrap="wrap">
            <VuiTypography variant="h3" color="white" fontWeight="bold" sx={{ lineHeight: 1 }}>
              {value}
            </VuiTypography>
            {detail && (
              <VuiTypography variant="button" color="text" fontWeight="medium">
                {detail}
              </VuiTypography>
            )}
          </VuiBox>
          {trend !== undefined ? (
            <VuiBox display="flex" alignItems="center" gap={1} mt={1}>
              <VuiTypography variant="caption" sx={{ color: trendColor, fontWeight: "bold" }}>
                {isUp ? "↑" : "↓"} {Math.abs(trend)}%
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                {trendLabel}
              </VuiTypography>
            </VuiBox>
          ) : (
            trendLabel && (
              <VuiBox display="flex" alignItems="center" gap={1} mt={1}>
                <VuiTypography variant="caption" color="text">
                  {trendLabel}
                </VuiTypography>
              </VuiBox>
            )
          )}
        </VuiBox>
        <VuiBox
          sx={{
            width: 56,
            height: 56,
            borderRadius: "50%",
            display: "grid",
            placeItems: "center",
            color: "#fff",
            background: `radial-gradient(circle at 30% 25%, #2ee7ff 0%, ${color} 48%, #1546ff 100%)`,
            boxShadow: `0 0 22px ${color}99`,
          }}
        >
          {icon}
        </VuiBox>
      </VuiBox>
    </Card>
  );
}

function SectionTitle({ icon, children, color = "#0075ff" }) {
  return (
    <VuiBox display="flex" alignItems="center" gap={1.25} mb={2}>
      <VuiBox
        sx={{
          width: 38,
          height: 38,
          borderRadius: "50%",
          display: "grid",
          placeItems: "center",
          color: "#fff",
          background: `radial-gradient(circle, ${color} 0%, rgba(0,117,255,0.35) 100%)`,
          boxShadow: `0 0 18px ${color}88`,
        }}
      >
        {icon}
      </VuiBox>
      <VuiTypography variant="h5" color="white" fontWeight="bold">
        {children}
      </VuiTypography>
    </VuiBox>
  );
}

function toDateTimeLocalValue(ms) {
  const d = new Date(ms);
  const pad = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function openNativeDateTimePicker(inputEl) {
  if (!inputEl) return;
  if (typeof inputEl.showPicker === "function") {
    try { inputEl.showPicker(); return; } catch {}
  }
  inputEl.focus();
}

// Simple sparkline component using SVG with gradient area
function Sparkline({ data, color }) {
  if (!data || data.length === 0) return null;
  const max = Math.max(...data);
  const min = Math.min(...data);
  const range = max - min || 1;
  const width = 80;
  const height = 24;
  const points = data.map((val, i) => `${(i / (data.length - 1)) * width},${height - ((val - min) / range) * height}`).join(" ");
  const polygonPoints = `${width},${height} 0,${height} ${points}`;

  return (
    <svg width={width} height={height} style={{ overflow: 'visible' }}>
      <defs>
        <linearGradient id={`grad-${color.replace('#','')}`} x1="0%" y1="0%" x2="0%" y2="100%">
          <stop offset="0%" stopColor={color} stopOpacity="0.4" />
          <stop offset="100%" stopColor={color} stopOpacity="0.0" />
        </linearGradient>
      </defs>
      <polygon fill={`url(#grad-${color.replace('#','')})`} points={polygonPoints} />
      <polyline fill="none" stroke={color} strokeWidth="1.5" points={points} />
    </svg>
  );
}

function TopNodesTable({ nodes }) {
  const topNodes = useMemo(() => {
    return nodes.map((n) => {
      return {
        ...n,
        currentThroughput: n.currentThroughput || 0,
        avgThroughput: n.avgThroughput || 0,
        packetLoss: typeof n.packetLoss === "number" ? n.packetLoss : 0
      };
    }).sort((a,b) => {
      if (b.currentThroughput !== a.currentThroughput) return b.currentThroughput - a.currentThroughput;
      return a.packetLoss - b.packetLoss;
    }).slice(0, 5);
  }, [nodes]);

  return (
    <Card sx={{ ...glassCardSx, p: 3, height: "100%" }}>
      <VuiBox display="flex" alignItems="center" gap={1} mb={3}>
        <VuiBox sx={{ width: 24, height: 24, borderRadius: "6px", background: "rgba(138, 44, 255, 0.2)", color: "#8a2cff", display: "flex", alignItems: "center", justifyContent: "center" }}>
           <IoHardwareChip size="14px" />
        </VuiBox>
        <VuiTypography variant="h6" color="white" fontWeight="bold">
          Top Nodes (by Throughput)
        </VuiTypography>
      </VuiBox>
      
      <VuiBox sx={{ width: "100%", overflowX: "auto" }}>
        <VuiBox sx={{ minWidth: 500 }}>
          <VuiBox display="flex" alignItems="center" pb={2} mb={2} sx={{ borderBottom: "1px solid rgba(255,255,255,0.05)" }}>
            <VuiBox width="10%"><VuiTypography variant="caption" color="text" fontWeight="bold">RANK</VuiTypography></VuiBox>
            <VuiBox width="25%"><VuiTypography variant="caption" color="text" fontWeight="bold">NODE</VuiTypography></VuiBox>
            <VuiBox width="25%"><VuiTypography variant="caption" color="text" fontWeight="bold">THROUGHPUT (CURRENT)</VuiTypography></VuiBox>
            <VuiBox width="25%"><VuiTypography variant="caption" color="text" fontWeight="bold">THROUGHPUT (AVG)</VuiTypography></VuiBox>
            <VuiBox width="15%" display="flex" justifyContent="flex-end"><VuiTypography variant="caption" color="text" fontWeight="bold">PACKET LOSS</VuiTypography></VuiBox>
          </VuiBox>

          <VuiBox display="flex" flexDirection="column">
            {topNodes.map((n, idx) => {
              const lossColor = n.packetLoss === 0 ? "#01f7a7" : (n.packetLoss < 0.03 ? "#ffb547" : "#ff285c");
              return (
                <VuiBox key={n.ip} display="flex" alignItems="center" py={2.5} sx={{ borderBottom: "1px solid rgba(255,255,255,0.02)" }}>
                  <VuiBox width="10%">
                    <VuiBox 
                      sx={{ 
                        width: 40, height: 40, borderRadius: "10px", 
                        border: `1px solid ${idx === 0 ? "#ffb547" : idx === 1 ? "#cbd5e1" : idx === 2 ? "#e28743" : "rgba(255,255,255,0.2)"}`,
                        color: idx === 0 ? "#ffb547" : idx === 1 ? "#cbd5e1" : idx === 2 ? "#e28743" : "#fff",
                        background: idx === 0 ? "rgba(255, 181, 71, 0.15)" : idx === 1 ? "rgba(203, 213, 225, 0.15)" : idx === 2 ? "rgba(226, 135, 67, 0.15)" : "rgba(255,255,255,0.02)",
                        display: "flex", alignItems: "center", justifyContent: "center",
                        fontSize: "16px", fontWeight: "bold"
                      }}
                    >
                      {idx + 1}
                    </VuiBox>
                  </VuiBox>
                  <VuiBox width="25%" display="flex" alignItems="center" gap={1.5}>
                    <VuiBox sx={{ width: 8, height: 8, borderRadius: "50%", background: n.online ? "#01f7a7" : "#ff285c", boxShadow: `0 0 8px ${n.online ? "#01f7a7" : "#ff285c"}` }} />
                    <VuiTypography variant="body1" color="white" fontWeight="bold" sx={{ fontSize: "16px" }}>{n.ip || n.id}</VuiTypography>
                  </VuiBox>
                  <VuiBox width="25%">
                    <VuiTypography variant="body1" color="white" fontWeight="bold" sx={{ fontSize: "16px" }}>{n.currentThroughput} B/s</VuiTypography>
                  </VuiBox>
                  <VuiBox width="25%">
                    <VuiTypography variant="body1" color="text" sx={{ fontSize: "16px" }}>{n.avgThroughput} B/s</VuiTypography>
                  </VuiBox>
                  <VuiBox width="15%" display="flex" justifyContent="flex-end">
                    <VuiTypography variant="body1" sx={{ color: lossColor, fontWeight: "bold", fontSize: "16px" }}>
                      {n.packetLoss.toFixed(2)}%
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>
              );
            })}
          </VuiBox>
        </VuiBox>
      </VuiBox>
      
      <VuiBox mt={4}>
        <Button variant="outlined" fullWidth sx={{ 
          borderColor: "rgba(255,255,255,0.05) !important", 
          color: "#fff !important", 
          borderRadius: "8px !important",
          py: 1,
          "&:hover": { background: "rgba(255,255,255,0.02) !important" }
        }}>
          View all nodes <IoChevronDown style={{ marginLeft: 8, transform: "rotate(-90deg)" }} />
        </Button>
      </VuiBox>
    </Card>
  );
}

function HealthStatItem({ icon: Icon, title, value, subtext, color }) {
  return (
    <VuiBox display="flex" gap={2} alignItems="center">
      <VuiBox sx={{ 
        minWidth: 56, width: 56, height: 56, borderRadius: "50%", 
        background: `radial-gradient(circle, ${color}33 0%, rgba(0,0,0,0) 70%)`,
        border: `1px solid ${color}44`,
        display: "flex", alignItems: "center", justifyContent: "center",
        color: color,
      }}>
        <Icon size="28px" />
      </VuiBox>
      <VuiBox>
        <VuiTypography variant="button" color="text" display="block" sx={{ fontSize: "14px" }}>{title}</VuiTypography>
        <VuiTypography variant="h3" color="white" fontWeight="bold" sx={{ lineHeight: 1.2 }}>{value}</VuiTypography>
        <VuiTypography variant="caption" color="text" sx={{ fontSize: "13px" }}>{subtext}</VuiTypography>
      </VuiBox>
    </VuiBox>
  )
}

function NetworkHealthCard({ nodes, gatewayStatus }) {
  const onlineCount = nodes.filter(n => n.online).length;
  const offlineCount = nodes.filter(n => !n.online).length;
  
  const formatUptime = (secs) => {
    if (!secs) return "0s";
    const h = Math.floor(secs / 3600);
    const m = Math.floor((secs % 3600) / 60);
    const s = Math.floor(secs % 60);
    if (h > 0) return `${h}h ${m}m ${s}s`;
    if (m > 0) return `${m}m ${s}s`;
    return `${s}s`;
  };
  const uptimeStr = gatewayStatus?.uptimeS ? formatUptime(gatewayStatus.uptimeS) : "N/A";
  
  return (
    <Card sx={{ ...glassCardSx, p: 3, height: "100%", display: "flex", flexDirection: "column" }}>
      <VuiBox display="flex" alignItems="center" gap={1} mb={4}>
        <VuiBox sx={{ width: 24, height: 24, borderRadius: "6px", background: "rgba(1, 247, 167, 0.2)", color: "#01f7a7", display: "flex", alignItems: "center", justifyContent: "center" }}>
           <IoShieldCheckmarkOutline size="14px" />
        </VuiBox>
        <VuiTypography variant="h6" color="white" fontWeight="bold">
          Network Health
        </VuiTypography>
      </VuiBox>
      
      <VuiBox display="flex" flexDirection="column" gap={4} mb={4} flex={1} justifyContent="center">
        <HealthStatItem 
          icon={IoServer} color="#0075ff" 
          title="Active Nodes" value={onlineCount} subtext="Online" 
        />
        <HealthStatItem 
          icon={IoServer} color="#ff285c" 
          title="Offline Nodes" value={offlineCount} subtext="Offline" 
        />
        <HealthStatItem 
          icon={IoTimeOutline} color="#01f7a7" 
          title="Uptime (System)" value={uptimeStr} subtext="Since last restart" 
        />
        <HealthStatItem 
          icon={IoSwapVertical} color="#8a2cff" 
          title="Data Transferred" value="N/A" subtext="Not tracked per node" 
        />
      </VuiBox>
      
      <VuiBox mt="auto">
        <Button variant="outlined" fullWidth sx={{ 
          borderColor: "rgba(255,255,255,0.05) !important", 
          color: "#fff !important", 
          borderRadius: "8px !important",
          py: 1,
          "&:hover": { background: "rgba(255,255,255,0.02) !important" }
        }}>
          View detailed analytics <IoChevronDown style={{ marginLeft: 8, transform: "rotate(-90deg)" }} />
        </Button>
      </VuiBox>
    </Card>
  );
}

function Performance() {
  const { throughput, nodes, connected, gatewayStatus } = useDashboardRealtime();
  
  const systemPacketLoss = useMemo(() => {
    const onlineNodes = nodes.filter((n) => n.online && typeof n.packetLoss === "number");
    if (onlineNodes.length === 0) return "-";
    const sum = onlineNodes.reduce((acc, n) => acc + n.packetLoss, 0);
    return `${(sum / onlineNodes.length).toFixed(2)}%`;
  }, [nodes]);

  // Node selection and filtering state
  const wsUrl = getWebSocketUrl();
  const [nodeIp, setNodeIp] = useState("");
  // Default timeframe: last 1 hour
  const [fromLocal, setFromLocal] = useState(() => toDateTimeLocalValue(Date.now() - 60 * 60_000));
  const [toLocal, setToLocal] = useState(() => toDateTimeLocalValue(Date.now()));
  const [ips, setIps] = useState([]);
  
  const combinedIps = useMemo(() => {
    const map = new Map();
    ips.forEach(n => map.set(n.ip, n));
    nodes.forEach(n => {
      const ip = n.ip || n.id;
      if (ip && !map.has(ip)) map.set(ip, { ip });
    });
    return Array.from(map.values());
  }, [ips, nodes]);

  // Historical data
  const [historySamples, setHistorySamples] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [retryCnt, setRetryCnt] = useState(0);

  const wsRef = useRef(null);
  const fromInputRef = useRef(null);
  const toInputRef = useRef(null);
  const requestSeqRef = useRef(1);
  const seriesReqIdRef = useRef(null);
  const metaReqIdRef = useRef(null);

  useEffect(() => {
    if (!nodeIp && combinedIps.length > 0) {
      setNodeIp(combinedIps[0].ip);
    }
  }, [combinedIps, nodeIp]);

  // Real-time appended points
  const [realtimePoints, setRealtimePoints] = useState([]);

  // When node changes or filter changes, we reset realtimePoints
  useEffect(() => {
    setRealtimePoints([]);
  }, [nodeIp, fromLocal, toLocal]);

  // Append new data from useDashboardRealtime to realtimePoints for selected node
  useEffect(() => {
    if (!nodeIp || nodes.length === 0) return;
    const n = nodes.find((node) => node.ip === nodeIp || node.id === nodeIp);
    if (n && typeof n.packetLoss === "number") {
      const ts = n.lastSeenIso ? new Date(n.lastSeenIso).getTime() : Date.now();
      
      setRealtimePoints(prev => {
        const last = prev.length > 0 ? prev[prev.length - 1] : null;
        if (!last || ts > last.x) {
          return [...prev, { x: ts, y: n.packetLoss }].slice(-3600); // limit local array
        }
        return prev;
      });
    }
  }, [nodes, nodeIp]);


  useEffect(() => {
    if (!wsUrl) return;
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
        metaTimer = setInterval(requestMeta, 10000);
      };
      wsRef.current.onmessage = (ev) => {
        if (typeof ev.data !== "string") return;
        let msg;
        try {
          msg = JSON.parse(ev.data);
        } catch { return; }

        if (msg.type === "history_meta_response") {
          if (msg.requestId && msg.requestId !== metaReqIdRef.current) return;
          if (!msg.error) {
            const list = (msg.ips || []).map(r => ({ ip: String(r?.ip || "").trim() })).filter(r => r.ip);
            setIps(list);
            setNodeIp((prev) => prev || list[0]?.ip || "");
          }
          return;
        }

        if (msg.type === "history_series_response") {
          if (msg.requestId && msg.requestId !== seriesReqIdRef.current) return;
          if (msg.error) {
            setError(String(msg.error));
            setLoading(false);
            return;
          }
          setHistorySamples(Array.isArray(msg.items) ? msg.items : []);
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
      if (wsRef.current) wsRef.current.close();
    };
  }, [wsUrl]);

  // Request historical series when filters change
  useEffect(() => {
    if (!nodeIp) {
      setHistorySamples([]);
      return;
    }
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) {
      setError("Waiting for connection...");
      return;
    }
    setLoading(true);
    setError("");
    const from = fromLocal ? new Date(fromLocal).toISOString() : new Date(Date.now() - 3600000).toISOString();
    const to = toLocal ? new Date(toLocal).toISOString() : new Date().toISOString();
    
    const requestId = `series-${requestSeqRef.current++}`;
    seriesReqIdRef.current = requestId;
    wsRef.current.send(
      JSON.stringify({
        type: "history_series_request",
        requestId,
        ip: nodeIp,
        field: "packetloss", 
        from,
        to,
        limit: 3000,
      })
    );
  }, [nodeIp, fromLocal, toLocal, retryCnt, connected]);

  const chartPoints = useMemo(() => {
    const hist = historySamples
      .map((s) => ({ x: new Date(s.time).getTime(), y: Number(s.value) }))
      .filter((p) => Number.isFinite(p.x) && Number.isFinite(p.y));
    
    const map = new Map();
    hist.forEach(p => map.set(p.x, p));
    realtimePoints.forEach(p => map.set(p.x, p));
    
    return Array.from(map.values()).sort((a, b) => a.x - b.x);
  }, [historySamples, realtimePoints]);

  const chartData = useMemo(() => [
    {
      name: "Packet Loss (%)",
      data: chartPoints,
    },
  ], [chartPoints]);

  const chartOptions = useMemo(() => ({
    ...lineChartOptionsDashboard,
    chart: {
      type: "area",
      toolbar: { show: false },
      background: "transparent",
    },
    stroke: {
      curve: "smooth",
      width: 2,
    },
    fill: {
      type: "gradient",
      gradient: {
        shadeIntensity: 1,
        opacityFrom: 0.45,
        opacityTo: 0.05,
        stops: [0, 90, 100]
      }
    },
    colors: ["#0075FF"],
    xaxis: {
      ...lineChartOptionsDashboard.xaxis,
      type: "datetime",
      categories: [],
      labels: {
        ...lineChartOptionsDashboard.xaxis.labels,
        formatter: (val) => {
          if (!val) return "";
          const d = new Date(val);
          return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`;
        }
      }
    },
    yaxis: {
      ...lineChartOptionsDashboard.yaxis,
      title: {
        text: "Packet Loss (%)",
        style: { color: "#c8cfca", fontSize: "11px" },
      },
      min: 0,
    },
  }), []);

  const setQuickRange = (hours) => {
    const now = Date.now();
    setToLocal(toDateTimeLocalValue(now));
    setFromLocal(toDateTimeLocalValue(now - hours * 3600000));
  };

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <VuiBox mb={3}>
          <Grid container spacing={3}>
            <Grid item xs={12} md={4}>
              <PerformanceCard 
                title="System Throughput (Current)" 
                value={`${throughput.currentBytesPerSec}`} 
                detail="B/s" 
                icon={<IoWifi size="24px" />} 
                color="#0075ff"
                trend={12.5}
                trendLabel="vs last hour"
              />
            </Grid>
            <Grid item xs={12} md={4}>
              <PerformanceCard 
                title="System Throughput (Avg)" 
                value={`${throughput.averageBytesPerSec}`} 
                detail="B/s" 
                icon={<IoAnalytics size="24px" />} 
                color="#8a2cff"
                trend={-8.3}
                trendLabel="vs last hour"
              />
            </Grid>
            <Grid item xs={12} md={4}>
              <PerformanceCard 
                title="Overall Packet Loss" 
                value={systemPacketLoss} 
                icon={<IoPulse size="24px" />} 
                color="#ff285c"
                trendLabel="Estimated"
              />
            </Grid>
          </Grid>
        </VuiBox>
        
        <VuiBox mb={3}>
          <Card sx={glassCardSx}>
            <VuiBox p={3}>
              <VuiBox display="flex" alignItems="center" gap={1.25} mb={2}>
                <VuiBox
                  sx={{
                    width: 32, height: 32, borderRadius: "8px", display: "grid", placeItems: "center",
                    color: "#fff", background: `radial-gradient(circle, #01f7a7 0%, rgba(0,117,255,0.35) 100%)`,
                    boxShadow: `0 0 12px #01f7a788`,
                  }}
                >
                  <IoHardwareChip size="16px" />
                </VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold">
                  Node Performance
                </VuiTypography>
              </VuiBox>

              <VuiBox display="flex" alignItems="center" justifyContent="space-between" flexWrap="wrap" gap={2} mb={3}>
                <VuiBox display="flex" gap={2} flexWrap="wrap" flex={1}>
                  <FormControl variant="outlined" sx={{ ...filterSx, minWidth: 200 }}>
                    <InputLabel id="history-ip-label" shrink>
                      Select Node
                    </InputLabel>
                    <Select
                      labelId="history-ip-label"
                      label="Select Node"
                      value={nodeIp}
                      onChange={(e) => setNodeIp(String(e.target.value || "").trim())}
                      MenuProps={selectMenuProps}
                    >
                      {combinedIps.length === 0 ? (
                        <MenuItem value="" disabled>No Nodes found</MenuItem>
                      ) : null}
                      {combinedIps.map((n) => (
                        <MenuItem key={n.ip} value={n.ip}>{n.ip}</MenuItem>
                      ))}
                    </Select>
                  </FormControl>

                  <TextField
                    type="datetime-local"
                    label="From time"
                    value={fromLocal}
                    onChange={(e) => setFromLocal(e.target.value)}
                    inputRef={fromInputRef}
                    onClick={() => openNativeDateTimePicker(fromInputRef.current)}
                    onFocus={() => openNativeDateTimePicker(fromInputRef.current)}
                    InputLabelProps={{ shrink: true }}
                    inputProps={{ step: 60 }}
                    sx={{ ...filterSx, width: 200 }}
                    InputProps={{
                      endAdornment: (
                        <InputAdornment position="end" sx={{ pointerEvents: "none" }}>
                          <IoCalendarOutline color="rgba(255,255,255,0.4)" size="16px" />
                        </InputAdornment>
                      ),
                    }}
                  />

                  <TextField
                    type="datetime-local"
                    label="To time"
                    value={toLocal}
                    onChange={(e) => setToLocal(e.target.value)}
                    inputRef={toInputRef}
                    onClick={() => openNativeDateTimePicker(toInputRef.current)}
                    onFocus={() => openNativeDateTimePicker(toInputRef.current)}
                    InputLabelProps={{ shrink: true }}
                    inputProps={{ step: 60 }}
                    sx={{ ...filterSx, width: 200 }}
                    InputProps={{
                      endAdornment: (
                        <InputAdornment position="end" sx={{ pointerEvents: "none" }}>
                          <IoCalendarOutline color="rgba(255,255,255,0.4)" size="16px" />
                        </InputAdornment>
                      ),
                    }}
                  />
                </VuiBox>

                <VuiBox display="flex" gap={1}>
                  <Button onClick={() => setQuickRange(1)} sx={{ background: "rgba(255,255,255,0.05)", color: "#fff", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "8px !important", minWidth: "48px", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>1H</Button>
                  <Button onClick={() => setQuickRange(6)} sx={{ background: "rgba(255,255,255,0.05)", color: "#fff", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "8px !important", minWidth: "48px", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>6H</Button>
                  <Button onClick={() => setQuickRange(12)} sx={{ background: "rgba(255,255,255,0.05)", color: "#fff", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "8px !important", minWidth: "48px", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>12H</Button>
                  <Button onClick={() => setQuickRange(24)} sx={{ background: "rgba(255,255,255,0.05)", color: "#fff", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "8px !important", minWidth: "48px", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>24H</Button>
                </VuiBox>
              </VuiBox>

              {error && !loading ? (
                <VuiBox sx={{ background: "rgba(255,40,92,0.05)", border: "1px solid rgba(255,40,92,0.15)", borderRadius: "12px", p: 2, display: "flex", justifyContent: "space-between", alignItems: "center", mb: 3 }}>
                  <VuiTypography variant="caption" color="error">Failed to fetch data</VuiTypography>
                  <Button variant="outlined" color="info" size="small" onClick={() => setRetryCnt((c) => c + 1)}>Retry</Button>
                </VuiBox>
              ) : null}

              {loading ? (
                <VuiTypography variant="caption" color="text" display="block" mb={1}>Loading chart data...</VuiTypography>
              ) : null}

              <VuiBox sx={{ height: "350px", mt: 1 }}>
                <ReactApexChart options={chartOptions} series={chartData} type="area" height="100%" />
              </VuiBox>
            </VuiBox>
          </Card>
        </VuiBox>

        <Grid container spacing={3}>
          <Grid item xs={12} md={7}>
            <TopNodesTable nodes={nodes} />
          </Grid>
          <Grid item xs={12} md={5}>
            <NetworkHealthCard nodes={nodes} gatewayStatus={gatewayStatus} />
          </Grid>
        </Grid>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Performance;
