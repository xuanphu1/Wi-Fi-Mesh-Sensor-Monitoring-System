import { useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import TextField from "@mui/material/TextField";
import Button from "@mui/material/Button";
import Switch from "@mui/material/Switch";
import Checkbox from "@mui/material/Checkbox";
import FormControlLabel from "@mui/material/FormControlLabel";
import LinearProgress from "@mui/material/LinearProgress";
import MenuItem from "@mui/material/MenuItem";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import {
  IoSaveOutline,
  IoReloadOutline,
  IoTimeOutline,
  IoCalendarOutline,
  IoGlobeOutline,
  IoHardwareChipOutline,
  IoLayersOutline,
  IoPowerOutline,
  IoServerOutline,
  IoDocumentTextOutline,
  IoReloadCircleOutline,
  IoColorWandOutline,
  IoDownloadOutline,
  IoScanCircleOutline,
  IoMailOutline,
  IoSettingsOutline,
  IoCheckmarkCircle
} from "react-icons/io5";

// Custom Input styling for the solid white inputs
const solidInputSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "8px !important",
    background: "#ffffff !important",
    color: "#000000 !important",
    height: "44px",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused fieldset": { borderColor: "#4318ff !important", borderWidth: "2px !important" },
  },
  "& .MuiInputBase-input": {
    color: "#000000 !important",
    padding: "0 14px !important",
    fontSize: "14px",
    fontWeight: "bold",
  },
};

const actionBoxSx = {
  p: 3,
  border: "1px solid rgba(255, 255, 255, 0.05)",
  borderRadius: "16px",
  background: "rgba(255, 255, 255, 0.02)",
  height: "100%",
  display: "flex",
  flexDirection: "column",
  justifyContent: "space-between",
  transition: "all 0.3s",
  "&:hover": {
    background: "rgba(255, 255, 255, 0.05)",
    border: "1px solid rgba(255, 255, 255, 0.1)"
  }
};

const SENSOR_METRICS = {
  0: [ // SENSOR_BME280
    { key: "temp", label: "Temperature threshold (°C)" },
    { key: "hum", label: "Humidity threshold (%)" },
    { key: "press", label: "Pressure threshold (hPa)" },
  ],
  1: [ // SENSOR_MHZ14A
    { key: "co2", label: "CO₂ threshold (ppm)" },
  ],
  2: [ // SENSOR_PMS7003
    { key: "pm1", label: "PM1.0 threshold (µg/m³)" },
    { key: "pm25", label: "PM2.5 threshold (µg/m³)" },
    { key: "pm10", label: "PM10 threshold (µg/m³)" },
  ],
  3: [ // SENSOR_DHT22
    { key: "temp", label: "Temperature threshold (°C)" },
    { key: "hum", label: "Humidity threshold (%)" },
  ],
  4: [ // SENSOR_AHT10
    { key: "temp", label: "Temperature threshold (°C)" },
    { key: "hum", label: "Humidity threshold (%)" },
  ],
};

