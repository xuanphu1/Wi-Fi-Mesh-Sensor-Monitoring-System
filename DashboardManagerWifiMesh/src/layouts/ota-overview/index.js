import { useMemo } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { mockOta } from "api/mockData";
import {
  IoHardwareChip,
  IoArrowUpCircle,
  IoAlertCircle,
  IoChevronForward,
  IoCloudDownload,
  IoReload,
  IoServer
} from "react-icons/io5";

function OtaSummaryCard({ title, icon, valueContent, borderColor, iconBg, iconColor }) {
  return (
    <Card sx={{
      border: borderColor ? `1px solid ${borderColor}` : "1px solid rgba(255, 255, 255, 0.05)",
      background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
      borderRadius: "16px",
      boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
      p: 2.5,
      height: "100%"
    }}>
      <VuiBox display="flex" justifyContent="space-between" alignItems="center">
        <VuiBox display="flex" alignItems="center" gap={2}>
          <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: iconBg, color: iconColor, display: "grid", placeItems: "center" }}>
            {icon}
          </VuiBox>
          <VuiBox>
            <VuiTypography variant="caption" color="text" display="block" mb={0.5}>{title}</VuiTypography>
            {valueContent}
          </VuiBox>
        </VuiBox>
        <IoChevronForward color="rgba(255,255,255,0.3)" />
      </VuiBox>
    </Card>
  );
}

function OtaRow({ nodeId, current, latest, status }) {
  const isOk = status === "ok";
  const isOutdated = status === "outdated";
  // status error is treated as offline in mockup
  
  const statusColor = isOk ? "#01f7a7" : isOutdated ? "#ffb547" : "#ff285c";
  const statusBg = isOk ? "transparent" : `rgba(${isOutdated ? "255,181,71" : "255,40,92"}, 0.15)`;
  
  const currentIsLatest = current === latest;
  const currentColor = currentIsLatest ? "success" : "info";

  return (
    <VuiBox sx={{ background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)", borderRadius: "16px", border: "1px solid rgba(255,255,255,0.05)", mb: 1.5, p: 2, display: "flex", alignItems: "center", transition: "all 0.3s", "&:hover": { transform: "translateY(-2px)", boxShadow: "0 8px 24px rgba(0,0,0,0.3)", border: "1px solid rgba(255,255,255,0.1)" } }}>
      <Grid container alignItems="center">
        <Grid item xs={3}>
          <VuiBox display="flex" alignItems="center" gap={2}>
            <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0,117,255,0.15)", color: "#0075ff", display: "grid", placeItems: "center" }}>
              <IoServer size="16px" />
            </VuiBox>
            <VuiTypography variant="button" color="white" fontWeight="bold">{nodeId}</VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={3}>
          <VuiTypography variant="button" color={currentColor} fontWeight="bold">{current}</VuiTypography>
        </Grid>
        <Grid item xs={3}>
          <VuiTypography variant="button" color="success" fontWeight="bold">{latest}</VuiTypography>
        </Grid>
        <Grid item xs={3}>
          <VuiBox display="flex" alignItems="center" justifyContent="space-between">
            <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: statusBg }}>
              <VuiTypography variant="caption" fontWeight="bold" sx={{ color: statusColor }}>{status === "error" ? "OFFLINE" : status.toUpperCase()}</VuiTypography>
            </VuiBox>
            <IoChevronForward color="rgba(255,255,255,0.3)" />
          </VuiBox>
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function OtaOverview() {
  const needUpdate = useMemo(() => mockOta.nodes.filter((n) => n.status === "outdated").length, []);
  const otaErrors = useMemo(() => mockOta.nodes.filter((n) => n.status === "error").length, []);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <Grid container spacing={3} mb={3}>
          <Grid item xs={12} md={6} lg={4}>
            <OtaSummaryCard 
              title="Root Firmware"
              icon={<IoHardwareChip size="20px" />}
              iconBg="rgba(0,117,255,0.2)"
              iconColor="#0075ff"
              borderColor="rgba(0,117,255,0.5)"
              valueContent={
                <VuiBox display="flex" gap={1.5} alignItems="center">
                  <VuiTypography variant="caption" color="text">Current: <VuiTypography component="span" variant="caption" color="white" fontWeight="bold">{mockOta.root.current}</VuiTypography></VuiTypography>
                  <VuiTypography variant="caption" color="text">Latest: <VuiTypography component="span" variant="caption" color={mockOta.root.updateAvailable ? "warning" : "success"} fontWeight="bold">{mockOta.root.latestAvailable}</VuiTypography></VuiTypography>
                </VuiBox>
              }
            />
          </Grid>
          <Grid item xs={12} md={6} lg={4}>
            <OtaSummaryCard 
              title="Nodes needing update"
              icon={<IoArrowUpCircle size="22px" />}
              iconBg="rgba(138,44,255,0.2)"
              iconColor="#8a2cff"
              valueContent={
                <VuiTypography variant="h4" color="white" fontWeight="bold">{needUpdate}</VuiTypography>
              }
            />
          </Grid>
          <Grid item xs={12} md={6} lg={4}>
            <OtaSummaryCard 
              title="OTA errors"
              icon={<IoAlertCircle size="22px" />}
              iconBg="rgba(255,40,92,0.2)"
              iconColor="#ff285c"
              valueContent={
                <VuiTypography variant="h4" color="white" fontWeight="bold">{otaErrors}</VuiTypography>
              }
            />
          </Grid>
        </Grid>

        <Card sx={{
          padding: "24px 20px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)"
        }}>
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={4}>
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoCloudDownload size="18px" />
              </VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                OTA Overview (Nodes)
              </VuiTypography>
            </VuiBox>
            <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)", display: "grid", placeItems: "center", cursor: "pointer", transition: "all 0.2s", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>
              <IoReload color="#fff" size="16px" />
            </VuiBox>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">NODE ID</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">CURRENT</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">LATEST</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">STATUS</VuiTypography></Grid>
          </Grid>

          <VuiBox>
            {mockOta.nodes.map((node, index) => (
              <OtaRow 
                key={index}
                nodeId={node.nodeId}
                current={node.current}
                latest={node.latestAvailable}
                status={node.status}
              />
            ))}
          </VuiBox>
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default OtaOverview;

