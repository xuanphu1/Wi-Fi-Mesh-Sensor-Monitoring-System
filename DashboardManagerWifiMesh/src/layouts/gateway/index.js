import React, { useState } from "react";
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import Dialog from "@mui/material/Dialog";
import DialogTitle from "@mui/material/DialogTitle";
import DialogContent from "@mui/material/DialogContent";
import DialogActions from "@mui/material/DialogActions";
import TextField from "@mui/material/TextField";
import Chip from "@mui/material/Chip";

import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useMeshRealtime } from "context/meshRealtime";

import {
  IoWifi,
  IoServer,
  IoCellular,
  IoTime,
  IoBatteryFull,
  IoHardwareChip,
  IoCalendar,
  IoChevronDown,
  IoExpand,
  IoLocationOutline,
  IoGitNetworkOutline,
  IoCreateOutline,
  IoCheckmarkCircle,
  IoLayersOutline,
  IoTrashOutline,
} from "react-icons/io5";

const panelSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.10)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
  borderRadius: "16px",
  backdropFilter: "blur(18px)",
};

const iconBoxSx = {
  width: "48px",
  height: "48px",
  borderRadius: "12px",
  display: "flex",
  alignItems: "center",
  justifyContent: "center",
  background: "rgba(255, 255, 255, 0.05)",
  border: "1px solid rgba(255, 255, 255, 0.10)",
};

function fmtInt(value, suffix = "") {
  if (typeof value !== "number" || !Number.isFinite(value)) return "-";
  return `${Math.round(value)}${suffix}`;
}

function fmtNumber(value, digits = 1, suffix = "") {
  if (typeof value !== "number" || !Number.isFinite(value)) return "-";
  return `${value.toFixed(digits)}${suffix}`;
}

