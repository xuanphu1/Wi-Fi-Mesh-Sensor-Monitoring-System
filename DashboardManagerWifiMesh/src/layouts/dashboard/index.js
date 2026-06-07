import { useMemo, useState, useEffect } from "react";
import { NavLink } from "react-router-dom";

import Grid from "@mui/material/Grid";
import { Card } from "@mui/material";

import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import {
  IoAlert,
  IoChevronForward,
  IoHardwareChip,
  IoPulse,
  IoServer,
  IoShieldCheckmark,
  IoTime,
  IoWifi,
} from "react-icons/io5";

import { useDashboardRealtime } from "hooks/useDashboardRealtime";

const glassCardSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.76) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(50, 105, 255, 0.35)",
  boxShadow: "0 0 24px rgba(0, 106, 255, 0.22), inset 0 1px 0 rgba(255,255,255,0.04)",
  borderRadius: "16px",
  backdropFilter: "blur(42px)",
};

function metricText(v, unit = "") {
  if (typeof v !== "number" || Number.isNaN(v)) return "-";
  return `${v.toFixed(2)}${unit}`;
}

function formatLatencyText(latencyMs) {
  if (typeof latencyMs !== "number" || Number.isNaN(latencyMs)) return "--";
  if (latencyMs < 1000) return `${Math.round(latencyMs)} ms`;
  return `${(latencyMs / 1000).toFixed(2)} s`;
}

function countUniqueFirmware(nodes) {
  const set = new Set(nodes.map((n) => String(n.firmwareVersion || "").trim()).filter(Boolean));
  return set.size;
}

