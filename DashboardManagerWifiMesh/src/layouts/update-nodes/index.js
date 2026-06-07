import { useMemo, useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import Button from "@mui/material/Button";
import InputAdornment from "@mui/material/InputAdornment";
import InputLabel from "@mui/material/InputLabel";
import FormControl from "@mui/material/FormControl";
import Select from "@mui/material/Select";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { mockOta, mockNodes } from "api/mockData";
import {
  IoRocket,
  IoCloudUpload,
  IoHardwareChip,
  IoReload,
  IoDocumentText,
  IoTrendingUp,
  IoCheckmark,
  IoCheckmarkCircle,
  IoCloseCircle,
  IoTime,
  IoEye,
  IoChevronDown,
  IoChevronBack,
  IoChevronForward,
  IoCloudDownloadOutline
} from "react-icons/io5";

// Mock data for recent jobs
const recentJobs = [
  { id: "JOB-20260525-001", fw: "1.0.0 (stable)", mode: "Multiple nodes", by: "admin", start: "05/25/2026 10:23 PM", status: "Completed", sum: "3/3 success", sumColor: "#01f7a7" },
  { id: "JOB-20260524-002", fw: "1.0.1 (beta)", mode: "Multiple nodes", by: "admin", start: "05/24/2026 08:41 PM", status: "Completed", sum: "2/3 success", sumColor: "#01f7a7" },
  { id: "JOB-20260524-001", fw: "1.0.0 (stable)", mode: "Single node", by: "admin", start: "05/24/2026 05:32 PM", status: "Failed", sum: "0/1 success", sumColor: "#ff285c" },
  { id: "JOB-20260523-003", fw: "1.0.0 (stable)", mode: "Multiple nodes", by: "admin", start: "05/23/2026 11:15 AM", status: "Completed", sum: "5/5 success", sumColor: "#01f7a7" },
  { id: "JOB-20260523-002", fw: "1.0.1 (beta)", mode: "Multiple nodes", by: "admin", start: "05/23/2026 09:02 AM", status: "In progress", sum: "1/4 success", sumColor: "#0075ff" },
];

const fotaSelectSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    background: "rgba(15, 18, 42, 0.4) !important",
    color: "#ffffff !important",
    height: "56px",
    border: "1px solid rgba(255, 255, 255, 0.1) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#4318ff !important", border: "1px solid #4318ff !important" },
  },
  "& .MuiInputLabel-root": {
    color: "rgba(255, 255, 255, 0.45) !important",
    "&.Mui-focused, &.MuiInputLabel-shrink": {
      color: "#ffffff !important",
      transform: "translate(14px, 8px) scale(0.85) !important",
    },
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    padding: "20px 40px 8px 14px !important",
  },
};

