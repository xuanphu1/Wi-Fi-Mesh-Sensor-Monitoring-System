import { useMemo, useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import InputAdornment from "@mui/material/InputAdornment";
import TextField from "@mui/material/TextField";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { mockOta } from "api/mockData";
import {
  IoCloudUpload,
  IoHardwareChip,
  IoPushOutline,
  IoDocumentText,
  IoSearch,
  IoFilterOutline,
  IoCube,
  IoCopyOutline,
  IoEllipsisVertical,
  IoChevronBack,
  IoChevronForward
} from "react-icons/io5";

const searchInputSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    background: "rgba(15, 18, 42, 0.4) !important",
    color: "#ffffff !important",
    height: "40px",
    border: "1px solid rgba(255, 255, 255, 0.1) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#4318ff !important", border: "1px solid #4318ff !important" },
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    padding: "0 14px !important",
    fontSize: "14px",
    "&::placeholder": { color: "rgba(255,255,255,0.3)", opacity: 1 }
  },
};

function FwRow({ fw }) {
  const isStable = fw.channel === "stable";
  const channelColor = isStable ? "#01f7a7" : "#ffb547";
  const channelBg = isStable ? "rgba(1,247,167,0.15)" : "rgba(255,181,71,0.15)";
  
  const uploadDate = new Date(fw.uploadedIso);
  const dateStr = uploadDate.toLocaleDateString();
  const timeStr = uploadDate.toLocaleTimeString();

  return (
    <VuiBox sx={{ background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)", borderRadius: "16px", border: "1px solid rgba(255,255,255,0.05)", mb: 1.5, p: 2, display: "flex", alignItems: "center", transition: "all 0.3s", "&:hover": { transform: "translateY(-2px)", boxShadow: "0 8px 24px rgba(0,0,0,0.3)", border: "1px solid rgba(255,255,255,0.1)" } }}>
      <Grid container alignItems="center">
        <Grid item xs={1.5}>
          <VuiTypography variant="button" color="text" fontWeight="bold">{fw.id}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiBox display="flex" alignItems="center" gap={1.5}>
            <VuiBox sx={{ width: 28, height: 28, borderRadius: "8px", background: "rgba(138,44,255,0.2)", color: "#8a2cff", display: "grid", placeItems: "center" }}>
              <IoCube size="14px" />
            </VuiBox>
            <VuiTypography variant="button" color="white" fontWeight="bold">{fw.device.toUpperCase()}</VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={1.5}>
          <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: "rgba(0,117,255,0.15)", display: "inline-block" }}>
            <VuiTypography variant="caption" fontWeight="bold" sx={{ color: "#0075ff" }}>{fw.version}</VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={2}>
          <VuiBox display="flex" alignItems="center" gap={1}>
            <VuiTypography variant="button" color="text">{fw.checksum}</VuiTypography>
            <IoCopyOutline color="rgba(255,255,255,0.3)" size="14px" style={{ cursor: "pointer" }} />
          </VuiBox>
        </Grid>
        <Grid item xs={1.5}>
          <VuiTypography variant="caption" color="text" display="block">{dateStr}</VuiTypography>
          <VuiTypography variant="caption" color="text" display="block">{timeStr}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", border: `1px solid ${channelColor}`, background: channelBg, display: "inline-block" }}>
            <VuiTypography variant="caption" fontWeight="bold" sx={{ color: channelColor }}>{fw.channel.toUpperCase()}</VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={1.5}>
          <VuiTypography variant="button" color="text">{fw.notes || "-"}</VuiTypography>
        </Grid>
        <Grid item xs={1}>
          <VuiBox display="flex" justifyContent="flex-end">
            <VuiBox sx={{ width: 32, height: 32, borderRadius: "50%", background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)", display: "grid", placeItems: "center", cursor: "pointer", transition: "all 0.2s", "&:hover": { background: "rgba(255,255,255,0.1)" } }}>
              <IoEllipsisVertical color="rgba(255,255,255,0.6)" size="14px" />
            </VuiBox>
          </VuiBox>
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function FirmwareManagement() {
  const [page, setPage] = useState(1);
  const rowsPerPage = 5;

  const totalFws = mockOta.firmwares.length;
  
  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        
        {/* UPLOAD FIRMWARE CARD */}
        <Card sx={{
          padding: "24px 24px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start" mb={3}>
            <VuiBox display="flex" alignItems="center" gap={2}>
              <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoCloudUpload size="22px" />
              </VuiBox>
              <VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold">Upload firmware</VuiTypography>
                <VuiTypography variant="caption" color="text">We'll connect upload API • checksum • release notes.</VuiTypography>
              </VuiBox>
            </VuiBox>
            <Button variant="outlined" color="info" size="small" sx={{ borderRadius: "8px", borderColor: "rgba(0,117,255,0.5)", px: 2 }}>
              <IoPushOutline style={{ marginRight: 8 }} size="16px" /> Browse files
            </Button>
          </VuiBox>
          
          <VuiBox sx={{
            border: "2px dashed rgba(255,255,255,0.1)",
            borderRadius: "16px",
            background: "rgba(255,255,255,0.02)",
            p: 4,
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            transition: "all 0.2s",
            cursor: "pointer",
            "&:hover": { borderColor: "rgba(0,117,255,0.4)", background: "rgba(0,117,255,0.05)" }
          }}>
            <VuiBox display="flex" alignItems="center" gap={3}>
              <VuiBox sx={{ width: 64, height: 64, borderRadius: "16px", background: "rgba(0,117,255,0.15)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoDocumentText size="32px" />
              </VuiBox>
              <VuiBox>
                <VuiTypography variant="button" color="white" fontWeight="medium" display="block">Drag and drop firmware file here</VuiTypography>
                <VuiTypography variant="button" color="text">or click to browse</VuiTypography>
              </VuiBox>
            </VuiBox>
            <VuiBox textAlign="right">
              <VuiTypography variant="caption" color="text" display="block" mb={1}>Supported file types</VuiTypography>
              <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: "rgba(255,255,255,0.1)", display: "inline-block", mb: 2 }}>
                <VuiTypography variant="caption" color="white" fontWeight="bold">BIN</VuiTypography>
              </VuiBox>
              <VuiTypography variant="caption" color="text" display="block">Max file size: 50 MB</VuiTypography>
            </VuiBox>
          </VuiBox>
        </Card>

        {/* FIRMWARE MANAGEMENT CARD */}
        <Card sx={{
          padding: "24px 20px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)"
        }}>
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={4}>
            <VuiBox display="flex" alignItems="center" gap={2}>
              <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoHardwareChip size="22px" />
              </VuiBox>
              <VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold">Firmware Management</VuiTypography>
                <VuiTypography variant="caption" color="text">Manage and track all firmware versions</VuiTypography>
              </VuiBox>
            </VuiBox>
            
            <VuiBox display="flex" gap={2} alignItems="center">
              <TextField
                variant="outlined"
                placeholder="Search firmware..."
                sx={searchInputSx}
                InputProps={{
                  endAdornment: (
                    <InputAdornment position="end">
                      <IoSearch color="rgba(255,255,255,0.4)" size="16px" />
                    </InputAdornment>
                  ),
                }}
              />
              <VuiBox sx={{ width: 40, height: 40, borderRadius: "12px", border: "1px solid rgba(255,255,255,0.1)", display: "grid", placeItems: "center", cursor: "pointer", transition: "all 0.2s", "&:hover": { background: "rgba(255,255,255,0.05)" } }}>
                <IoFilterOutline color="#fff" size="18px" />
              </VuiBox>
            </VuiBox>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">ID</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">DEVICE</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">VERSION</VuiTypography></Grid>
            <Grid item xs={2}><VuiTypography variant="caption" color="text" fontWeight="medium">CHECKSUM</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">UPLOADED</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">CHANNEL</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">NOTES</VuiTypography></Grid>
            <Grid item xs={1} textAlign="right"><VuiTypography variant="caption" color="text" fontWeight="medium">ACTIONS</VuiTypography></Grid>
          </Grid>

          <VuiBox>
            {mockOta.firmwares.map((fw, index) => (
              <FwRow key={index} fw={fw} />
            ))}
          </VuiBox>

          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mt={3} px={1}>
            <VuiTypography variant="caption" color="text">
              Showing 1 to {totalFws} of {totalFws} firmware
            </VuiTypography>
            <VuiBox display="flex" gap={1}>
              <VuiBox sx={{ width: 32, height: 32, borderRadius: "8px", background: "rgba(255,255,255,0.05)", display: "grid", placeItems: "center", cursor: "pointer" }}>
                <IoChevronBack color="rgba(255,255,255,0.4)" size="14px" />
              </VuiBox>
              <VuiBox sx={{ width: 32, height: 32, borderRadius: "8px", background: "#4318ff", display: "grid", placeItems: "center", cursor: "pointer" }}>
                <VuiTypography variant="caption" color="white" fontWeight="bold">1</VuiTypography>
              </VuiBox>
              <VuiBox sx={{ width: 32, height: 32, borderRadius: "8px", background: "rgba(255,255,255,0.05)", display: "grid", placeItems: "center", cursor: "pointer" }}>
                <IoChevronForward color="rgba(255,255,255,0.4)" size="14px" />
              </VuiBox>
            </VuiBox>
          </VuiBox>
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default FirmwareManagement;

