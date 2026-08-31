import React, { useState, useEffect, useCallback, useMemo, useRef } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import LinearProgress from "@mui/material/LinearProgress";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import Select from "@mui/material/Select";
import IconButton from "@mui/material/IconButton";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";
import { useDashboardRealtime } from "hooks/useDashboardRealtime";
import { getHttpUrl, getWebSocketUrl } from "utils/wsConfig";

import {
  IoWifi,
  IoHardwareChip,
  IoCloudUpload,
  IoCheckmarkCircle,
  IoCloseCircle,
  IoReload,
  IoTime,
  IoDocumentText,
  IoServer,
  IoShieldCheckmark,
  IoArrowUpCircle,
  IoChevronDown,
  IoTrashOutline,
  IoCube,
  IoSend,
  IoFlash,
  IoArrowForward,
  IoDownload,
  IoClose
} from "react-icons/io5";

const cardSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.10)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
  borderRadius: "16px",
  backdropFilter: "blur(18px)",
  height: "100%",
  p: 3,
};

const formControlSx = {
  width: "100%",
  "& .MuiOutlinedInput-root": {
    borderRadius: "14px !important",
    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.35) 0%, rgba(10, 14, 35, 0.25) 100%) !important",
    backdropFilter: "blur(18px)",
    color: "#ffffff !important",
    height: "48px !important",
    minHeight: "48px !important",
    maxHeight: "48px !important",
    border: "1px solid rgba(255, 255, 255, 0.12) !important",
    boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    boxSizing: "border-box",
    "& fieldset": { border: "none !important" },
    "&:hover fieldset": { border: "none !important" },
    "&.Mui-focused": { border: "1px solid #0075ff !important" },
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    fontSize: "14px !important",
    fontWeight: "500",
    padding: "0 16px !important",
    height: "48px !important",
    display: "flex",
    alignItems: "center",
    boxSizing: "border-box !important",
    "&::placeholder": { color: "rgba(255, 255, 255, 0.50)", opacity: 1 }
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    fontSize: "14px !important",
    fontWeight: "500",
    padding: "0 40px 0 16px !important",
    height: "48px !important",
    display: "flex !important",
    alignItems: "center !important",
    boxSizing: "border-box !important",
  },
  "& .MuiSelect-icon": {
    color: "rgba(255, 255, 255, 0.70) !important",
    right: "14px !important",
    top: "calc(50% - 8px) !important",
    position: "absolute",
    pointerEvents: "none",
  }
};

const selectMenuProps = {
  PaperProps: {
    sx: {
      mt: 1,
      borderRadius: "12px",
      background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
      backdropFilter: "blur(20px)",
      border: "1px solid rgba(255, 255, 255, 0.12)",
      boxShadow: "0 8px 32px rgba(0, 0, 0, 0.55)",
      "& .MuiMenuItem-root": { color: "#fff", fontSize: "0.875rem", py: 1.2 },
      "& .MuiMenuItem-root:hover": { backgroundColor: "rgba(0, 117, 255, 0.22)" },
      "& .MuiMenuItem-root.Mui-selected": { backgroundColor: "rgba(255, 255, 255, 0.20)" },
    },
  },
};

