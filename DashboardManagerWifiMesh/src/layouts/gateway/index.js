import React from "react";
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";

import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useMeshRealtime } from "context/meshRealtime";

import { IoWifi, IoServer, IoCellular, IoTime, IoBatteryFull, IoHardwareChip, IoCalendar, IoChevronDown, IoExpand } from "react-icons/io5";

const panelSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.08)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
  borderRadius: "16px",
  backdropFilter: "blur(42px)",
};

const iconBoxSx = {
  width: "48px",
  height: "48px",
  borderRadius: "12px",
  display: "flex",
  alignItems: "center",
  justifyContent: "center",
  background: "#111c44",
  border: "1px solid rgba(255, 255, 255, 0.05)",
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
        <VuiBox sx={{ ...iconBoxSx, background: "linear-gradient(127deg, #4F38DF, #6142FF)", flexShrink: 0 }}>
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
  const { gatewayStatus, gatewaySeries, gatewayLogs, wsOpen } = useMeshRealtime();
  const g = gatewayStatus;
  const series = gatewaySeries || [];

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
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
              label="IP Address" 
              value={g?.staIp || "-"} 
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
              title="WiFi" 
              value={`${g?.wifiSsid || "Unknown"} | gateway ${g?.staGateway || "192.168.1.1"}`}
              subtitle={`Signal: ${fmtInt(g?.wifiRssi, " dBm")}`}
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
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Gateway;
