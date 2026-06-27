import React, { useMemo } from "react";
import Grid from "@mui/material/Grid";
import { Card } from "@mui/material";
import ReactApexChart from "react-apexcharts";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Layout components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";

// Icons
import { IoServer, IoHardwareChip, IoTimeOutline, IoPulse } from "react-icons/io5";

import { useDashboardRealtime } from "hooks/useDashboardRealtime";
import { lineChartOptionsDashboard } from "layouts/dashboard/data/lineChartOptions";

const glassCardSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.76) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(50, 105, 255, 0.35)",
  boxShadow: "0 0 24px rgba(0, 106, 255, 0.22), inset 0 1px 0 rgba(255,255,255,0.04)",
  borderRadius: "16px",
  backdropFilter: "blur(42px)",
  height: "100%",
};

function PerformanceCard({ title, value, detail, icon, color = "#0075ff" }) {
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

function ServerMonitor() {
  const { serverMetrics, serverMetricsSeries } = useDashboardRealtime();

  const chartData = useMemo(() => {
    return [
      { name: "CPU Load (%)", data: serverMetricsSeries.map((s) => ({ x: s.t, y: Number((s.cpu || 0).toFixed(1)) })) },
      { name: "RAM Used (%)", data: serverMetricsSeries.map((s) => ({ x: s.t, y: Number((s.ram || 0).toFixed(1)) })) },
    ];
  }, [serverMetricsSeries]);

  const tempChartData = useMemo(() => {
    return [
      { name: "Temperature (°C)", data: serverMetricsSeries.map((s) => ({ x: s.t, y: Number((s.temp || 0).toFixed(1)) })) },
    ];
  }, [serverMetricsSeries]);

  const chartOptions = {
    ...lineChartOptionsDashboard,
    colors: ["#0075FF", "#8a2cff"],
    xaxis: { 
      ...lineChartOptionsDashboard.xaxis, 
      type: "datetime",
      labels: {
        ...lineChartOptionsDashboard.xaxis.labels,
        formatter: (val) => {
          if (!val) return "";
          const d = new Date(val);
          return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`;
        }
      }
    },
    yaxis: { ...lineChartOptionsDashboard.yaxis, max: 100 },
  };

  const tempChartOptions = {
    ...lineChartOptionsDashboard,
    colors: ["#ff285c"],
    xaxis: { 
      ...lineChartOptionsDashboard.xaxis, 
      type: "datetime",
      labels: {
        ...lineChartOptionsDashboard.xaxis.labels,
        formatter: (val) => {
          if (!val) return "";
          const d = new Date(val);
          return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`;
        }
      }
    },
    yaxis: { ...lineChartOptionsDashboard.yaxis, min: 0, max: 100 },
  };

  const formatUptime = (secs) => {
    if (!secs) return "0s";
    const h = Math.floor(secs / 3600);
    const m = Math.floor((secs % 3600) / 60);
    const s = Math.floor(secs % 60);
    if (h > 0) return `${h}h ${m}m ${s}s`;
    if (m > 0) return `${m}m ${s}s`;
    return `${s}s`;
  };

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <VuiBox mb={3}>
          <Grid container spacing={3}>
            <Grid item xs={12} md={3}>
              <PerformanceCard 
                title="CPU Usage" 
                value={serverMetrics ? `${Math.round(serverMetrics.cpuLoadPercent)}%` : "-"} 
                icon={<IoHardwareChip size="24px" />} 
                color="#0075ff"
              />
            </Grid>
            <Grid item xs={12} md={3}>
              <PerformanceCard 
                title="RAM Usage" 
                value={serverMetrics ? `${Math.round(serverMetrics.ramUsedPercent)}%` : "-"} 
                detail={serverMetrics ? `(${serverMetrics.ramUsedMb.toFixed(0)} / ${serverMetrics.ramTotalMb.toFixed(0)} MB)` : ""}
                icon={<IoServer size="24px" />} 
                color="#8a2cff"
              />
            </Grid>
            <Grid item xs={12} md={3}>
              <PerformanceCard 
                title="Temperature" 
                value={serverMetrics && serverMetrics.chipTempC ? `${Math.round(serverMetrics.chipTempC)}°C` : "-"} 
                icon={<IoPulse size="24px" />} 
                color="#ff285c"
              />
            </Grid>
            <Grid item xs={12} md={3}>
              <PerformanceCard 
                title="System Uptime" 
                value={serverMetrics ? formatUptime(serverMetrics.uptimeS) : "-"} 
                icon={<IoTimeOutline size="24px" />} 
                color="#01f7a7"
              />
            </Grid>
          </Grid>
        </VuiBox>

        <VuiBox mb={3}>
          <Card sx={glassCardSx}>
            <VuiBox p={3}>
              <VuiTypography variant="h5" color="white" fontWeight="bold" mb={2}>
                CPU & RAM Usage History
              </VuiTypography>
              <VuiBox sx={{ height: "350px", mt: 1 }}>
                <ReactApexChart options={chartOptions} series={chartData} type="area" height="100%" />
              </VuiBox>
            </VuiBox>
          </Card>
        </VuiBox>

        <VuiBox mb={3}>
          <Card sx={glassCardSx}>
            <VuiBox p={3}>
              <VuiTypography variant="h5" color="white" fontWeight="bold" mb={2}>
                Temperature History
              </VuiTypography>
              <VuiBox sx={{ height: "350px", mt: 1 }}>
                <ReactApexChart options={tempChartOptions} series={tempChartData} type="area" height="100%" />
              </VuiBox>
            </VuiBox>
          </Card>
        </VuiBox>

      </VuiBox>
    </DashboardLayout>
  );
}

export default ServerMonitor;