function OtaGateway() {
  const { connected, deviceInfo } = useDashboardRealtime();
  const httpUrl = getHttpUrl();
  const wsUrl = getWebSocketUrl();

  // --- Step 1: Upload state ---
  const [selectedFile, setSelectedFile] = useState(null);
  const [uploadVersion, setUploadVersion] = useState("");
  const [channel, setChannel] = useState("stable");
  const [notes, setNotes] = useState("");
  const [uploading, setUploading] = useState(false);
  const [uploadMsg, setUploadMsg] = useState("");
  const [uploadError, setUploadError] = useState("");

  // --- Step 2: Trigger state ---
  const [selectedFirmwareId, setSelectedFirmwareId] = useState("");
  const [isTriggering, setIsTriggering] = useState(false);
  const [currentJobId, setCurrentJobId] = useState(null);
  const currentJobIdRef = useRef("");
  const otaTimeoutRef = useRef(null);
  const otaCompletedWaitingRebootRef = useRef(false);
  const autoDismissTimerRef = useRef(null);

  // --- Realtime OTA Progress State from Gateway ---
  const [liveOta, setLiveOta] = useState({
    active: false,
    jobId: "",
    status: "", // "Downloading", "Flashing", "Success", "Failed"
    percent: 0,
    bytes_read: 0,
    total_bytes: 0,
    running_partition: "",
    target_partition: "",
    message: "",
  });

  // --- SQLite Data ---
  const [firmwares, setFirmwares] = useState([]);
  const [historyJobs, setHistoryJobs] = useState([]);

  // Fetch firmwares from SQLite
  const loadFirmwares = useCallback(async () => {
    try {
      const res = await fetch(`${httpUrl}/api/ota/firmwares?targetType=gateway`);
      const data = await res.json();
      if (Array.isArray(data.firmwares)) {
        setFirmwares(data.firmwares);
        if (data.firmwares.length > 0 && !selectedFirmwareId) {
          setSelectedFirmwareId(data.firmwares[0].id);
        }
      }
    } catch (err) {
      console.error("[ota] Failed to load firmwares:", err.message);
    }
  }, [httpUrl, selectedFirmwareId]);

  // Fetch real OTA history from SQLite
  const loadJobs = useCallback(async () => {
    try {
      const res = await fetch(`${httpUrl}/api/ota/jobs?targetType=gateway&limit=50`);
      const data = await res.json();
      if (Array.isArray(data.jobs)) {
        setHistoryJobs(data.jobs);
      }
    } catch (err) {
      console.error("[ota] Failed to load jobs:", err.message);
    }
  }, [httpUrl]);

  useEffect(() => {
    loadFirmwares();
    loadJobs();
    const interval = setInterval(() => {
      loadJobs();
    }, 5000);
    return () => clearInterval(interval);
  }, [loadFirmwares, loadJobs]);

  // Clear timers on unmount
  useEffect(() => {
    return () => {
      if (otaTimeoutRef.current) clearTimeout(otaTimeoutRef.current);
      if (autoDismissTimerRef.current) clearTimeout(autoDismissTimerRef.current);
    };
  }, []);

  // Auto-dismiss the progress card when Gateway reconnects after successful OTA reboot
  useEffect(() => {
    if (connected && otaCompletedWaitingRebootRef.current) {
      const timer = setTimeout(() => {
        setLiveOta((prev) => ({ ...prev, active: false }));
        otaCompletedWaitingRebootRef.current = false;
        loadJobs();
      }, 2000); // 2s visual confirmation of reconnection
      return () => clearTimeout(timer);
    }
  }, [connected, loadJobs]);

  // Determine Active Gateway Firmware Version: ONLY when OTA has completed successfully or reported by device
  const activeGatewayVersion = useMemo(() => {
    // 1. Hardware live reported firmware version
    if (deviceInfo?.ver || deviceInfo?.version) {
      return `v${deviceInfo.ver || deviceInfo.version}`;
    }
    // 2. Most recent successfully completed OTA Job in SQLite
    const lastSuccessfulJob = historyJobs.find(
      (j) => j.targetType === "gateway" && (j.status === "Completed" || j.status === "Success")
    );
    if (lastSuccessfulJob && lastSuccessfulJob.version) {
      return `v${lastSuccessfulJob.version}`;
    }
    // 3. Initial baseline before any successful OTA execution
    return "v0.0.1 (Initial)";
  }, [deviceInfo, historyJobs]);

  // Listen to WebSocket OTA progress events from Gateway
  useEffect(() => {
    if (!wsUrl) return;
    let ws;
    try {
      ws = new WebSocket(wsUrl);
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          const activeJobId = currentJobIdRef.current;
          if (msg.type === "ota_progress" && (msg.target === "gateway" || (activeJobId && msg.jobId === activeJobId) || !msg.target)) {
            const pct = Number(msg.percent ?? msg.progress ?? 0);
            const status = msg.status || "In progress";
            const bytesRead = Number(msg.bytes_read || 0);
            const totalBytes = Number(msg.total_bytes || 0);
            const runningPart = msg.running_partition || "";
            const targetPart = msg.target_partition || "";
            const message = msg.message || msg.msg || "";

            // If Gateway has responded with progress/download, clear 5s timeout!
            if (status === "Downloading" || status === "Flashing" || status === "Success" || bytesRead > 0 || pct > 5) {
              if (otaTimeoutRef.current) {
                clearTimeout(otaTimeoutRef.current);
                otaTimeoutRef.current = null;
              }
            }

            setLiveOta({
              active: true,
              jobId: msg.jobId || activeJobId || "",
              status: status,
              percent: pct,
              bytes_read: bytesRead,
              total_bytes: totalBytes,
              running_partition: runningPart,
              target_partition: targetPart,
              message: message,
            });

            if (status === "Success" || status === "Completed" || pct >= 100) {
              setIsTriggering(false);
              otaCompletedWaitingRebootRef.current = true;
              if (otaTimeoutRef.current) clearTimeout(otaTimeoutRef.current);
              loadJobs();

              // Auto-dismiss after 6 seconds if reboot is complete
              if (autoDismissTimerRef.current) clearTimeout(autoDismissTimerRef.current);
              autoDismissTimerRef.current = setTimeout(() => {
                setLiveOta((prev) => ({ ...prev, active: false }));
                otaCompletedWaitingRebootRef.current = false;
              }, 6000);
            } else if (status === "Failed") {
              setIsTriggering(false);
              if (otaTimeoutRef.current) clearTimeout(otaTimeoutRef.current);
              loadJobs();
            }
          }
        } catch (e) {}
      };
    } catch (e) {}

    return () => {
      if (ws) ws.close();
    };
  }, [wsUrl, loadJobs]);

  const handleFileChange = (e) => {
    if (e.target.files && e.target.files[0]) {
      setSelectedFile(e.target.files[0]);
      setUploadMsg("");
      setUploadError("");
    }
  };

  // --- Step 1: Upload Firmware to SQLite ---
  const handleUploadFirmware = async () => {
    if (!selectedFile) {
      setUploadError("Please select a .bin firmware binary file!");
      return;
    }
    if (!uploadVersion.trim()) {
      setUploadError("Firmware Version is required (e.g. 1.0.1)!");
      return;
    }

    setUploading(true);
    setUploadMsg("Uploading and storing binary in SQLite database...");
    setUploadError("");

    try {
      const formData = new FormData();
      formData.append("file", selectedFile);
      formData.append("targetType", "gateway");
      formData.append("version", uploadVersion.trim());
      formData.append("channel", channel);
      formData.append("notes", notes.trim());

      const res = await fetch(`${httpUrl}/api/ota/upload`, {
        method: "POST",
        body: formData,
      });
      const data = await res.json();
      if (data.success && data.firmware) {
        setUploadMsg(`✓ Successfully stored: ${data.firmware.filename}`);
        setSelectedFile(null);
        setUploadVersion("");
        setNotes("");
        await loadFirmwares();
        setSelectedFirmwareId(data.firmware.id);
      } else {
        setUploadError(`Upload Error: ${data.error || "Failed to save"}`);
      }
    } catch (err) {
      setUploadError(`Upload Failed: ${err.message}`);
    } finally {
      setUploading(false);
    }
  };

  // --- Step 2: Trigger OTA Command to Gateway (with 5-second Timeout) ---
  const handleStartOta = async () => {
    const selectedFw = firmwares.find((f) => f.id === Number(selectedFirmwareId));
    if (!selectedFw) {
      alert("Please select a firmware version from the dropdown first.");
      return;
    }

    if (otaTimeoutRef.current) {
      clearTimeout(otaTimeoutRef.current);
      otaTimeoutRef.current = null;
    }
    if (autoDismissTimerRef.current) {
      clearTimeout(autoDismissTimerRef.current);
    }
    otaCompletedWaitingRebootRef.current = false;

    setIsTriggering(true);
    setLiveOta({
      active: true,
      jobId: "",
      status: "Initiating",
      percent: 5,
      bytes_read: 0,
      total_bytes: selectedFw.fileSize || 0,
      running_partition: "Detecting...",
      target_partition: "Detecting...",
      message: `Dispatching OTA Command to Gateway (URL: ${selectedFw.filename})...`,
    });

    try {
      const res = await fetch(`${httpUrl}/api/ota/trigger`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          targetType: "gateway",
          targetDetail: "gateway",
          firmwareId: selectedFw.id,
          version: selectedFw.version,
          filename: selectedFw.filename,
        }),
      });
      const data = await res.json();
      if (data.success && data.job) {
        const jobId = data.job.jobId;
        setCurrentJobId(jobId);
        currentJobIdRef.current = jobId;
        setLiveOta((prev) => ({
          ...prev,
          jobId: jobId,
          status: "Dispatched",
          message: `Command sent (${jobId}). Waiting for Gateway to respond (Timeout: 5s)...`,
        }));
        loadJobs();

        // ⏱️ Start 5-second watchdog timer: If Gateway doesn't send progress in 5s -> FAIL
        otaTimeoutRef.current = setTimeout(async () => {
          setIsTriggering(false);
          setLiveOta((prev) => {
            if (prev.status === "Dispatched" || prev.status === "Initiating") {
              return {
                ...prev,
                status: "Failed",
                percent: 0,
                message: "Timeout: No OTA response from Gateway after 5s. Gateway did not start download.",
              };
            }
            return prev;
          });

          // Update SQLite Job status to Failed
          try {
            await fetch(`${httpUrl}/api/ota/jobs/${jobId}/status`, {
              method: "PATCH",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                status: "Failed",
                progress: 0,
                summary: "Failed: Gateway response timeout after 5s",
                errorMsg: "Timeout: No OTA response from Gateway after 5 seconds",
              }),
            });
            loadJobs();
          } catch (e) {}
        }, 5000);

      } else {
        setIsTriggering(false);
        setLiveOta((prev) => ({
          ...prev,
          status: "Failed",
          message: `Trigger Failed: ${data.error || "Unknown error"}`,
        }));
      }
    } catch (err) {
      setIsTriggering(false);
      setLiveOta((prev) => ({
        ...prev,
        status: "Failed",
        message: `Error: ${err.message}`,
      }));
    }
  };

  const handleDeleteFirmware = async (id) => {
    if (!window.confirm("Are you sure you want to delete this firmware from SQLite database?")) return;
    try {
      await fetch(`${httpUrl}/api/ota/firmwares/${id}`, { method: "DELETE" });
      await loadFirmwares();
    } catch (e) {
      console.error(e);
    }
  };

  const selectedFirmwareObj = firmwares.find((f) => f.id === Number(selectedFirmwareId));

  // Determine Live Status Badge Color
  const getStatusColor = (st) => {
    switch (st) {
      case "Success":
      case "Completed":
        return "#01f7a7";
      case "Failed":
        return "#ff285c";
      case "Flashing":
        return "#ffb547";
      case "Downloading":
      default:
        return "#0075ff";
    }
  };

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        {/* Top Summary Cards */}
        <Grid container spacing={3} mb={3}>
          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(0, 117, 255, 0.15)", border: "1px solid rgba(0, 117, 255, 0.35)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                  <IoWifi size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Gateway Status</VuiTypography>
                  <VuiTypography variant="h6" color={connected ? "success" : "error"} fontWeight="bold">
                    {connected ? "ONLINE" : "OFFLINE"}
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    {deviceInfo?.staIp || "192.168.1.209"}
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(1, 247, 167, 0.15)", border: "1px solid rgba(1, 247, 167, 0.35)", color: "#01f7a7", display: "grid", placeItems: "center" }}>
                  <IoShieldCheckmark size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Active Gateway Firmware</VuiTypography>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                    {activeGatewayVersion}
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    ESP32 Gateway Node
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(138, 44, 255, 0.15)", border: "1px solid rgba(138, 44, 255, 0.35)", color: "#8a2cff", display: "grid", placeItems: "center" }}>
                  <IoHardwareChip size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Stored Binaries in SQLite</VuiTypography>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                    {firmwares.length} Files
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    BLOB SQLite Storage
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(255, 181, 71, 0.15)", border: "1px solid rgba(255, 181, 71, 0.35)", color: "#ffb547", display: "grid", placeItems: "center" }}>
                  <IoTime size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Total OTA Executions</VuiTypography>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                    {historyJobs.length} Jobs
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    Real SQLite History
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
        </Grid>

        {/* Horizontal Row: Combined Operations Card (Left) & Stored Repository Card (Right) */}
        <Grid container spacing={3} mb={3} alignItems="stretch">
          {/* COMBINED CARD: Step 1 (Upload) + Step 2 (Select & Start OTA) */}
          <Grid item xs={12} lg={7}>
            <Card sx={{ ...cardSx, display: "flex", flexDirection: "column" }}>
              {/* Card Header */}
              <VuiBox display="flex" alignItems="center" gap={1.5} mb={2.5}>
                <VuiBox sx={{ width: 38, height: 38, borderRadius: "10px", background: "rgba(0, 117, 255, 0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                  <IoFlash size="20px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="h5" color="white" fontWeight="bold">
                    Gateway OTA Operations
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text">
                    Upload binary image or select existing version to dispatch OTA flash command.
                  </VuiTypography>
                </VuiBox>
              </VuiBox>

              {/* --- SECTION 1: UPLOAD FIRMWARE --- */}
              <VuiBox p={2.5} borderRadius="14px" sx={{ background: "rgba(255, 255, 255, 0.02)", border: "1px solid rgba(255, 255, 255, 0.06)", mb: 2.5, width: "100%", boxSizing: "border-box" }}>
                <VuiBox display="flex" alignItems="center" gap={1} mb={1.5}>
                  <IoCloudUpload color="#8a2cff" size="18px" />
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    1. Upload New Firmware (.bin)
                  </VuiTypography>
                </VuiBox>

                {/* Upload Drop Zone - Fixed with display: block & width: 100% */}
                <VuiBox
                  sx={{
                    display: "block",
                    width: "100%",
                    boxSizing: "border-box",
                    border: "2px dashed rgba(138, 44, 255, 0.45)",
                    borderRadius: "14px",
                    p: 2.5,
                    textAlign: "center",
                    background: "rgba(138, 44, 255, 0.04)",
                    cursor: "pointer",
                    transition: "all 0.3s ease",
                    "&:hover": {
                      borderColor: "#8a2cff",
                      background: "rgba(138, 44, 255, 0.08)",
                    }
                  }}
                  component="label"
                >
                  <input
                    type="file"
                    accept=".bin"
                    hidden
                    onChange={handleFileChange}
                  />
                  <IoCloudUpload size="32px" color="#8a2cff" style={{ marginBottom: "4px", display: "inline-block" }} />
                  <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                    {selectedFile ? selectedFile.name : "Select or drag .bin Gateway Firmware file"}
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11px", mt: 0.3 }}>
                    {selectedFile
                      ? `Size: ${(selectedFile.size / (1024 * 1024)).toFixed(2)} MB • Auto-renamed to [name]_[version].bin`
                      : "Compiled ESP-IDF binary image (target: ESP32 Gateway)"}
                  </VuiTypography>
                </VuiBox>

                {/* Symmetrical Inputs: Version & Release Channel */}
                <Grid container spacing={2} mt={0.5}>
                  <Grid item xs={12} sm={6}>
                    <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                      Firmware Version <span style={{ color: "#ff285c" }}>* (Required)</span>
                    </VuiTypography>
                    <TextField
                      fullWidth
                      variant="outlined"
                      value={uploadVersion}
                      onChange={(e) => setUploadVersion(e.target.value)}
                      sx={formControlSx}
                      placeholder="e.g. 1.0.1"
                    />
                  </Grid>
                  <Grid item xs={12} sm={6}>
                    <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                      Release Channel
                    </VuiTypography>
                    <FormControl fullWidth variant="outlined" sx={formControlSx}>
                      <Select
                        value={channel}
                        onChange={(e) => setChannel(e.target.value)}
                        MenuProps={selectMenuProps}
                        IconComponent={() => (
                          <IoChevronDown
                            color="rgba(255,255,255,0.7)"
                            size="16px"
                            style={{ position: "absolute", right: "14px", pointerEvents: "none" }}
                          />
                        )}
                      >
                        <MenuItem value="stable">Stable (Recommended)</MenuItem>
                        <MenuItem value="beta">Beta / Testing</MenuItem>
                        <MenuItem value="dev">Nightly Development</MenuItem>
                      </Select>
                    </FormControl>
                  </Grid>
                  <Grid item xs={12}>
                    <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                      Release Notes (Optional)
                    </VuiTypography>
                    <TextField
                      fullWidth
                      variant="outlined"
                      value={notes}
                      onChange={(e) => setNotes(e.target.value)}
                      sx={formControlSx}
                      placeholder="Describe changes or bug fixes in this build (optional)..."
                    />
                  </Grid>
                </Grid>

                {uploadError && (
                  <VuiBox mt={1.5} p={1} borderRadius="8px" sx={{ background: "rgba(255, 40, 92, 0.15)", border: "1px solid rgba(255, 40, 92, 0.4)" }}>
                    <VuiTypography variant="caption" color="error" fontWeight="bold">
                      ✕ {uploadError}
                    </VuiTypography>
                  </VuiBox>
                )}

                {uploadMsg && (
                  <VuiBox mt={1.5} p={1} borderRadius="8px" sx={{ background: "rgba(1, 247, 167, 0.15)", border: "1px solid rgba(1, 247, 167, 0.4)" }}>
                    <VuiTypography variant="caption" color="success" fontWeight="bold">
                      {uploadMsg}
                    </VuiTypography>
                  </VuiBox>
                )}

                <VuiBox display="flex" justifyContent="flex-end" mt={2}>
                  <Button
                    variant="contained"
                    disabled={uploading || !selectedFile}
                    onClick={handleUploadFirmware}
                    sx={{
                      borderRadius: "10px",
                      background: "linear-gradient(135deg, #8a2cff 0%, #5b10b0 100%)",
                      color: "#ffffff",
                      px: 3,
                      height: "38px",
                      fontSize: "12px",
                      fontWeight: "bold",
                      "&:hover": { background: "linear-gradient(135deg, #7a1fe6 0%, #4d0b96 100%)" },
                      "&.Mui-disabled": { background: "rgba(255, 255, 255, 0.12)", color: "rgba(255, 255, 255, 0.4)" }
                    }}
                  >
                    <IoCloudUpload size="16px" style={{ marginRight: "6px" }} />
                    {uploading ? "Saving..." : "Upload & Save Firmware"}
                  </Button>
                </VuiBox>
              </VuiBox>

              {/* --- SECTION 2: SELECT VERSION & START OTA --- */}
              <VuiBox p={2.5} borderRadius="14px" sx={{ background: "rgba(255, 255, 255, 0.02)", border: "1px solid rgba(255, 255, 255, 0.06)", flex: 1, display: "flex", flexDirection: "column", justifyContent: "space-between", width: "100%", boxSizing: "border-box" }}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1} mb={1.5}>
                    <IoSend color="#0075ff" size="16px" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">
                      2. Select Firmware Version & Start Gateway OTA
                    </VuiTypography>
                  </VuiBox>

                  <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                    Select Stored Gateway Firmware:
                  </VuiTypography>
                  <FormControl fullWidth variant="outlined" sx={formControlSx}>
                    <Select
                      value={selectedFirmwareId}
                      onChange={(e) => setSelectedFirmwareId(e.target.value)}
                      MenuProps={selectMenuProps}
                      IconComponent={() => (
                        <IoChevronDown
                          color="rgba(255,255,255,0.7)"
                          size="16px"
                          style={{ position: "absolute", right: "14px", pointerEvents: "none" }}
                        />
                      )}
                    >
                      {firmwares.length === 0 ? (
                        <MenuItem value="">No firmware available in SQLite (Upload in section 1 above)</MenuItem>
                      ) : (
                        firmwares.map((f) => (
                          <MenuItem key={f.id} value={f.id}>
                            {f.filename} (v{f.version}) • {(f.fileSize / (1024 * 1024)).toFixed(2)} MB
                          </MenuItem>
                        ))
                      )}
                    </Select>
                  </FormControl>

                  {/* Selected Firmware Preview Card */}
                  {selectedFirmwareObj && (
                    <VuiBox mt={1.5} p={1.5} borderRadius="12px" sx={{ background: "rgba(0, 117, 255, 0.08)", border: "1px solid rgba(0, 117, 255, 0.25)" }}>
                      <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={0.5}>
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "13px" }}>
                          {selectedFirmwareObj.filename}
                        </VuiTypography>
                        <VuiBox sx={{ px: 1.2, py: 0.2, borderRadius: "6px", background: "rgba(1, 247, 167, 0.15)" }}>
                          <VuiTypography variant="caption" color="success" fontWeight="bold">
                            v{selectedFirmwareObj.version}
                          </VuiTypography>
                        </VuiBox>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11px" }}>
                        MD5: <span style={{ color: "#fff", fontFamily: "monospace" }}>{selectedFirmwareObj.checksum}</span>
                      </VuiTypography>
                      <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11px" }}>
                        Size: {(selectedFirmwareObj.fileSize / (1024 * 1024)).toFixed(2)} MB • Uploaded: {new Date(selectedFirmwareObj.uploadedAt).toLocaleString()}
                      </VuiTypography>
                    </VuiBox>
                  )}

                  {/* REALTIME RICH PROGRESS REPORT FROM GATEWAY */}
                  {liveOta.active && (
                    <VuiBox
                      mt={2}
                      p={2}
                      borderRadius="14px"
                      sx={{
                        background: "linear-gradient(127deg, rgba(6, 11, 40, 0.5) 0%, rgba(10, 14, 35, 0.4) 100%)",
                        border: `1px solid ${getStatusColor(liveOta.status)}66`,
                        backdropFilter: "blur(18px)",
                        position: "relative",
                      }}
                    >
                      {/* Top Header: Status Badge, Partition Routing & Percent */}
                      <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1}>
                        <VuiBox display="flex" alignItems="center" gap={1}>
                          <VuiBox
                            sx={{
                              px: 1.5,
                              py: 0.4,
                              borderRadius: "8px",
                              background: `${getStatusColor(liveOta.status)}22`,
                              border: `1px solid ${getStatusColor(liveOta.status)}55`,
                              display: "inline-flex",
                              alignItems: "center",
                              gap: 0.8,
                            }}
                          >
                            {liveOta.status === "Success" || liveOta.status === "Completed" ? (
                              <IoCheckmarkCircle color="#01f7a7" size="15px" />
                            ) : liveOta.status === "Failed" ? (
                              <IoCloseCircle color="#ff285c" size="15px" />
                            ) : (
                              <IoDownload color="#0075ff" size="15px" />
                            )}
                            <VuiTypography variant="caption" fontWeight="bold" sx={{ color: getStatusColor(liveOta.status) }}>
                              {liveOta.status?.toUpperCase()}
                            </VuiTypography>
                          </VuiBox>

                          {liveOta.running_partition && liveOta.target_partition && (
                            <VuiBox display="flex" alignItems="center" gap={0.5} sx={{ px: 1, py: 0.3, borderRadius: "6px", background: "rgba(255,255,255,0.06)" }}>
                              <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                                {liveOta.running_partition}
                              </VuiTypography>
                              <IoArrowForward size="12px" color="rgba(255,255,255,0.6)" />
                              <VuiTypography variant="caption" color="white" fontWeight="bold" sx={{ fontSize: "11px", color: "#01f7a7" }}>
                                {liveOta.target_partition}
                              </VuiTypography>
                            </VuiBox>
                          )}
                        </VuiBox>

                        <VuiBox display="flex" alignItems="center" gap={1}>
                          <VuiTypography variant="h6" fontWeight="bold" sx={{ color: getStatusColor(liveOta.status) }}>
                            {liveOta.percent}%
                          </VuiTypography>
                          <IconButton
                            size="small"
                            onClick={() => setLiveOta((prev) => ({ ...prev, active: false }))}
                            sx={{ color: "rgba(255,255,255,0.4)", "&:hover": { color: "#fff" } }}
                          >
                            <IoClose size="16px" />
                          </IconButton>
                        </VuiBox>
                      </VuiBox>

                      {/* Progress Bar */}
                      <LinearProgress
                        variant="determinate"
                        value={liveOta.percent}
                        sx={{
                          height: "8px",
                          borderRadius: "4px",
                          backgroundColor: "rgba(255, 255, 255, 0.1)",
                          mb: 1.2,
                          "& .MuiLinearProgress-bar": {
                            background:
                              liveOta.status === "Success" || liveOta.status === "Completed"
                                ? "linear-gradient(90deg, #01f7a7 0%, #00d285 100%)"
                                : liveOta.status === "Failed"
                                ? "linear-gradient(90deg, #ff285c 0%, #d41140 100%)"
                                : "linear-gradient(90deg, #0075ff 0%, #01f7a7 100%)",
                            borderRadius: "4px",
                          }
                        }}
                      />

                      {/* Detail row: Bytes transferred & Message */}
                      <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                        <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                          {liveOta.total_bytes > 0
                            ? `${(liveOta.bytes_read / 1024).toFixed(1)} KB / ${(liveOta.total_bytes / 1024).toFixed(1)} KB`
                            : liveOta.message || "Transferring image..."}
                        </VuiTypography>
                        {liveOta.message && (
                          <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", fontStyle: "italic", maxWidth: "65%", textAlign: "right" }}>
                            {liveOta.message}
                          </VuiTypography>
                        )}
                      </VuiBox>
                    </VuiBox>
                  )}
                </VuiBox>

                <VuiBox display="flex" justifyContent="flex-end" mt={2}>
                  <Button
                    variant="contained"
                    disabled={isTriggering || !selectedFirmwareObj}
                    onClick={handleStartOta}
                    sx={{
                      borderRadius: "12px",
                      background: "linear-gradient(135deg, #0075ff 0%, #004ecc 100%)",
                      color: "#ffffff",
                      px: 3.5,
                      height: "42px",
                      fontWeight: "bold",
                      "&:hover": { background: "linear-gradient(135deg, #0060d4 0%, #003fa8 100%)" },
                      "&.Mui-disabled": { background: "rgba(255, 255, 255, 0.12)", color: "rgba(255, 255, 255, 0.4)" }
                    }}
                  >
                    <IoSend size="16px" style={{ marginRight: "8px" }} />
                    Send OTA Command to Gateway
                  </Button>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          {/* STORED REPOSITORY CARD (Right side, side-by-side with Operations Card) */}
          <Grid item xs={12} lg={5}>
            <Card sx={{ ...cardSx, display: "flex", flexDirection: "column" }}>
              <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={2}>
                <VuiBox display="flex" alignItems="center" gap={1.5}>
                  <VuiBox sx={{ width: 38, height: 38, borderRadius: "10px", background: "rgba(138, 44, 255, 0.2)", color: "#8a2cff", display: "grid", placeItems: "center" }}>
                    <IoCube size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="h5" color="white" fontWeight="bold">
                      Stored Repository
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text">
                      SQLite BLOB Firmware Images
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>
                <IconButton onClick={loadFirmwares} sx={{ color: "rgba(255,255,255,0.6)" }}>
                  <IoReload size="16px" />
                </IconButton>
              </VuiBox>

              <VuiBox flex={1} overflow="auto" maxHeight="680px" pr={0.5}>
                {firmwares.length === 0 ? (
                  <VuiBox p={4} textAlign="center" borderRadius="12px" sx={{ background: "rgba(255,255,255,0.02)", border: "1px dashed rgba(255,255,255,0.1)" }}>
                    <IoCube size="32px" color="rgba(255,255,255,0.3)" style={{ marginBottom: "8px" }} />
                    <VuiTypography variant="button" color="text" display="block">
                      No Gateway firmware binaries uploaded yet.
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                      Use the Upload form on the left to add your first binary.
                    </VuiTypography>
                  </VuiBox>
                ) : (
                  firmwares.map((fw) => {
                    const isSelected = selectedFirmwareId === fw.id;
                    return (
                      <VuiBox
                        key={fw.id}
                        p={2}
                        borderRadius="14px"
                        mb={1.5}
                        onClick={() => setSelectedFirmwareId(fw.id)}
                        sx={{
                          background: isSelected ? "rgba(0, 117, 255, 0.15)" : "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
                          border: isSelected ? "1px solid rgba(0, 117, 255, 0.50)" : "1px solid rgba(255, 255, 255, 0.08)",
                          backdropFilter: "blur(18px)",
                          cursor: "pointer",
                          transition: "all 0.2s ease",
                          "&:hover": { border: "1px solid rgba(0, 117, 255, 0.40)" }
                        }}
                      >
                        <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start" mb={0.5}>
                          <VuiBox>
                            <VuiTypography variant="button" color="white" fontWeight="bold">
                              {fw.filename}
                            </VuiTypography>
                            <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11px" }}>
                              Size: {(fw.fileSize / (1024 * 1024)).toFixed(2)} MB • {new Date(fw.uploadedAt).toLocaleDateString()}
                            </VuiTypography>
                          </VuiBox>
                          <VuiBox display="flex" alignItems="center" gap={1}>
                            <VuiBox sx={{ px: 1.2, py: 0.2, borderRadius: "6px", background: "rgba(1, 247, 167, 0.15)" }}>
                              <VuiTypography variant="caption" color="success" fontWeight="bold">
                                v{fw.version}
                              </VuiTypography>
                            </VuiBox>
                            <IconButton
                              size="small"
                              sx={{ color: "rgba(255, 40, 92, 0.7)" }}
                              onClick={(e) => {
                                e.stopPropagation();
                                handleDeleteFirmware(fw.id);
                              }}
                            >
                              <IoTrashOutline size="16px" />
                            </IconButton>
                          </VuiBox>
                        </VuiBox>
                        <VuiTypography variant="caption" color="text" sx={{ fontFamily: "monospace", fontSize: "10px", opacity: 0.7 }}>
                          MD5: {fw.checksum}
                        </VuiTypography>
                        {fw.notes && (
                          <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11px", fontStyle: "italic", mt: 0.5 }}>
                            "{fw.notes}"
                          </VuiTypography>
                        )}
                      </VuiBox>
                    );
                  })
                )}
              </VuiBox>
            </Card>
          </Grid>
        </Grid>

        {/* Gateway OTA History Table from SQLite */}
        <Card sx={cardSx}>
          <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={3}>
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0, 117, 255, 0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoTime size="18px" />
              </VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                Gateway OTA Execution History (SQLite Real Data)
              </VuiTypography>
            </VuiBox>
            <IconButton onClick={loadJobs} sx={{ color: "rgba(255,255,255,0.6)" }}>
              <IoReload size="16px" />
            </IconButton>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={2.5}><VuiTypography variant="caption" color="text" fontWeight="medium">JOB ID</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">VERSION</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">DETAILS / PARTITIONS</VuiTypography></Grid>
            <Grid item xs={2.5}><VuiTypography variant="caption" color="text" fontWeight="medium">EXECUTED AT</VuiTypography></Grid>
            <Grid item xs={2.5} textAlign="right"><VuiTypography variant="caption" color="text" fontWeight="medium">STATUS</VuiTypography></Grid>
          </Grid>

          {historyJobs.length === 0 ? (
            <VuiBox p={4} textAlign="center">
              <VuiTypography variant="caption" color="text">
                No Gateway OTA execution records found in SQLite database.
              </VuiTypography>
            </VuiBox>
          ) : (
            historyJobs.map((item) => {
              const isSuccess = item.status === "Completed" || item.status === "Success";
              const isFailed = item.status === "Failed";
              const statusColor = isSuccess ? "#01f7a7" : isFailed ? "#ff285c" : "#0075ff";
              const statusBg = isSuccess ? "rgba(1, 247, 167, 0.15)" : isFailed ? "rgba(255, 40, 92, 0.15)" : "rgba(0, 117, 255, 0.15)";

              return (
                <VuiBox
                  key={item.id || item.jobId}
                  sx={{
                    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
                    borderRadius: "14px",
                    border: "1px solid rgba(255, 255, 255, 0.08)",
                    backdropFilter: "blur(18px)",
                    p: 2,
                    mb: 1.5,
                  }}
                >
                  <Grid container alignItems="center">
                    <Grid item xs={2.5}>
                      <VuiTypography variant="button" color="text" fontWeight="bold">{item.jobId}</VuiTypography>
                    </Grid>
                    <Grid item xs={1.5}>
                      <VuiTypography variant="button" color="white" fontWeight="bold">v{item.version}</VuiTypography>
                    </Grid>
                    <Grid item xs={3}>
                      <VuiTypography variant="caption" color="text" display="block">
                        {item.summary || item.firmwareUrl}
                      </VuiTypography>
                    </Grid>
                    <Grid item xs={2.5}>
                      <VuiTypography variant="caption" color="text">
                        {new Date(item.startedAt).toLocaleString()}
                      </VuiTypography>
                    </Grid>
                    <Grid item xs={2.5} textAlign="right">
                      <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: statusBg, display: "inline-flex", alignItems: "center", gap: 1 }}>
                        {isSuccess ? (
                          <IoCheckmarkCircle color={statusColor} size="14px" />
                        ) : isFailed ? (
                          <IoCloseCircle color={statusColor} size="14px" />
                        ) : (
                          <IoReload color={statusColor} size="14px" />
                        )}
                        <VuiTypography variant="caption" fontWeight="bold" sx={{ color: statusColor }}>
                          {item.status?.toUpperCase()}
                        </VuiTypography>
                      </VuiBox>
                    </Grid>
                  </Grid>
                </VuiBox>
              );
            })
          )}
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default OtaGateway;