function SystemSettings() {
  const [systemTimeout, setSystemTimeout] = useState(60);
  const [refreshMs, setRefreshMs] = useState(1000);
  
  // Sensor type state
  const [selectedSensor, setSelectedSensor] = useState(1);
  const [sensorThresholds, setSensorThresholds] = useState({
    0: { temp: 35, hum: 80, press: 1013 },
    1: { co2: 800 },
    2: { pm1: 35, pm25: 35, pm10: 50 },
    3: { temp: 35, hum: 80 },
    4: { temp: 35, hum: 80 },
  });

  const [emailEnabled, setEmailEnabled] = useState(true);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        
        {/* System Settings Card */}
        <Card sx={{
          padding: "24px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <VuiTypography variant="h6" color="white" fontWeight="bold" mb={3}>
            System Settings
          </VuiTypography>

          <Grid container spacing={3} mb={3}>
            <Grid item xs={12} md={6}>
              <VuiTypography variant="caption" color="text" display="block" mb={1}>
                System timeout (s)
              </VuiTypography>
              <TextField
                fullWidth
                type="number"
                value={systemTimeout}
                onChange={(e) => setSystemTimeout(Number(e.target.value))}
                sx={solidInputSx}
              />
            </Grid>
            <Grid item xs={12} md={6}>
              <VuiTypography variant="caption" color="text" display="block" mb={1}>
                Web refresh interval (ms)
              </VuiTypography>
              <TextField
                fullWidth
                type="number"
                value={refreshMs}
                onChange={(e) => setRefreshMs(Number(e.target.value))}
                sx={solidInputSx}
              />
            </Grid>

            {/* Sensor Thresholds */}
            <Grid item xs={12} md={12} mt={1}>
              <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                Sensor Threshold Configuration
              </VuiTypography>
            </Grid>
            <Grid item xs={12} md={12}>
              <VuiTypography variant="caption" color="text" display="block" mb={1}>
                Select Sensor
              </VuiTypography>
              <TextField
                select
                fullWidth
                value={selectedSensor}
                onChange={(e) => setSelectedSensor(Number(e.target.value))}
                sx={solidInputSx}
                SelectProps={{
                  MenuProps: {
                    sx: { "& .MuiPaper-root": { background: "#0f1535", color: "white" } }
                  }
                }}
              >
                <MenuItem value={0}>SENSOR_BME280</MenuItem>
                <MenuItem value={1}>SENSOR_MHZ14A</MenuItem>
                <MenuItem value={2}>SENSOR_PMS7003</MenuItem>
                <MenuItem value={3}>SENSOR_DHT22</MenuItem>
                <MenuItem value={4}>SENSOR_AHT10</MenuItem>
              </TextField>
            </Grid>

            {SENSOR_METRICS[selectedSensor].map((metric) => (
              <Grid item xs={12} md={4} key={metric.key}>
                <VuiTypography variant="caption" color="text" display="block" mb={1}>
                  {metric.label}
                </VuiTypography>
                <TextField
                  fullWidth
                  type="number"
                  value={sensorThresholds[selectedSensor][metric.key]}
                  onChange={(e) => {
                    const val = Number(e.target.value);
                    setSensorThresholds(prev => ({
                      ...prev,
                      [selectedSensor]: {
                        ...prev[selectedSensor],
                        [metric.key]: val
                      }
                    }));
                  }}
                  sx={solidInputSx}
                />
              </Grid>
            ))}
          </Grid>

          <VuiBox display="flex" gap={2}>
            <Button variant="contained" color="primary" sx={{ background: "#4318ff", "&:hover": { background: "#3311cc" }, px: 3, borderRadius: "8px" }}>
              <IoSaveOutline style={{ marginRight: 8 }} size="16px" /> Save changes
            </Button>
            <Button variant="outlined" color="white" sx={{ borderColor: "rgba(255,255,255,0.2)", color: "#fff", px: 3, borderRadius: "8px" }}>
              <IoReloadOutline style={{ marginRight: 8 }} size="16px" /> Reset to default
            </Button>
          </VuiBox>
        </Card>

        {/* System Information Card */}
        <Card sx={{
          padding: "24px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <VuiTypography variant="h6" color="white" fontWeight="bold" mb={4}>
            System Information
          </VuiTypography>

          <Grid container spacing={4} mb={4}>
            {[
              { label: "System Uptime", value: "4h 4m 48s", icon: <IoTimeOutline size="20px" />, bg: "rgba(0,117,255,0.15)", color: "#0075ff" },
              { label: "System Time", value: "05/25/2026 10:23 PM", icon: <IoCalendarOutline size="20px" />, bg: "rgba(138,44,255,0.15)", color: "#8a2cff" },
              { label: "Timezone", value: "UTC+07:00 (Asia/Ho_Chi_Minh)", icon: <IoGlobeOutline size="20px" />, bg: "rgba(0,117,255,0.15)", color: "#0075ff" },
              { label: "Device ID", value: "MSMS-GW-0001", icon: <IoHardwareChipOutline size="20px" />, bg: "rgba(138,44,255,0.15)", color: "#8a2cff" },
              { label: "Firmware Version", value: "1.0.0", icon: <IoHardwareChipOutline size="20px" />, bg: "rgba(0,117,255,0.15)", color: "#0075ff" },
              { label: "Hardware Version", value: "v2.1", icon: <IoLayersOutline size="20px" />, bg: "rgba(138,44,255,0.15)", color: "#8a2cff" },
              { label: "Last Restart", value: "05/25/2026 06:18 PM", icon: <IoPowerOutline size="20px" />, bg: "rgba(1,247,167,0.15)", color: "#01f7a7" },
              { label: "Memory Usage", value: "32.4%", icon: <IoServerOutline size="20px" />, bg: "rgba(1,247,167,0.15)", color: "#01f7a7", isProgress: true },
            ].map((item, idx) => (
              <Grid item xs={12} sm={6} md={3} key={idx}>
                <VuiBox display="flex" gap={2}>
                  <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: item.bg, color: item.color, display: "grid", placeItems: "center", flexShrink: 0 }}>
                    {item.icon}
                  </VuiBox>
                  <VuiBox width="100%">
                    <VuiTypography variant="caption" color="text" display="block" mb={0.5}>{item.label}</VuiTypography>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">{item.value}</VuiTypography>
                    {item.isProgress && (
                      <VuiBox mt={1}>
                        <LinearProgress variant="determinate" value={32.4} sx={{
                          height: 4, borderRadius: 2, background: "rgba(255,255,255,0.1)",
                          "& .MuiLinearProgress-bar": { background: "#4318ff" }
                        }} />
                      </VuiBox>
                    )}
                  </VuiBox>
                </VuiBox>
              </Grid>
            ))}
          </Grid>

          <VuiBox display="flex" gap={2}>
            <Button variant="outlined" color="white" sx={{ borderColor: "rgba(255,255,255,0.2)", color: "#fff", px: 3, borderRadius: "8px" }}>
              <IoDocumentTextOutline style={{ marginRight: 8 }} size="16px" /> View system logs
            </Button>
            <Button 
              component="a" 
              href="http://meshgateway.local" 
              target="_blank"
              rel="noreferrer"
              variant="outlined" 
              color="info" 
              sx={{ borderColor: "rgba(0,117,255,0.5)", color: "#0075ff", px: 3, borderRadius: "8px" }}
            >
              <IoGlobeOutline style={{ marginRight: 8 }} size="16px" /> Gateway Config
            </Button>
          </VuiBox>
        </Card>

        {/* System Actions Card */}
        <Card sx={{
          padding: "24px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <VuiTypography variant="h6" color="white" fontWeight="bold" mb={3}>
            System Actions
          </VuiTypography>

          <Grid container spacing={3}>
            <Grid item xs={12} sm={6} md={3}>
              <VuiBox sx={actionBoxSx}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1.5} mb={2}>
                    <IoReloadCircleOutline size="24px" color="#4318ff" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">Restart System</VuiTypography>
                  </VuiBox>
                  <VuiTypography variant="caption" color="text" display="block" mb={3}>
                    Restart the gateway system safely.
                  </VuiTypography>
                </VuiBox>
                <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(0,117,255,0.5)", width: "100px" }}>Restart</Button>
              </VuiBox>
            </Grid>
            
            <Grid item xs={12} sm={6} md={3}>
              <VuiBox sx={actionBoxSx}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1.5} mb={2}>
                    <IoColorWandOutline size="24px" color="#4318ff" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">Clear Cache</VuiTypography>
                  </VuiBox>
                  <VuiTypography variant="caption" color="text" display="block" mb={3}>
                    Clear temporary cache and old data.
                  </VuiTypography>
                </VuiBox>
                <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(0,117,255,0.5)", width: "100px" }}>Clear</Button>
              </VuiBox>
            </Grid>

            <Grid item xs={12} sm={6} md={3}>
              <VuiBox sx={actionBoxSx}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1.5} mb={2}>
                    <IoDownloadOutline size="24px" color="#8a2cff" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">Backup Settings</VuiTypography>
                  </VuiBox>
                  <VuiTypography variant="caption" color="text" display="block" mb={3}>
                    Download a backup of current system settings.
                  </VuiTypography>
                </VuiBox>
                <Button variant="outlined" color="info" size="small" sx={{ borderColor: "rgba(138,44,255,0.5)", color: "#8a2cff", width: "100px" }}>Backup</Button>
              </VuiBox>
            </Grid>

            <Grid item xs={12} sm={6} md={3}>
              <VuiBox sx={actionBoxSx}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1.5} mb={2}>
                    <IoScanCircleOutline size="24px" color="#ff285c" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">Factory Reset</VuiTypography>
                  </VuiBox>
                  <VuiTypography variant="caption" color="text" display="block" mb={3}>
                    Reset all settings to factory defaults.
                  </VuiTypography>
                </VuiBox>
                <Button variant="outlined" color="error" size="small" sx={{ borderColor: "rgba(255,40,92,0.5)", color: "#ff285c", width: "100px" }}>Reset</Button>
              </VuiBox>
            </Grid>
          </Grid>
        </Card>

        {/* Notification Preferences Card */}
        <Card sx={{
          padding: "24px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)"
        }}>
          <VuiTypography variant="h6" color="white" fontWeight="bold" mb={3}>
            Notification Preferences
          </VuiTypography>

          <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start">
            <VuiBox display="flex" gap={2}>
              <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(138,44,255,0.15)", color: "#8a2cff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                <IoMailOutline size="22px" />
              </VuiBox>
              <VuiBox>
                <VuiTypography variant="button" color="white" fontWeight="bold" display="block">Email notifications</VuiTypography>
                <VuiTypography variant="caption" color="text" display="block" mb={2}>Receive system alerts and OTA status via email.</VuiTypography>
                
                <VuiBox display="flex" alignItems="center" gap={2}>
                  <VuiTypography variant="caption" color="text">Alert severity</VuiTypography>
                  <FormControlLabel
                    control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#4318ff" } }} />}
                    label={<VuiTypography variant="caption" color="white">Info</VuiTypography>}
                  />
                  <FormControlLabel
                    control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#4318ff" } }} />}
                    label={<VuiTypography variant="caption" color="white">Warning</VuiTypography>}
                  />
                  <FormControlLabel
                    control={<Checkbox defaultChecked sx={{ color: "rgba(255,255,255,0.3)", "&.Mui-checked": { color: "#4318ff" } }} />}
                    label={<VuiTypography variant="caption" color="white">Error</VuiTypography>}
                  />
                </VuiBox>
              </VuiBox>
            </VuiBox>

            <VuiBox display="flex" flexDirection="column" alignItems="flex-end" gap={3}>
              <Switch checked={emailEnabled} onChange={() => setEmailEnabled(!emailEnabled)} color="primary" />
              <Button variant="outlined" color="white" sx={{ borderColor: "rgba(255,255,255,0.2)", color: "#fff", borderRadius: "8px" }}>
                <IoSettingsOutline style={{ marginRight: 8 }} size="16px" /> Configure email
              </Button>
            </VuiBox>
          </VuiBox>
        </Card>

      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default SystemSettings;