function Sparkline({ color = "#00d5ff", data = [], width = 118, height = 44, minPoints = 20, padWithZero = false }) {
  let plotData = [...data];
  if (plotData.length < minPoints) {
    const padCount = minPoints - plotData.length;
    let padVal = 0;
    if (!padWithZero && plotData.length > 0) {
      padVal = plotData[0];
    }
    const padding = Array(padCount).fill(padVal);
    plotData = [...padding, ...plotData];
  }

  if (plotData.length < 2) {
    return (
      <svg viewBox={`0 0 ${width} ${height}`} width={width} height={height} aria-hidden="true" style={{ overflow: "visible", display: "block" }}>
        <line x1="0" y1={height / 2} x2={width} y2={height / 2} stroke={color} strokeOpacity="0.3" strokeWidth="2" strokeDasharray="4" />
      </svg>
    );
  }

  const min = Math.min(...plotData);
  const max = Math.max(...plotData);
  const span = Math.max(1, max - min);
  const paddingY = 4;
  const innerHeight = height - paddingY * 2;

  const points = plotData.map((val, i) => {
    const x = (i / (plotData.length - 1)) * width;
    const y = paddingY + innerHeight - ((val - min) / span) * innerHeight;
    return { x, y };
  });

  const linePath = points.map((p, i) => `${i === 0 ? "M" : "L"}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(" ");
  const areaPath = `${linePath} L${width},${height} L0,${height} Z`;

  const gradientId = `glow-${color.replace("#", "")}`;

  return (
    <svg viewBox={`0 0 ${width} ${height}`} width={width} height={height} aria-hidden="true" style={{ overflow: "visible", display: "block" }}>
      <defs>
        <linearGradient id={gradientId} x1="0" x2="0" y1="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity="0.4" />
          <stop offset="100%" stopColor={color} stopOpacity="0.0" />
        </linearGradient>
      </defs>
      <path d={areaPath} fill={`url(#${gradientId})`} />
      <path d={linePath} fill="none" stroke={color} strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

function IconBubble({ icon, color = "#0075ff" }) {
  return (
    <VuiBox
      sx={{
        width: 46,
        height: 46,
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
  );
}

function TopMetricCard({ title, value, detail, icon, active }) {
  return (
    <Card sx={{ ...glassCardSx, height: "100%", position: "relative", overflow: "hidden" }}>
      <VuiBox p={2} display="flex" alignItems="center" justifyContent="space-between">
        <VuiBox>
          <VuiTypography variant="caption" color="text" display="block" mb={0.5}>
            {title}
          </VuiTypography>
          <VuiBox display="flex" alignItems="baseline" gap={1} flexWrap="wrap">
            <VuiTypography variant="h4" color="white" fontWeight="bold" sx={{ lineHeight: 1 }}>
              {value}
            </VuiTypography>
            <VuiTypography variant="caption" color={active ? "success" : "warning"} fontWeight="bold">
              {detail}
            </VuiTypography>
          </VuiBox>
        </VuiBox>
        <IconBubble icon={icon} />
      </VuiBox>
      <VuiBox
        sx={{
          position: "absolute",
          right: 10,
          top: 10,
          width: 8,
          height: 8,
          borderRadius: "50%",
          background: active ? "#01f7a7" : "#ffb547",
          boxShadow: active ? "0 0 12px #01f7a7" : "0 0 12px #ffb547",
        }}
      />
    </Card>
  );
}

function SectionTitle({ icon, children, color = "#0075ff" }) {
  return (
    <VuiBox display="flex" alignItems="center" gap={1.25} mb={2}>
      <VuiBox
        sx={{
          width: 34,
          height: 34,
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
      <VuiTypography variant="lg" color="white" fontWeight="bold">
        {children}
      </VuiTypography>
    </VuiBox>
  );
}

function HealthTile({ label, value, color, data, footer, padWithZero }) {
  return (
    <VuiBox
      px={1.5}
      py={1.5}
      sx={{
        height: "100%",
        minHeight: 104,
        border: "1px solid rgba(255,255,255,0.08)",
        borderRadius: "12px",
        background: "linear-gradient(127deg, rgba(255,255,255,0.045), rgba(255,255,255,0.018))",
      }}
    >
      <VuiTypography variant="button" color="text" display="block" mb={1} sx={{ fontSize: "14px" }}>
        {label}
      </VuiTypography>
      <Grid container alignItems="end" sx={{ height: "calc(100% - 28px)" }}>
        <Grid item xs={data ? 5 : 12}>
          <VuiTypography variant="h3" color="white" fontWeight="bold" sx={{ lineHeight: 1 }}>
            {value}
          </VuiTypography>
          {footer ? (
            <VuiTypography variant="caption" color="text" display="block" mt={1} sx={{ fontSize: "13px" }}>
              {footer}
            </VuiTypography>
          ) : null}
        </Grid>
        {data && (
          <Grid item xs={7} display="flex" justifyContent="flex-end">
            <Sparkline color={color} data={data} width={118} padWithZero={padWithZero} />
          </Grid>
        )}
      </Grid>
    </VuiBox>
  );
}

function problemLabel(node) {
  if (!node.online) return { text: "Offline", color: "error" };
  if (Array.isArray(node.runtimeErrors) && node.runtimeErrors.length > 0) {
    return { text: `${node.runtimeErrors.length} runtime error(s)`, color: "warning" };
  }
  if (typeof node.latencyMs === "number" && Number.isFinite(node.latencyMs) && node.latencyMs >= 2000) {
    return { text: `High latency ${formatLatencyText(node.latencyMs)}`, color: "info" };
  }
  return { text: "No RTC / invalid RTC", color: "warning" };
}

function ProblemNodeRow({ node }) {
  const issue = problemLabel(node);
  return (
    <VuiBox
      component={NavLink}
      to={`/nodes/${encodeURIComponent(node.id || node.ip || "")}`}
      px={1.25}
      py={0.9}
      sx={{
        display: "block",
        textDecoration: "none",
        border: "1px solid rgba(255,255,255,0.08)",
        borderRadius: "10px",
        background: "rgba(255,255,255,0.025)",
        transition: "all 160ms ease",
        "&:hover": {
          transform: "translateX(2px)",
          borderColor: "rgba(0,117,255,0.55)",
          boxShadow: "0 0 18px rgba(0,117,255,0.2)",
        },
      }}
    >
      <Grid container spacing={1} alignItems="center">
        <Grid item xs={5}>
          <VuiTypography variant="body2" color="white" fontWeight="bold" sx={{ fontSize: "15px" }}>
            {node.ip || node.id || "-"}
          </VuiTypography>
          <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "13px" }}>
            {node.meshLevel != null ? `Mesh L${node.meshLevel}` : "Mesh"} | FW {node.firmwareVersion || "-"}
          </VuiTypography>
        </Grid>
        <Grid item xs={6}>
          <VuiTypography variant="body2" color={issue.color} fontWeight="bold" sx={{ fontSize: "15px" }}>
            {issue.text}
          </VuiTypography>
          <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "13px" }}>
            Last seen: {node.lastSeenIso ? new Date(node.lastSeenIso).toLocaleString() : "-"}
          </VuiTypography>
        </Grid>
        <Grid item xs={1} textAlign="right">
          <IoChevronForward color="#8fa7d6" />
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function SensorRealtimeRow({ row, index, data }) {
  const colors = ["#0075ff", "#00d1a7", "#8a2cff", "#ffb547"];
  const color = colors[index % colors.length];
  return (
    <VuiBox
      px={1.25}
      py={0.9}
      sx={{
        border: "1px solid rgba(255,255,255,0.08)",
        borderRadius: "10px",
        background: "rgba(255,255,255,0.025)",
      }}
    >
      <Grid container spacing={1} alignItems="center">
        <Grid item xs={12} md={5}>
          <VuiTypography variant="caption" color="text" display="block">
            L{row.meshLevel} | {row.nodeIp} | Port {row.port} | {row.sensor} | {row.label}
          </VuiTypography>
          <VuiTypography variant="h5" color="white" fontWeight="bold">
            {metricText(row.value, row.unit ? ` ${row.unit}` : "")}
          </VuiTypography>
        </Grid>
        <Grid item xs={11} md={6} display="flex" justifyContent="flex-end">
          <Sparkline color={color} data={data} width={200} height={36} />
        </Grid>
        <Grid item xs={1} textAlign="right">
          <IoChevronForward color="#8fa7d6" />
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function DeviceInfoRow({ icon, label, value }) {
  return (
    <VuiBox
      px={1.25}
      py={0.9}
      sx={{
        border: "1px solid rgba(255,255,255,0.08)",
        borderRadius: "10px",
        background: "rgba(255,255,255,0.025)",
      }}
    >
      <Grid container spacing={1} alignItems="center">
        <Grid item xs={1.5}>
          <VuiBox
            sx={{
              width: 28,
              height: 28,
              borderRadius: "8px",
              display: "grid",
              placeItems: "center",
              background: "rgba(0,117,255,0.16)",
              color: "#0075ff",
            }}
          >
            {icon}
          </VuiBox>
        </Grid>
        <Grid item xs={5.5}>
          <VuiTypography variant="button" color="text">
            {label}
          </VuiTypography>
        </Grid>
        <Grid item xs={5}>
          <VuiTypography variant="button" color="white" fontWeight="medium" display="block" textAlign="right">
            {value || "-"}
          </VuiTypography>
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function Dashboard() {
  const {
    connected,
    nodeStats,
    nodes,
    deviceInfo,
    latestSensors,
    throughput,
    lastUpdateIso,
  } = useDashboardRealtime();

  const latestSensorRows = latestSensors.slice(0, 4);
  const runtimeErrorNodes = useMemo(
    () => nodes.filter((n) => Array.isArray(n.runtimeErrors) && n.runtimeErrors.length > 0),
    [nodes]
  );
  const nodesWithoutRtc = useMemo(
    () => nodes.filter((n) => n.online && (typeof n.latencyMs !== "number" || Number.isNaN(n.latencyMs))),
    [nodes]
  );
  const highLatencyNodes = useMemo(
    () => nodes.filter((n) => typeof n.latencyMs === "number" && Number.isFinite(n.latencyMs) && n.latencyMs >= 2000),
    [nodes]
  );
  const problemNodes = useMemo(
    () =>
      [...nodes]
        .filter(
          (n) =>
            !n.online ||
            (Array.isArray(n.runtimeErrors) && n.runtimeErrors.length > 0) ||
            (typeof n.latencyMs === "number" && Number.isFinite(n.latencyMs) && n.latencyMs >= 2000) ||
            (!n.latencyMs && n.online)
        )
        .sort((a, b) => {
          const score = (node) => {
            if (!node.online) return 0;
            if (Array.isArray(node.runtimeErrors) && node.runtimeErrors.length > 0) return 1;
            if (typeof node.latencyMs === "number" && Number.isFinite(node.latencyMs) && node.latencyMs >= 2000) return 2;
            return 3;
          };
          const sa = score(a);
          const sb = score(b);
          if (sa !== sb) return sa - sb;
          return String(a.id || a.ip).localeCompare(String(b.id || b.ip));
        })
        .slice(0, 5),
    [nodes]
  );
  const firmwareVariantCount = useMemo(() => countUniqueFirmware(nodes), [nodes]);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <VuiBox mb={3}>
          <Grid container spacing={2.5}>
            <Grid item xs={12} md={6} xl={3}>
              <TopMetricCard
                title="Root Status"
                value={connected ? "Connected" : "Waiting"}
                detail={connected ? "ESP online" : "No data"}
                active={connected}
                icon={<IoWifi size="22px" />}
              />
            </Grid>
            <Grid item xs={12} md={6} xl={3}>
              <TopMetricCard
                title="Nodes Online / Offline"
                value={`${nodeStats.online} / ${nodeStats.offline}`}
                detail={`Total ${nodeStats.total}`}
                active={nodeStats.online > 0}
                icon={<IoHardwareChip size="22px" />}
              />
            </Grid>
            <Grid item xs={12} md={6} xl={3}>
              <TopMetricCard
                title="Runtime Error Nodes"
                value={String(runtimeErrorNodes.length)}
                detail={runtimeErrorNodes.length > 0 ? "Need checking" : "No runtime error"}
                active={runtimeErrorNodes.length === 0}
                icon={<IoPulse size="22px" />}
              />
            </Grid>
            <Grid item xs={12} md={6} xl={3}>
              <TopMetricCard
                title="Last Update"
                value={lastUpdateIso ? new Date(lastUpdateIso).toLocaleTimeString() : "--:--:--"}
                detail={connected ? "Realtime stream active" : "Waiting data"}
                active={connected}
                icon={<IoTime size="22px" />}
              />
            </Grid>
          </Grid>
        </VuiBox>

        <VuiBox mb={2.5}>
          <Grid container spacing={2.5} alignItems="stretch">
            <Grid item xs={12} xl={6}>
              <Card sx={{ ...glassCardSx, height: "100%" }}>
                <VuiBox p={2}>
                  <SectionTitle icon={<IoShieldCheckmark size="18px" />} color="#0075ff">
                    Network Health Summary
                  </SectionTitle>
                  <Grid container spacing={1.5}>
                    <Grid item xs={12} md={6}>
                      <HealthTile label="High latency nodes (&gt;= 2s)" value={highLatencyNodes.length} color="#01f7a7" seed={2} />
                    </Grid>
                    <Grid item xs={12} md={6}>
                      <HealthTile label="Nodes without valid RTC" value={nodesWithoutRtc.length} color="#ffb547" seed={5} />
                    </Grid>
                    <Grid item xs={12} md={6}>
                      <HealthTile label="Firmware variants" value={firmwareVariantCount} color="#0075ff" seed={8} />
                    </Grid>
                    <Grid item xs={12} md={6}>
                      <HealthTile
                        label="Current throughput"
                        value={`${throughput.currentBytesPerSec} B/s`}
                        footer={`Avg ${throughput.averageBytesPerSec} B/s`}
                        color="#8a2cff"
                        seed={11}
                      />
                    </Grid>
                  </Grid>
                </VuiBox>
              </Card>
            </Grid>

            <Grid item xs={12} xl={6}>
              <Card sx={{ ...glassCardSx, height: "100%" }}>
                <VuiBox p={2}>
                  <SectionTitle icon={<IoAlert size="18px" />} color="#ff285c">
                    Problem Nodes
                  </SectionTitle>
                  <VuiTypography variant="button" color="text" mb={1.5} display="block">
                    Offline nodes, runtime errors, high latency, or missing RTC
                  </VuiTypography>
                  {problemNodes.length === 0 ? (
                    <VuiTypography color="text" variant="button">
                      No problem nodes right now.
                    </VuiTypography>
                  ) : (
                    <VuiBox display="grid" gap={0.75}>
                      {problemNodes.map((node) => (
                        <ProblemNodeRow key={node.id || node.ip} node={node} />
                      ))}
                    </VuiBox>
                  )}
                </VuiBox>
              </Card>
            </Grid>
          </Grid>
        </VuiBox>

        <VuiBox mb={2.5}>
          <Grid container spacing={2.5}>
            <Grid item xs={12} xl={8}>
              <Card sx={{ ...glassCardSx, height: "100%" }}>
                <VuiBox p={2}>
                  <SectionTitle icon={<IoPulse size="18px" />} color="#0075ff">
                    Sensor Realtime
                  </SectionTitle>
                  {latestSensorRows.length === 0 ? (
                    <VuiTypography color="text">No sensor data received from device yet.</VuiTypography>
                  ) : (
                    <VuiBox display="grid" gap={0.75}>
                      {latestSensorRows.map((row, index) => (
                        <SensorRealtimeRow key={row.key} row={row} index={index} />
                      ))}
                    </VuiBox>
                  )}
                </VuiBox>
              </Card>
            </Grid>

            <Grid item xs={12} xl={4}>
              <Card sx={{ ...glassCardSx, height: "100%" }}>
                <VuiBox p={2}>
                  <SectionTitle icon={<IoWifi size="18px" />} color="#0075ff">
                    WiFi / Device info
                  </SectionTitle>
                  <VuiBox display="grid" gap={0.75}>
                    <DeviceInfoRow icon={<IoWifi size="15px" />} label="SSID / Adapter" value={deviceInfo.ssid || "Unknown"} />
                    <DeviceInfoRow icon={<IoServer size="15px" />} label="Gateway" value={deviceInfo.gatewayIp || "-"} />
                    <DeviceInfoRow icon={<IoHardwareChip size="15px" />} label="Server IP" value={deviceInfo.staIp || "-"} />
                    <DeviceInfoRow icon={<IoPulse size="15px" />} label="MAC" value={deviceInfo.mac || "-"} />
                  </VuiBox>
                </VuiBox>
              </Card>
            </Grid>
          </Grid>
        </VuiBox>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Dashboard;