function FotaRow({ job }) {
  const isCompleted = job.status === "Completed";
  const isFailed = job.status === "Failed";
  const isInProgress = job.status === "In progress";
  
  const statusColor = isCompleted ? "#01f7a7" : isFailed ? "#ff285c" : "#0075ff";
  const statusBg = isCompleted ? "rgba(1,247,167,0.15)" : isFailed ? "rgba(255,40,92,0.15)" : "rgba(0,117,255,0.15)";
  const StatusIcon = isCompleted ? IoCheckmarkCircle : isFailed ? IoCloseCircle : IoReload;

  return (
    <VuiBox sx={{ background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)", borderRadius: "16px", border: "1px solid rgba(255,255,255,0.05)", mb: 1.5, p: 2, display: "flex", alignItems: "center", transition: "all 0.3s", "&:hover": { transform: "translateY(-2px)", boxShadow: "0 8px 24px rgba(0,0,0,0.3)", border: "1px solid rgba(255,255,255,0.1)" } }}>
      <Grid container alignItems="center">
        <Grid item xs={2}>
          <VuiTypography variant="button" color="text" fontWeight="bold">{job.id}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiTypography variant="button" color="white" fontWeight="bold">{job.fw}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiTypography variant="button" color="text">{job.mode}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiTypography variant="button" color="text">{job.by}</VuiTypography>
        </Grid>
        <Grid item xs={2}>
          <VuiTypography variant="button" color="text">{job.start}</VuiTypography>
        </Grid>
        <Grid item xs={1.5}>
          <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: statusBg, display: "inline-flex", alignItems: "center", gap: 1 }}>
            <StatusIcon color={statusColor} size="14px" />
            <VuiTypography variant="caption" fontWeight="bold" sx={{ color: statusColor }}>{job.status}</VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={1}>
          <VuiTypography variant="button" fontWeight="bold" sx={{ color: job.sumColor }}>{job.sum}</VuiTypography>
        </Grid>
        <Grid item xs={1} textAlign="right">
          <VuiBox display="flex" justifyContent="flex-end">
            <VuiBox sx={{ width: 32, height: 32, borderRadius: "50%", background: "rgba(0,117,255,0.15)", color: "#0075ff", display: "grid", placeItems: "center", cursor: "pointer", transition: "all 0.2s", "&:hover": { background: "rgba(0,117,255,0.3)" } }}>
              <IoEye size="14px" />
            </VuiBox>
          </VuiBox>
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function Fota() {
  const [firmwareId, setFirmwareId] = useState("1.0.0");
  const [mode, setMode] = useState("multi");

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <Grid container spacing={3} mb={3}>
          <Grid item xs={12} lg={8}>
            <Card sx={{
              padding: "24px 24px",
              background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
              border: "1px solid rgba(255, 255, 255, 0.08)",
              boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
              borderRadius: "16px",
              backdropFilter: "blur(42px)",
              height: "100%"
            }}>
              <VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold" mb="6px">
                  FOTA
                </VuiTypography>
                <VuiTypography variant="caption" color="text" mb="24px" display="block">
                  Select firmware and update mode to start OTA process.
                </VuiTypography>

                <Grid container spacing={3} mb={4}>
                  <Grid item xs={12} md={6}>
                    <FormControl fullWidth variant="outlined" sx={fotaSelectSx}>
                      <InputLabel id="fw-label" shrink>Select firmware</InputLabel>
                      <Select
                        labelId="fw-label"
                        value={firmwareId}
                        onChange={(e) => setFirmwareId(e.target.value)}
                        IconComponent={(props) => (
                          <div {...props} style={{ right: 14, position: "absolute", pointerEvents: "none", display: "flex", color: "rgba(255,255,255,0.4)" }}>
                            <IoChevronDown size="16px" />
                          </div>
                        )}
                      >
                        <MenuItem value="1.0.0">1.0.0 (stable) <span style={{marginLeft: 8, padding: "2px 6px", background: "rgba(138,44,255,0.2)", color: "#8a2cff", borderRadius: 4, fontSize: 10, fontWeight: "bold"}}>Latest</span></MenuItem>
                        <MenuItem value="0.9.4">0.9.4 (stable)</MenuItem>
                      </Select>
                    </FormControl>
                  </Grid>
                  <Grid item xs={12} md={6}>
                    <FormControl fullWidth variant="outlined" sx={fotaSelectSx}>
                      <InputLabel id="mode-label" shrink>Mode</InputLabel>
                      <Select
                        labelId="mode-label"
                        value={mode}
                        onChange={(e) => setMode(e.target.value)}
                        IconComponent={(props) => (
                          <div {...props} style={{ right: 14, position: "absolute", pointerEvents: "none", display: "flex", color: "rgba(255,255,255,0.4)" }}>
                            <IoChevronDown size="16px" />
                          </div>
                        )}
                      >
                        <MenuItem value="root">Root Node</MenuItem>
                        <MenuItem value="single">Single Node</MenuItem>
                        <MenuItem value="multi">Multiple Nodes</MenuItem>
                      </Select>
                    </FormControl>
                  </Grid>
                </Grid>

                <VuiBox display="flex" gap={2} mb={5}>
                  <Button variant="contained" color="primary" sx={{ background: "#4318ff", "&:hover": { background: "#3311cc" }, px: 3 }}>
                    <IoRocket style={{ marginRight: 8 }} size="16px" /> Start OTA (demo)
                  </Button>
                  <Button variant="outlined" color="white" sx={{ borderColor: "rgba(255,255,255,0.2)", color: "#fff", px: 3 }}>
                    Cancel (demo)
                  </Button>
                </VuiBox>

                <VuiBox>
                  <VuiTypography variant="caption" color="text" display="block" mb={3}>
                    Progress states (placeholder):
                  </VuiTypography>
                  <VuiBox display="flex" justifyContent="space-between" alignItems="center" position="relative" sx={{ maxWidth: "500px" }}>
                    {/* Dashed line */}
                    <VuiBox sx={{ position: "absolute", top: "20px", left: "20px", right: "20px", height: "1px", borderTop: "1px dashed rgba(255,255,255,0.2)", zIndex: 0 }} />
                    
                    {/* States */}
                    {[
                      { icon: <IoCloudUpload size="18px" />, label: "Queued", active: true },
                      { icon: <IoCloudDownloadOutline size="18px" />, label: "Downloading", active: false },
                      { icon: <IoHardwareChip size="18px" />, label: "Flashing", active: false },
                      { icon: <IoReload size="18px" />, label: "Reboot", active: false },
                      { icon: <IoCheckmark size="18px" />, label: "Success / Fail", active: false }
                    ].map((step, idx) => (
                      <VuiBox key={idx} display="flex" flexDirection="column" alignItems="center" zIndex={1} sx={{ width: "80px" }}>
                        <VuiBox sx={{ 
                          width: 40, height: 40, borderRadius: "50%", 
                          background: step.active ? "linear-gradient(127deg, #4318ff 0%, #8a2cff 100%)" : "rgba(15,18,42,0.8)",
                          border: step.active ? "none" : "1px solid rgba(255,255,255,0.1)",
                          color: step.active ? "#fff" : "rgba(255,255,255,0.5)",
                          display: "grid", placeItems: "center", mb: 1,
                          boxShadow: step.active ? "0 0 16px rgba(138,44,255,0.5)" : "none"
                        }}>
                          {step.icon}
                        </VuiBox>
                        <VuiTypography variant="caption" sx={{ color: step.active ? "#8a2cff" : "rgba(255,255,255,0.5)", fontWeight: step.active ? "bold" : "normal", textAlign: "center" }}>
                          {step.label}
                        </VuiTypography>
                      </VuiBox>
                    ))}
                  </VuiBox>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
          
          <Grid item xs={12} lg={4}>
            <Card sx={{
              padding: "24px 20px",
              background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
              border: "1px solid rgba(255, 255, 255, 0.08)",
              boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
              borderRadius: "16px",
              backdropFilter: "blur(42px)",
              height: "100%"
            }}>
              <VuiBox display="flex" flexDirection="column" gap={4}>
                <VuiBox display="flex" gap={2}>
                  <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: "rgba(138,44,255,0.2)", color: "#8a2cff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                    <IoDocumentText size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">Notes</VuiTypography>
                    <VuiTypography variant="caption" color="text">We'll connect the API to show per-node realtime progress, retries, and OTA logs.</VuiTypography>
                  </VuiBox>
                </VuiBox>
                
                <VuiBox display="flex" gap={2}>
                  <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                    <IoTrendingUp size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">Realtime progress</VuiTypography>
                    <VuiTypography variant="caption" color="text">Per-node status and percentage</VuiTypography>
                  </VuiBox>
                </VuiBox>

                <VuiBox display="flex" gap={2}>
                  <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                    <IoReload size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">Auto retry</VuiTypography>
                    <VuiTypography variant="caption" color="text">Failed nodes will be retried</VuiTypography>
                  </VuiBox>
                </VuiBox>

                <VuiBox display="flex" gap={2}>
                  <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: "rgba(138,44,255,0.2)", color: "#8a2cff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                    <IoDocumentText size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">OTA logs</VuiTypography>
                    <VuiTypography variant="caption" color="text">View detailed logs and errors</VuiTypography>
                  </VuiBox>
                </VuiBox>
              </VuiBox>
            </Card>
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
          <VuiBox display="flex" alignItems="center" gap={1.5} mb={4}>
            <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0,117,255,0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
              <IoTime size="18px" />
            </VuiBox>
            <VuiTypography variant="h5" color="white" fontWeight="bold">
              Recent update jobs
            </VuiTypography>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={2}><VuiTypography variant="caption" color="text" fontWeight="medium">JOB ID</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">FIRMWARE</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">MODE</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">INITIATED BY</VuiTypography></Grid>
            <Grid item xs={2}><VuiTypography variant="caption" color="text" fontWeight="medium">STARTED AT</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">STATUS</VuiTypography></Grid>
            <Grid item xs={1}><VuiTypography variant="caption" color="text" fontWeight="medium">SUMMARY</VuiTypography></Grid>
            <Grid item xs={1} textAlign="right"><VuiTypography variant="caption" color="text" fontWeight="medium">ACTIONS</VuiTypography></Grid>
          </Grid>

          <VuiBox>
            {recentJobs.map((job, index) => (
              <FotaRow key={index} job={job} />
            ))}
          </VuiBox>

          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mt={3} px={1}>
            <VuiTypography variant="caption" color="text">
              Showing 1 to 5 of 12 jobs
            </VuiTypography>
            <Button variant="outlined" color="white" size="small" sx={{ borderRadius: "8px", borderColor: "rgba(138,44,255,0.5)", color: "#fff" }}>
              View all jobs <IoChevronForward style={{ marginLeft: 6 }} />
            </Button>
          </VuiBox>
        </Card>

      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Fota;