function fmtUptime(seconds) {
  if (typeof seconds !== "number" || !Number.isFinite(seconds)) return "-";
  const s = Math.max(0, Math.floor(seconds));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  if (h > 0) return `${h}h ${m}m ${sec}s`;
  if (m > 0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

function MetricCardRight({ label, value, accent = "white", icon, statusDot }) {
  return (
    <Card sx={panelSx}>
      <VuiBox p={2} display="flex" justifyContent="space-between" alignItems="center">
        <VuiBox>
          <VuiBox display="flex" alignItems="center" gap={1} mb={0.5}>
            <VuiTypography variant="caption" color="text" display="block">
              {label}
            </VuiTypography>
            {statusDot && (
              <VuiBox width="8px" height="8px" borderRadius="50%" bgColor={statusDot} sx={{ boxShadow: `0 0 8px ${statusDot}` }} />
            )}
          </VuiBox>
          <VuiTypography variant="h6" color={accent} fontWeight="bold">
            {value || "-"}
          </VuiTypography>
        </VuiBox>
        <VuiBox sx={iconBoxSx}>
          {icon}
        </VuiBox>
      </VuiBox>
    </Card>
  );
}

function MetricCardLeft({ title, value, subtitle, progress, icon }) {
  return (
    <Card sx={panelSx}>
      <VuiBox p={2} display="flex" alignItems="center" gap={2}>
        <VuiBox sx={{ ...iconBoxSx, background: "rgba(97, 66, 255, 0.18)", border: "1px solid rgba(97, 66, 255, 0.40)", color: "#b8a9ff", flexShrink: 0 }}>
          {icon}
        </VuiBox>
        <VuiBox display="flex" flexDirection="column" width="100%">
          <VuiTypography variant="caption" color="text" display="block">
            {title}
          </VuiTypography>
          <VuiTypography variant="button" color="white" fontWeight="bold">
            {value}
          </VuiTypography>
          {subtitle && (
            <VuiTypography variant="caption" color="info" display="block">
              {subtitle}
            </VuiTypography>
          )}
          {progress !== undefined && (
            <VuiBox mt={1} width="100%" height="6px" borderRadius="3px" bgColor="rgba(255,255,255,0.1)">
              <VuiBox height="100%" width={`${progress}%`} borderRadius="3px" sx={{ background: "linear-gradient(90deg, #4F38DF 0%, #00d2ff 100%)", boxShadow: "0 0 10px rgba(0,210,255,0.5)" }} />
            </VuiBox>
          )}
        </VuiBox>
      </VuiBox>
    </Card>
  );
}

function CircularGauge({ title, value, color }) {
  const safeValue =
    typeof value === "number" && Number.isFinite(value)
      ? Math.max(0, Math.min(100, value))
      : 0;
  const label = typeof value === "number" && Number.isFinite(value) ? Math.round(value) : "-";

  return (
    <Card sx={{ ...panelSx, height: "100%", position: "relative" }}>
      <VuiBox position="absolute" top={16} right={16}>
         <IoHardwareChip size="16px" color="rgba(255,255,255,0.5)" />
      </VuiBox>
      <VuiBox p={2.5} display="flex" flexDirection="column" alignItems="center" justifyContent="center">
        <VuiBox display="flex" justifyContent="space-between" width="100%" mb={2.5}>
           <VuiTypography variant="caption" color="text">
             {title}
           </VuiTypography>
        </VuiBox>
        <VuiBox
          sx={{
            width: 132,
            height: 132,
            borderRadius: "50%",
            background: `conic-gradient(${color} ${safeValue * 3.6}deg, rgba(255,255,255,0.05) 0deg)`,
            display: "grid",
            placeItems: "center",
            position: "relative",
            boxShadow: `0 0 18px ${color}66, 0 0 42px ${color}3d, 0 0 80px ${color}24`,
            "&::before": {
              content: '""',
              position: "absolute",
              inset: "-18px",
              borderRadius: "50%",
              background: `radial-gradient(circle, ${color}33 0%, ${color}18 34%, transparent 72%)`,
              filter: "blur(10px)",
              zIndex: 0,
            },
            "&::after": {
              content: '""',
              position: "absolute",
              inset: "-4px",
              borderRadius: "50%",
              border: `1px solid ${color}55`,
              boxShadow: `inset 0 0 16px ${color}44`,
              zIndex: 1,
            },
          }}
        >
          <VuiBox
            sx={{
              width: 100,
              height: 100,
              borderRadius: "50%",
              background: "linear-gradient(127deg, rgba(6, 11, 40, 0.98) 0%, rgba(10, 14, 35, 0.98) 100%)",
              display: "grid",
              placeItems: "center",
              border: "1px solid rgba(255,255,255,0.08)",
              boxShadow: "inset 0 0 18px rgba(0,0,0,0.45)",
              position: "relative",
              zIndex: 2,
            }}
          >
            <VuiTypography variant="h4" color="white" fontWeight="bold">
              {label}%
            </VuiTypography>
          </VuiBox>
        </VuiBox>
        <VuiTypography variant="button" color="text" fontWeight="bold" mt={2.5}>
          {label} %
        </VuiTypography>
      </VuiBox>
    </Card>
  );
}

function buildPolyline(series, key, minY, maxY, width, height, pad) {
  const values = series.map((p, idx) => ({ idx, value: p[key] })).filter((p) => typeof p.value === "number" && Number.isFinite(p.value));
  if (values.length === 0) return "";
  const spanX = Math.max(1, series.length - 1);
  const spanY = Math.max(1, maxY - minY);
  return values
    .map(({ idx, value }) => {
      const x = pad.l + (idx / spanX) * (width - pad.l - pad.r);
      const y = pad.t + (1 - (value - minY) / spanY) * (height - pad.t - pad.b);
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(" ");
}

function LineChartPanel({ title, subtitle, series, lines, minY, maxY, yLabels, height = 210, note, xStartLabel, xEndLabel, horizontalLayout = false }) {
  const width = horizontalLayout ? 1200 : 760;
  const pad = { l: horizontalLayout ? 0 : 54, r: 18, t: 40, b: 32 };
  const rows = yLabels && yLabels.length ? yLabels : [maxY, (maxY + minY) / 2, minY];

  const svgContent = (
    <svg viewBox={`0 0 ${width} ${height}`} width="100%" height={height} role="img" style={{ overflow: "visible" }}>
      {rows.map((v) => {
        const y = pad.t + (1 - (v - minY) / Math.max(1, maxY - minY)) * (height - pad.t - pad.b);
        return (
          <g key={v}>
            <line x1={pad.l + 30} y1={y} x2={width - pad.r} y2={y} stroke="rgba(148,163,184,0.15)" strokeWidth="1" />
            <text x={pad.l + 22} y={y + 4} fill="#8aa0bb" fontSize="11" textAnchor="end">
              {v}
            </text>
          </g>
        );
      })}
      {lines.map((line) => (
        <polyline
          key={line.key}
          fill="none"
          points={buildPolyline(series, line.key, minY, maxY, width, height, { ...pad, l: pad.l + 30 })}
          stroke={line.color}
          strokeWidth="2.5"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
      ))}
      <text x={pad.l + 30} y={height - 8} fill="#8aa0bb" fontSize="11">
        {xStartLabel}
      </text>
      <text x={width - pad.r} y={height - 8} fill="#8aa0bb" fontSize="11" textAnchor="end">
        {xEndLabel}
      </text>
    </svg>
  );

  if (horizontalLayout) {
    return (
      <Card sx={{ ...panelSx, height: "100%", position: "relative", minHeight: "300px" }}>
        <VuiBox position="absolute" top={16} right={16}>
           <IoExpand size="16px" color="rgba(255,255,255,0.5)" />
        </VuiBox>
        <VuiBox p={3} display="flex" height="100%">
          <VuiBox width={{ xs: "100%", md: "25%" }} display="flex" flexDirection="column" justifyContent="space-between" pr={2}>
            <VuiBox>
              <VuiTypography variant="caption" color="text" display="block" mb={0}>
                {title}
              </VuiTypography>
              {subtitle && (
                <VuiTypography variant="caption" color="text" display="block" sx={{ opacity: 0.7 }}>
                  {subtitle}
                </VuiTypography>
              )}
            </VuiBox>
            {note && (
              <VuiTypography variant="caption" color="text" display="block" sx={{ opacity: 0.5, fontSize: "0.65rem", paddingRight: "20px" }}>
                {note}
              </VuiTypography>
            )}
          </VuiBox>
          <VuiBox width={{ xs: "100%", md: "75%" }} display="flex" alignItems="center">
             {svgContent}
          </VuiBox>
        </VuiBox>
      </Card>
    );
  }

  return (
    <Card sx={{ ...panelSx, height: "100%", position: "relative" }}>
      <VuiBox position="absolute" top={16} right={16}>
         <IoExpand size="16px" color="rgba(255,255,255,0.5)" />
      </VuiBox>
      <VuiBox p={2}>
        <VuiTypography variant="caption" color="text" display="block" mb={0}>
          {title}
        </VuiTypography>
        {subtitle && (
          <VuiTypography variant="caption" color="text" display="block" mb={1} sx={{ opacity: 0.7 }}>
            {subtitle}
          </VuiTypography>
        )}
        {svgContent}
        {lines.length > 1 ? (
          <VuiBox display="flex" justifyContent="center" gap={3} mt={0.5}>
            {lines.map((line) => (
              <VuiTypography key={line.key} variant="caption" color="text" fontWeight="bold">
                <span style={{ display: "inline-block", width: 8, height: 8, background: line.color, marginRight: 6 }} />
                {line.label}
              </VuiTypography>
            ))}
          </VuiBox>
        ) : null}
        {note ? (
          <VuiTypography variant="caption" color="text" display="block" mt={1} sx={{ opacity: 0.7, maxWidth: "300px" }}>
            {note}
          </VuiTypography>
        ) : null}
      </VuiBox>
    </Card>
  );
}

function GatewayEventLogs({ logs }) {
  return (
    <Card sx={panelSx}>
      <VuiBox px={2} py={2} display="flex" justifyContent="space-between" alignItems="center" borderBottom="1px solid rgba(255,255,255,0.05)">
        <VuiTypography variant="button" color="white" fontWeight="bold">
          Gateway Event Logs
        </VuiTypography>
        <VuiBox display="flex" gap={1}>
           <VuiBox px={2} py={0.5} borderRadius="8px" border="1px solid rgba(255,255,255,0.1)" display="flex" alignItems="center" gap={1} sx={{ cursor: "pointer" }}>
             <VuiTypography variant="caption" color="white">All Levels</VuiTypography>
             <IoChevronDown color="white" size="12px" />
           </VuiBox>
           <VuiBox width="32px" height="32px" borderRadius="8px" border="1px solid rgba(255,255,255,0.1)" display="flex" alignItems="center" justifyContent="center" sx={{ cursor: "pointer" }}>
             <IoCalendar color="white" size="14px" />
           </VuiBox>
        </VuiBox>
      </VuiBox>
      <VuiBox
        component="table"
        sx={{
          width: "100%",
          borderCollapse: "collapse",
          "& th": {
            color: "#ffffff",
            fontSize: "0.75rem",
            fontWeight: 700,
            textAlign: "left",
            padding: "16px",
            borderBottom: "1px solid rgba(255,255,255,0.05)",
          },
          "& td": {
            color: "#ffffff",
            fontSize: "0.75rem",
            padding: "16px",
            borderBottom: "1px solid rgba(255,255,255,0.05)",
          },
          "& tbody tr:hover": {
            background: "rgba(255,255,255,0.02)",
          },
        }}
      >
        <thead>
          <tr>
            <th>Time</th>
            <th>Level</th>
            <th>Message</th>
          </tr>
        </thead>
        <tbody>
          {logs.length === 0 ? (
            <tr>
              <td colSpan={3}>No events yet.</td>
            </tr>
          ) : (
            logs.slice(0, 7).map((log) => (
              <tr key={log.id}>
                <td>{new Date(log.timeIso).toLocaleTimeString()}</td>
                <td>
                  <span style={{ background: "rgba(56, 189, 248, 0.1)", color: "#38bdf8", borderRadius: 4, padding: "4px 8px", fontWeight: 600 }}>
                    {log.level}
                  </span>
                </td>
                <td style={{ color: "rgba(255,255,255,0.7)" }}>{log.message}</td>
              </tr>
            ))
          )}
        </tbody>
      </VuiBox>
      <VuiBox px={2} py={2}>
        <VuiTypography variant="caption" color="text">
          Showing 1 to {Math.min(7, logs.length)} of {logs.length} logs
        </VuiTypography>
      </VuiBox>
    </Card>
  );
}

function Gateway() {
  const {
    gatewayStatus,
    gatewaySeries,
    gatewayLogs,
    wsOpen,
    sendTimeSync,
    gateways = [],
    selectedGatewayId = "all",
    setSelectedGatewayId,
    updateGatewayMeta,
    deleteGateway,
    nodes = [],
  } = useMeshRealtime() || {};

  const [syncFeedback, setSyncFeedback] = useState(null);
  const [editGw, setEditGw] = useState(null);
  const [editName, setEditName] = useState("");
  const [editLocation, setEditLocation] = useState("");
  const [editLatitude, setEditLatitude] = useState("");
  const [editLongitude, setEditLongitude] = useState("");
  const [editNotes, setEditNotes] = useState("");
  const [isSaving, setIsSaving] = useState(false);
  const [confirmDeleteGw, setConfirmDeleteGw] = useState(null);
  const [isDeletingGw, setIsDeletingGw] = useState(false);

  const selectedGateway = gateways.find((gw) => gw.gw_id === selectedGatewayId) || null;
  const hasSelectedGateway = Boolean(selectedGatewayId && selectedGatewayId !== "all");

  const g = gatewayStatus;
  const series = gatewaySeries || [];

  const handleSyncTime = () => {
    const ok = sendTimeSync && sendTimeSync();
    if (ok) {
      setSyncFeedback("Sent time sync command to Gateway!");
      setTimeout(() => setSyncFeedback(null), 3500);
    } else {
      setSyncFeedback("Failed: WebSocket is disconnected.");
      setTimeout(() => setSyncFeedback(null), 3500);
    }
  };

  const handleOpenEdit = (gw) => {
    setEditGw(gw);
    setEditName(gw.name || gw.gw_id);
    setEditLocation(gw.location || "");
    setEditLatitude(gw.latitude != null ? String(gw.latitude) : "");
    setEditLongitude(gw.longitude != null ? String(gw.longitude) : "");
    setEditNotes(gw.notes || "");
  };

  const handleSaveEdit = async () => {
    if (!editGw || !updateGatewayMeta) return;
    setIsSaving(true);
    await updateGatewayMeta(editGw.gw_id, {
      name: editName,
      location: editLocation,
      latitude: editLatitude !== "" ? Number(editLatitude) : null,
      longitude: editLongitude !== "" ? Number(editLongitude) : null,
      notes: editNotes,
    });
    setIsSaving(false);
    setEditGw(null);
  };

  const totalGateways = gateways.length;
  const onlineGateways = gateways.filter((gw) => gw.status === "online").length;
  const uniqueMeshes = new Set(gateways.map((gw) => gw.mesh_id || "mesh-default")).size;
  const totalNodes = nodes.length;

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        {/* Multi-Gateway Hub Overview Header */}
        <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={2} flexWrap="wrap" gap={1.5}>
          <VuiBox>
            <VuiTypography variant="h4" color="white" fontWeight="bold">
              Multi-Gateway & Multi-Mesh Hub
            </VuiTypography>
            <VuiTypography variant="caption" color="text">
              Quản lý phân tầng các trạm Gateway, mạng Mesh khu vực và danh sách Node cảm biến trực thuộc
            </VuiTypography>
          </VuiBox>
          <VuiBox display="flex" alignItems="center" gap={1.5}>
            {syncFeedback && (
              <VuiTypography variant="caption" color={syncFeedback.startsWith("Sent") ? "success" : "warning"} fontWeight="bold">
                {syncFeedback}
              </VuiTypography>
            )}
            <Button
              variant="contained"
              size="small"
              onClick={handleSyncTime}
              disabled={!wsOpen}
              sx={{
                background: "linear-gradient(135deg, #0075ff 0%, #00d2ff 100%) !important",
                color: "#fff !important",
                fontWeight: "bold",
                borderRadius: "10px",
                px: 2.5,
                py: 1,
                textTransform: "none",
                fontSize: "13px",
                boxShadow: "0 0 16px rgba(0, 117, 255, 0.4)",
              }}
            >
              <IoTime size="16px" style={{ marginRight: 6 }} /> Sync RTC Time
            </Button>
          </VuiBox>
        </VuiBox>

        {/* Overview Stats for Multi-Gateway */}
        <Grid container spacing={2.5} mb={3}>
          <Grid item xs={12} sm={6} md={3}>
            <Card sx={panelSx}>
              <VuiBox p={2.5} display="flex" justifyContent="space-between" alignItems="center">
                <VuiBox>
                  <VuiTypography variant="caption" color="text">TỔNG SỐ GATEWAY</VuiTypography>
                  <VuiTypography variant="h4" color="white" fontWeight="bold">{totalGateways}</VuiTypography>
                </VuiBox>
                <VuiBox sx={{ ...iconBoxSx, background: "rgba(0, 212, 255, 0.15)", color: "#00d4ff" }}>
                  <IoServer size="24px" />
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
          <Grid item xs={12} sm={6} md={3}>
            <Card sx={panelSx}>
              <VuiBox p={2.5} display="flex" justifyContent="space-between" alignItems="center">
                <VuiBox>
                  <VuiTypography variant="caption" color="text">GATEWAY ONLINE</VuiTypography>
                  <VuiTypography variant="h4" color="#01f7a7" fontWeight="bold">{onlineGateways} / {totalGateways}</VuiTypography>
                </VuiBox>
                <VuiBox sx={{ ...iconBoxSx, background: "rgba(1, 247, 167, 0.15)", color: "#01f7a7" }}>
                  <IoWifi size="24px" />
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
          <Grid item xs={12} sm={6} md={3}>
            <Card sx={panelSx}>
              <VuiBox p={2.5} display="flex" justifyContent="space-between" alignItems="center">
                <VuiBox>
                  <VuiTypography variant="caption" color="text">MẠNG MESH HOẠT ĐỘNG</VuiTypography>
                  <VuiTypography variant="h4" color="#b8a9ff" fontWeight="bold">{uniqueMeshes}</VuiTypography>
                </VuiBox>
                <VuiBox sx={{ ...iconBoxSx, background: "rgba(138, 44, 255, 0.18)", color: "#b8a9ff" }}>
                  <IoGitNetworkOutline size="24px" />
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
          <Grid item xs={12} sm={6} md={3}>
            <Card sx={panelSx}>
              <VuiBox p={2.5} display="flex" justifyContent="space-between" alignItems="center">
                <VuiBox>
                  <VuiTypography variant="caption" color="text">TỔNG SỐ NODES</VuiTypography>
                  <VuiTypography variant="h4" color="#ffb547" fontWeight="bold">{totalNodes}</VuiTypography>
                </VuiBox>
                <VuiBox sx={{ ...iconBoxSx, background: "rgba(255, 181, 71, 0.15)", color: "#ffb547" }}>
                  <IoHardwareChip size="24px" />
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
        </Grid>

        {/* Gateway Cards Grid */}
        <VuiTypography variant="button" color="white" fontWeight="bold" display="block" mb={1.5}>
          DANH SÁCH CÁC TRẠM GATEWAY & VÙNG QUẢN LÝ
        </VuiTypography>
        <Grid container spacing={2.5} mb={3.5}>
          {gateways.map((gw) => {
            const isSelected = selectedGatewayId === gw.gw_id;
            const isOnline = gw.status === "online";
            const gwNodes = nodes.filter((n) => (n.gw_id || "gw-default") === gw.gw_id);

            return (
              <Grid item xs={12} md={6} lg={4} key={gw.gw_id}>
                <Card
                  sx={{
                    ...panelSx,
                    p: 2.5,
                    border: isSelected ? "2px solid #00d4ff" : "1px solid rgba(255, 255, 255, 0.12)",
                    boxShadow: isSelected ? "0 0 24px rgba(0, 212, 255, 0.25)" : "0 8px 32px rgba(0,0,0,0.35)",
                    transition: "all 0.25s ease",
                  }}
                >
                  <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start" mb={1.5}>
                    <VuiBox display="flex" alignItems="center" gap={1.5}>
                      <VuiBox
                        sx={{
                          width: 42,
                          height: 42,
                          borderRadius: "12px",
                          background: isOnline ? "rgba(1, 247, 167, 0.15)" : "rgba(255, 82, 82, 0.15)",
                          display: "grid",
                          placeItems: "center",
                          color: isOnline ? "#01f7a7" : "#ff5252",
                          border: `1px solid ${isOnline ? "rgba(1, 247, 167, 0.35)" : "rgba(255, 82, 82, 0.35)"}`,
                        }}
                      >
                        <IoServer size="20px" />
                      </VuiBox>
                      <VuiBox>
                        <VuiTypography variant="h6" color="white" fontWeight="bold">
                          {gw.name || gw.gw_id}
                        </VuiTypography>
                        <VuiTypography variant="caption" color="text" sx={{ fontFamily: "monospace" }}>
                          ID: {gw.mac || gw.root_mac || (gw.gw_id === "gw-default" && gatewayStatus?.mac ? gatewayStatus.mac : (gw.gw_id || "—"))}
                        </VuiTypography>
                      </VuiBox>
                    </VuiBox>
                    <Chip
                      size="small"
                      label={isOnline ? "ONLINE" : "OFFLINE"}
                      sx={{
                        backgroundColor: isOnline ? "rgba(1, 247, 167, 0.2)" : "rgba(255, 82, 82, 0.2)",
                        color: isOnline ? "#01f7a7" : "#ff5252",
                        fontWeight: "bold",
                        fontSize: "11px",
                      }}
                    />
                  </VuiBox>

                  {/* Metadata fields */}
                  <VuiBox display="flex" flexDirection="column" gap={0.8} mb={2}>
                    <VuiBox display="flex" alignItems="center">
                      <VuiTypography variant="caption" color="white">
                        Vị trí: <b>{gw.location || "Chưa thiết lập"}</b>
                        {gw.latitude != null && gw.longitude != null && (
                          <span style={{ color: "rgba(255,255,255,0.6)", marginLeft: 6, fontSize: "11px" }}>
                            ({Number(gw.latitude).toFixed(4)}, {Number(gw.longitude).toFixed(4)})
                          </span>
                        )}
                      </VuiTypography>
                    </VuiBox>
                    <VuiBox display="flex" alignItems="center">
                      <VuiTypography variant="caption" color="white">
                        Mạng Mesh: <b>{gw.mesh_id || "mesh-default"}</b>
                      </VuiTypography>
                    </VuiBox>
                    <VuiBox display="flex" alignItems="center">
                      <VuiTypography variant="caption" color="white">
                        Số Nodes trực thuộc: <b style={{ color: "#01f7a7" }}>{gwNodes.length || gw.node_count || 0} nodes</b>
                      </VuiTypography>
                    </VuiBox>
                  </VuiBox>

                  {/* Action buttons */}
                  <VuiBox display="flex" gap={1} mt="auto">
                    <Button
                      fullWidth
                      size="small"
                      variant={isSelected ? "contained" : "outlined"}
                      onClick={() => setSelectedGatewayId && setSelectedGatewayId(isSelected ? "all" : gw.gw_id)}
                      sx={{
                        textTransform: "none",
                        fontSize: "12px",
                        fontWeight: "bold",
                        borderRadius: "10px",
                        color: isSelected ? "#060b28 !important" : "#00d4ff !important",
                        borderColor: "#00d4ff",
                        background: isSelected ? "#00d4ff !important" : "transparent",
                      }}
                    >
                      {isSelected ? "Đang chọn xem ✓" : "Chọn xem trạm này"}
                    </Button>
                    <Button
                      size="small"
                      variant="outlined"
                      onClick={() => handleOpenEdit(gw)}
                      sx={{
                        minWidth: "36px",
                        px: 1.2,
                        borderRadius: "10px",
                        borderColor: "rgba(255,255,255,0.2)",
                        color: "rgba(255,255,255,0.8)",
                      }}
                    >
                      <IoCreateOutline size="16px" />
                    </Button>
                    <Button
                      size="small"
                      variant="outlined"
                      onClick={() => setConfirmDeleteGw(gw)}
                      sx={{
                        minWidth: "36px",
                        px: 1.2,
                        borderRadius: "10px",
                        borderColor: "rgba(255, 82, 82, 0.35)",
                        color: "#ff5252",
                        "&:hover": {
                          borderColor: "#ff5252",
                          background: "rgba(255, 82, 82, 0.15)",
                        },
                      }}
                    >
                      <IoTrashOutline size="16px" />
                    </Button>
                  </VuiBox>
                </Card>
              </Grid>
            );
          })}
        </Grid>

        {/* Live Hardware Telemetry Section - Only displayed when a specific Gateway is selected */}
        {!hasSelectedGateway ? (
          <Card
            sx={{
              ...panelSx,
              p: 4,
              textAlign: "center",
              mb: 3,
              border: "1px dashed rgba(255, 255, 255, 0.18)",
            }}
          >
            <VuiBox display="flex" flexDirection="column" alignItems="center" gap={1}>
              <VuiBox
                width="44px"
                height="44px"
                borderRadius="12px"
                display="flex"
                alignItems="center"
                justifyContent="center"
                sx={{ background: "rgba(0, 212, 255, 0.12)", color: "#00d4ff", border: "1px solid rgba(0, 212, 255, 0.25)" }}
              >
                <IoServer size="22px" />
              </VuiBox>
              <VuiTypography variant="h6" color="white" fontWeight="bold">
                Chưa chọn Gateway nào
              </VuiTypography>
              <VuiTypography variant="caption" color="text" maxWidth="520px">
                Nhấp nút <b>"Chọn xem trạm này"</b> trên một Gateway ở danh sách phía trên để mở bảng theo dõi <b>Trạng Thái Phần Cứng & Telemetry (Thời Gian Thực)</b> của trạm đó.
              </VuiTypography>
            </VuiBox>
          </Card>
        ) : (
          <>
            <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={1.5} flexWrap="wrap" gap={1}>
              <VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold">
                  Trạng Thái Phần Cứng & Telemetry (Thời Gian Thực)
                </VuiTypography>
                <VuiTypography variant="caption" color="text">
                  Dữ liệu CPU, RAM, Uptime và sóng RSSI của Gateway đang được chọn:{" "}
                  <b style={{ color: "#00d4ff" }}>
                    {selectedGateway?.name || selectedGatewayId} ({selectedGateway?.mac || selectedGateway?.root_mac || (selectedGatewayId === "gw-default" && gatewayStatus?.mac ? gatewayStatus.mac : selectedGatewayId)})
                  </b>
                </VuiTypography>
              </VuiBox>
              <Button
                size="small"
                variant="outlined"
                onClick={() => setSelectedGatewayId && setSelectedGatewayId("all")}
                sx={{
                  color: "rgba(255,255,255,0.7)",
                  borderColor: "rgba(255,255,255,0.2)",
                  textTransform: "none",
                  borderRadius: "10px",
                }}
              >
                Ẩn bảng Telemetry
              </Button>
            </VuiBox>

            <Grid container spacing={2} mb={2}>
              <Grid item xs={12} md={3}>
                <MetricCardRight 
                  label="Gateway Status" 
                  value={g ? "Receiving gateway_status" : wsOpen ? "Waiting for gateway_status" : "WebSocket disconnected"} 
                  accent={g ? "#4ade80" : "warning"} 
                  icon={<IoWifi size="24px" color="#38bdf8" />}
                  statusDot={g ? "#4ade80" : "transparent"}
                />
              </Grid>
              <Grid item xs={12} md={3}>
                <MetricCardRight 
                  label="MAC Address" 
                  value={g?.mac || g?.staMac || g?.sta_mac || selectedGateway?.mac || selectedGateway?.root_mac || "-"} 
                  icon={<IoServer size="24px" color="#38bdf8" />}
                />
              </Grid>
              <Grid item xs={12} md={3}>
                <MetricCardRight 
                  label="RSSI" 
                  value={fmtInt(g?.wifiRssi, " dBm")} 
                  icon={<IoCellular size="24px" color="#38bdf8" />}
                />
              </Grid>
              <Grid item xs={12} md={3}>
                <MetricCardRight 
                  label="Uptime" 
                  value={fmtUptime(g?.uptimeS)} 
                  icon={<IoTime size="24px" color="#38bdf8" />}
                />
              </Grid>
            </Grid>

            <Grid container spacing={2} mb={2}>
              <Grid item xs={12} lg={4}>
                <CircularGauge title="CPU Load" value={g?.cpuLoadPercent} color="#4F38DF" />
              </Grid>
              <Grid item xs={12} lg={4}>
                <CircularGauge title="RAM Used" value={g?.ramUsedPercent} color="#00d2ff" />
              </Grid>
              <Grid item xs={12} lg={4}>
                <LineChartPanel
                  title="CPU & RAM (5 min)"
                  series={series}
                  minY={0}
                  maxY={100}
                  yLabels={["100%", "75%", "50%", "25%", "0%"]}
                  lines={[
                    { key: "cpu", label: "CPU", color: "#4F38DF" },
                    { key: "ram", label: "RAM", color: "#00d2ff" },
                  ]}
                  height={230}
                  xStartLabel="-5 min"
                  xEndLabel="Now"
                />
              </Grid>
            </Grid>

            <VuiBox mb={2}>
              <LineChartPanel
                title="RSSI (dBm)"
                subtitle="Last 1 hour"
                series={series}
                minY={-100}
                maxY={-20}
                yLabels={[-20, -40, -60, -80, -100]}
                lines={[{ key: "rssi", label: "RSSI", color: "#4ade80" }]}
                height={250}
                note="RSSI (in dBm where <= -90 is the the open verifical if available."
                xStartLabel="-1h"
                xEndLabel="Now"
                horizontalLayout={true}
              />
            </VuiBox>

            <Grid container spacing={2} mb={2}>
              <Grid item xs={12} md={4}>
                <MetricCardLeft 
                  title="Battery" 
                  value={`Battery | ${fmtNumber(g?.batteryVoltageV, 2, "V")} | ${fmtInt(g?.batteryPercent, "%")}`} 
                  progress={g?.batteryPercent || 0}
                  icon={<IoBatteryFull size="24px" color="white" />}
                />
              </Grid>
              <Grid item xs={12} md={4}>
                <MetricCardLeft 
                  title="WiFi & Mạng IP" 
                  value={`${g?.wifiSsid || "Unknown"} | IP: ${g?.staIp || selectedGateway?.ip || "N/A"}`}
                  subtitle={`Signal: ${fmtInt(g?.wifiRssi, " dBm")} | Gateway: ${g?.staGateway || "192.168.1.1"}`}
                  icon={<IoWifi size="24px" color="white" />}
                />
              </Grid>
              <Grid item xs={12} md={4}>
                <MetricCardLeft 
                  title="System" 
                  value={`${g?.clientType || "esp32"} | RAM ${fmtInt(g?.ramUsedKb, "kB")} | temp ${g?.chipTempInternalSupported ? fmtNumber(g?.chipTempC, 1, " C") : "N/A"}`} 
                  icon={<IoHardwareChip size="24px" color="white" />}
                />
              </Grid>
            </Grid>

            <GatewayEventLogs logs={gatewayLogs || []} />
          </>
        )}

        {/* Edit Gateway Dialog */}
        <Dialog
          open={Boolean(editGw)}
          onClose={() => setEditGw(null)}
          PaperProps={{
            sx: {
              background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
              backdropFilter: "blur(20px)",
              border: "1px solid rgba(255, 255, 255, 0.15)",
              borderRadius: "18px",
              color: "#fff",
              minWidth: { xs: "90%", sm: "460px" },
              p: 2,
            },
          }}
        >
          <DialogTitle sx={{ color: "#fff", fontWeight: "bold" }}>
            Cấu hình Gateway: {editGw?.name || editGw?.gw_id}
          </DialogTitle>
          <DialogContent>
            <VuiBox display="flex" flexDirection="column" gap={2} mt={1}>
              <TextField
                label="Tên Trạm Gateway"
                value={editName}
                onChange={(e) => setEditName(e.target.value)}
                fullWidth
                variant="outlined"
                InputLabelProps={{ style: { color: "rgba(255,255,255,0.7)" } }}
                sx={{
                  "& .MuiOutlinedInput-root": {
                    color: "#fff",
                    borderRadius: "12px",
                    background: "rgba(0,0,0,0.3)",
                    border: "1px solid rgba(255,255,255,0.15)",
                  },
                }}
              />
              <TextField
                label="Vị Trí / Khu Vực (Ví dụ: Nhà Màng 1, Vườn A)"
                value={editLocation}
                onChange={(e) => setEditLocation(e.target.value)}
                fullWidth
                variant="outlined"
                InputLabelProps={{ style: { color: "rgba(255,255,255,0.7)" } }}
                sx={{
                  "& .MuiOutlinedInput-root": {
                    color: "#fff",
                    borderRadius: "12px",
                    background: "rgba(0,0,0,0.3)",
                    border: "1px solid rgba(255,255,255,0.15)",
                  },
                }}
              />
              <Grid container spacing={2}>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Vĩ độ (Latitude)"
                    value={editLatitude}
                    onChange={(e) => setEditLatitude(e.target.value)}
                    placeholder="Ví dụ: 10.8231"
                    fullWidth
                    variant="outlined"
                    InputLabelProps={{ style: { color: "rgba(255,255,255,0.7)" } }}
                    sx={{
                      "& .MuiOutlinedInput-root": {
                        color: "#fff",
                        borderRadius: "12px",
                        background: "rgba(0,0,0,0.3)",
                        border: "1px solid rgba(255,255,255,0.15)",
                      },
                    }}
                  />
                </Grid>
                <Grid item xs={12} sm={6}>
                  <TextField
                    label="Kinh độ (Longitude)"
                    value={editLongitude}
                    onChange={(e) => setEditLongitude(e.target.value)}
                    placeholder="Ví dụ: 106.6297"
                    fullWidth
                    variant="outlined"
                    InputLabelProps={{ style: { color: "rgba(255,255,255,0.7)" } }}
                    sx={{
                      "& .MuiOutlinedInput-root": {
                        color: "#fff",
                        borderRadius: "12px",
                        background: "rgba(0,0,0,0.3)",
                        border: "1px solid rgba(255,255,255,0.15)",
                      },
                    }}
                  />
                </Grid>
              </Grid>
              <TextField
                label="Ghi chú kỹ thuật"
                value={editNotes}
                onChange={(e) => setEditNotes(e.target.value)}
                fullWidth
                multiline
                rows={2}
                variant="outlined"
                InputLabelProps={{ style: { color: "rgba(255,255,255,0.7)" } }}
                sx={{
                  "& .MuiOutlinedInput-root": {
                    color: "#fff",
                    borderRadius: "12px",
                    background: "rgba(0,0,0,0.3)",
                    border: "1px solid rgba(255,255,255,0.15)",
                  },
                }}
              />
            </VuiBox>
          </DialogContent>
          <DialogActions sx={{ px: 3, pb: 2 }}>
            <Button onClick={() => setEditGw(null)} sx={{ color: "rgba(255,255,255,0.6)" }}>
              Hủy
            </Button>
            <Button
              variant="contained"
              disabled={isSaving}
              onClick={handleSaveEdit}
              sx={{
                background: "linear-gradient(135deg, #0075ff 0%, #00d2ff 100%) !important",
                color: "#fff",
                fontWeight: "bold",
                borderRadius: "10px",
                px: 3,
              }}
            >
              {isSaving ? "Đang lưu..." : "Lưu thay đổi"}
            </Button>
          </DialogActions>
        </Dialog>

        {/* Delete Gateway Confirmation Dialog */}
        <Dialog
          open={Boolean(confirmDeleteGw)}
          onClose={() => setConfirmDeleteGw(null)}
          PaperProps={{
            sx: {
              background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
              backdropFilter: "blur(20px)",
              border: "1px solid rgba(255, 82, 82, 0.35)",
              borderRadius: "18px",
              color: "#fff",
              minWidth: { xs: "90%", sm: "400px" },
              p: 2,
            },
          }}
        >
          <DialogTitle sx={{ color: "#ff5252", fontWeight: "bold", display: "flex", alignItems: "center", gap: 1 }}>
            <IoTrashOutline size="22px" /> Xác nhận xóa Gateway?
          </DialogTitle>
          <DialogContent>
            <VuiTypography variant="button" color="text" display="block">
              Bạn có chắc chắn muốn xóa trạm Gateway: <b style={{ color: "#fff" }}>{confirmDeleteGw?.name || confirmDeleteGw?.gw_id}</b> (ID: {confirmDeleteGw?.gw_id})?
            </VuiTypography>
            <VuiTypography variant="caption" color="text" display="block" mt={1} sx={{ opacity: 0.8 }}>
              Hành động này sẽ xóa dữ liệu Gateway và các bản ghi lịch sử tương ứng khỏi cơ sở dữ liệu.
            </VuiTypography>
          </DialogContent>
          <DialogActions sx={{ px: 3, pb: 2 }}>
            <Button onClick={() => setConfirmDeleteGw(null)} sx={{ color: "rgba(255,255,255,0.6)" }}>
              Hủy
            </Button>
            <Button
              variant="contained"
              disabled={isDeletingGw}
              onClick={async () => {
                if (!confirmDeleteGw || !deleteGateway) return;
                setIsDeletingGw(true);
                await deleteGateway(confirmDeleteGw.gw_id);
                setIsDeletingGw(false);
                setConfirmDeleteGw(null);
              }}
              sx={{
                background: "linear-gradient(135deg, #ff5252 0%, #ff1744 100%) !important",
                color: "#fff",
                fontWeight: "bold",
                borderRadius: "10px",
                px: 3,
              }}
            >
              {isDeletingGw ? "Đang xóa..." : "Xác nhận xóa"}
            </Button>
          </DialogActions>
        </Dialog>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Gateway;
