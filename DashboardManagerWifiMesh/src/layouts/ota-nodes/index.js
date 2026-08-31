import React, { useMemo, useState, useEffect, useCallback, useRef } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import Select from "@mui/material/Select";
import LinearProgress from "@mui/material/LinearProgress";
import Checkbox from "@mui/material/Checkbox";
import FormControlLabel from "@mui/material/FormControlLabel";
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
  IoHardwareChip,
  IoCloudUpload,
  IoRocket,
  IoCheckmarkCircle,
  IoCloseCircle,
  IoReload,
  IoTime,
  IoDocumentText,
  IoServer,
  IoWarning,
  IoArrowUpCircle,
  IoLayers,
  IoChevronDown,
  IoCube,
  IoTrashOutline,
  IoSend,
  IoFlash,
  IoClose,
  IoDownload,
  IoArrowForward,
  IoShieldCheckmark
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

function OtaNodes() {
  const { nodes } = useDashboardRealtime();
  const httpUrl = getHttpUrl();
  const wsUrl = getWebSocketUrl();

  const otaTimeoutRef = useRef(null);
  const autoDismissTimerRef = useRef(null);
  const currentJobIdRef = useRef("");

  // --- Step 1: Upload state ---
  const [selectedFile, setSelectedFile] = useState(null);
  const [uploadVersion, setUploadVersion] = useState("");
  const [channel, setChannel] = useState("stable");
  const [notes, setNotes] = useState("");
  const [uploadingFw, setUploadingFw] = useState(false);
  const [uploadMsg, setUploadMsg] = useState("");
  const [uploadError, setUploadError] = useState("");

  // --- Step 2: Trigger state ---
  const [targetScope, setTargetScope] = useState("all");
  const [selectedFirmwareId, setSelectedFirmwareId] = useState("");
  const [isUpdating, setIsUpdating] = useState(false);
  const [jobSummary, setJobSummary] = useState("");
  const [autoRetry, setAutoRetry] = useState(true);
  const [currentJobId, setCurrentJobId] = useState("");

  // Live OTA progress state
  const [liveOta, setLiveOta] = useState({
    active: false,
    jobId: "",
    targetDetail: "all",
    status: "Initiating", // Initiating, Dispatched, Downloading, Flashing, Success, Failed
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
  const [nodeProgressMap, setNodeProgressMap] = useState({});

  // Fetch firmwares from SQLite
  const loadFirmwares = useCallback(async () => {
    try {
      const res = await fetch(`${httpUrl}/api/ota/firmwares`);
      const data = await res.json();
      if (Array.isArray(data.firmwares)) {
        const nodeFws = data.firmwares.filter(f => f.targetType === "node" || f.targetType === "root");
        setFirmwares(nodeFws);
        if (nodeFws.length > 0 && !selectedFirmwareId) {
          setSelectedFirmwareId(nodeFws[0].id);
        }
      }
    } catch (err) {
      console.error("[ota] Failed to load firmwares:", err.message);
    }
  }, [httpUrl, selectedFirmwareId]);

  // Fetch real FOTA jobs from SQLite
  const loadJobs = useCallback(async () => {
    try {
      const res = await fetch(`${httpUrl}/api/ota/jobs?limit=50`);
      const data = await res.json();
      if (Array.isArray(data.jobs)) {
        const nodeJobs = data.jobs.filter(j => j.targetType === "node" || j.targetType === "root" || j.targetType === "all");
        setHistoryJobs(nodeJobs);
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

  // Clear timers on component unmount only
  useEffect(() => {
    return () => {
      if (otaTimeoutRef.current) clearTimeout(otaTimeoutRef.current);
      if (autoDismissTimerRef.current) clearTimeout(autoDismissTimerRef.current);
    };
  }, []);

  // Listen to WebSocket FOTA progress
  useEffect(() => {
    if (!wsUrl) return;
    let ws;
    try {
      ws = new WebSocket(wsUrl);
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          const activeJobId = currentJobIdRef.current;
          if (msg.type === "ota_progress" && (msg.target === "node" || msg.target === "root" || msg.targetType === "node" || (activeJobId && msg.jobId === activeJobId))) {
            const detail = msg.targetDetail || msg.targetMac || "all";
            const progress = Number(msg.progress ?? msg.percent ?? 0);
            const status = msg.status || "Downloading";
            const bytesRead = Number(msg.bytes_read ?? 0);
            const totalBytes = Number(msg.total_bytes ?? 0);
            const runningPart = msg.running_partition || "";
            const targetPart = msg.target_partition || "";
            const message = msg.message || (status === "Success" ? "Mesh FOTA completed!" : "Downloading and distributing across mesh...");

            // ⏱️ Clear watchdog timeout as soon as real response arrives from hardware
            if (status === "Downloading" || status === "Flashing" || status === "Success" || bytesRead > 0 || progress > 5) {
              if (otaTimeoutRef.current) {
                clearTimeout(otaTimeoutRef.current);
                otaTimeoutRef.current = null;
              }
            }

            setLiveOta({
              active: true,
              jobId: msg.jobId || activeJobId,
              targetDetail: detail,
              status: status,
              percent: progress,
              bytes_read: bytesRead,
              total_bytes: totalBytes,
              running_partition: runningPart,
              target_partition: targetPart,
              message: message,
            });

            setNodeProgressMap((prev) => ({
              ...prev,
              [detail]: {
                progress,
                status: status,
                statusColor: progress === 100 ? "#01f7a7" : status === "Failed" ? "#ff285c" : "#0075ff",
              },
            }));

            if (progress === 100 || status === "Completed" || status === "Success") {
              setIsUpdating(false);
              setJobSummary("Mesh FOTA process completed successfully!");
              loadJobs();

              if (autoDismissTimerRef.current) clearTimeout(autoDismissTimerRef.current);
              autoDismissTimerRef.current = setTimeout(() => {
                setLiveOta((prev) => ({ ...prev, active: false }));
              }, 7000);
            } else if (status === "Failed") {
              setIsUpdating(false);
              setJobSummary(`Mesh FOTA failed: ${msg.errorMsg || message}`);
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

  // Real Active Nodes computed dynamically from telemetry
  const activeNodes = useMemo(() => {
    const raw = Array.isArray(nodes) ? nodes : [];
    const selectedFw = firmwares.find((f) => f.id === Number(selectedFirmwareId));
    
    const rootNode = raw.find((n) => Number(n.meshLevel) === 0);
    const rootActiveVer = rootNode?.firmwareVersion && rootNode.firmwareVersion !== "0.0.0"
      ? (String(rootNode.firmwareVersion).startsWith("v") ? String(rootNode.firmwareVersion) : `v${rootNode.firmwareVersion}`)
      : (selectedFw ? `v${selectedFw.version}` : (firmwares.length > 0 ? `v${firmwares[0].version}` : "v0.0.1"));

    const targetVerStr = targetScope === "root"
      ? (selectedFw ? `v${selectedFw.version}` : (firmwares.length > 0 ? `v${firmwares[0].version}` : "—"))
      : rootActiveVer;

    const lookupProgress = (macStr, levelNum) => {
      const normMac = String(macStr || "").toUpperCase();
      for (const [k, v] of Object.entries(nodeProgressMap)) {
        if (String(k).toUpperCase() === normMac) return v;
      }
      if (nodeProgressMap[`level:${levelNum}`]) return nodeProgressMap[`level:${levelNum}`];
      if (levelNum === 0 && nodeProgressMap["root"]) return nodeProgressMap["root"];
      if (levelNum >= 1 && nodeProgressMap["leaf"]) return nodeProgressMap["leaf"];
      if (nodeProgressMap["all"]) return nodeProgressMap["all"];
      if (nodeProgressMap["broadcast"]) return nodeProgressMap["broadcast"];
      return null;
    };

    return raw.map((n, idx) => {
      const mac = n.mac || n.staMac || n.staIpv4 || n.id || `NODE-${idx + 1}`;
      const level = Number(n.meshLevel ?? (idx === 0 ? 0 : 1));
      const prg = lookupProgress(mac, level);
      
      const rawVer = n.firmwareVersion || n.rawPayload?.ver || n.version;
      const currentVersion = rawVer && rawVer !== "0.0.0"
        ? (String(rawVer).startsWith("v") ? String(rawVer) : `v${rawVer}`)
        : (n.schemaVersion ? `v0.0.${n.schemaVersion}` : "v0.0.1");

      const isUpToDate = targetVerStr !== "—" && currentVersion === targetVerStr;

      let status = "Ready";
      let statusColor = "#0075ff";

      if (prg) {
        status = prg.status || "In progress";
        statusColor = prg.statusColor || (prg.progress === 100 ? "#01f7a7" : "#0075ff");
      } else if (isUpToDate) {
        status = "Up to Date";
        statusColor = "#01f7a7";
      } else if (targetVerStr !== "—") {
        status = "Ready to Update";
        statusColor = "#ffb547";
      }

      const shortMac = mac && mac.length >= 5 ? mac.slice(-5) : mac;
      let displayName = `Mesh Node ${idx + 1} (${shortMac})`;
      if (level === 0) displayName = `Root Node (Mesh Root · ${shortMac})`;
      else if (level === 1) displayName = `Mesh Node (Level 1 · ${shortMac})`;
      else if (level === 2) displayName = `Mesh Node (Level 2 · ${shortMac})`;
      else if (level >= 3) displayName = `Leaf Node (Level ${level} · ${shortMac})`;

      return {
        mac,
        name: displayName,
        level,
        currentVersion,
        targetVersion: targetVerStr,
        progress: prg?.progress ?? 0,
        status,
        statusColor,
      };
    });
  }, [nodes, nodeProgressMap, firmwares, selectedFirmwareId]);

  // Real Stats (Total real active nodes, upToDate, outdated)
  const stats = useMemo(() => {
    const total = activeNodes.length;
    const selectedFw = firmwares.find(f => f.id === Number(selectedFirmwareId));
    const targetVerStr = selectedFw ? `v${selectedFw.version}` : (firmwares.length > 0 ? `v${firmwares[0].version}` : "v0.0.1");
    const upToDate = activeNodes.filter(n => n.currentVersion === targetVerStr).length;
    return {
      total,
      upToDate,
      outdated: Math.max(0, total - upToDate),
    };
  }, [activeNodes, firmwares, selectedFirmwareId]);

  // --- Step 1: Upload Node/Root firmware file to SQLite ---
  const handleUploadFirmware = async () => {
    if (!selectedFile) {
      setUploadError("Please select a .bin firmware binary file!");
      return;
    }
    if (!uploadVersion.trim()) {
      setUploadError("Firmware Version is required (e.g. 1.0.1)!");
      return;
    }

    setUploadingFw(true);
    setUploadMsg("Uploading and storing binary in SQLite database...");
    setUploadError("");

    try {
      const formData = new FormData();
      formData.append("file", selectedFile);
      formData.append("targetType", "node");
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
      setUploadingFw(false);
    }
  };

  // --- Step 2: Trigger FOTA Mesh Update (with 10-second Watchdog Timeout) ---
  const handleStartMeshFota = async () => {
    const isRootTarget = targetScope === "root";
    const selectedFw = firmwares.find((f) => f.id === Number(selectedFirmwareId));

    if (isRootTarget && !selectedFw) {
      alert("Please select a target Root Node firmware version from the dropdown first.");
      return;
    }

    if (otaTimeoutRef.current) {
      clearTimeout(otaTimeoutRef.current);
      otaTimeoutRef.current = null;
    }
    if (autoDismissTimerRef.current) {
      clearTimeout(autoDismissTimerRef.current);
    }

    setIsUpdating(true);
    setJobSummary(
      isRootTarget
        ? `Dispatching Root OTA command to Server HTTP endpoint (${selectedFw?.filename})...`
        : `Triggering Root Node to distribute active firmware to Mesh Nodes (${targetScope})...`
    );

    setLiveOta({
      active: true,
      jobId: "",
      targetDetail: targetScope,
      status: "Initiating",
      percent: 5,
      bytes_read: 0,
      total_bytes: isRootTarget ? (selectedFw?.fileSize || 0) : 0,
      running_partition: "Detecting...",
      target_partition: "Detecting...",
      message: isRootTarget
        ? `Dispatching Root Node OTA Command (URL: ${selectedFw?.filename})...`
        : `Dispatching Mesh Distribution Command to Root (Scope: ${targetScope})...`,
    });

    try {
      const payload = {
        targetType: "node",
        targetDetail: targetScope,
        targetMac: targetScope,
      };

      if (isRootTarget && selectedFw) {
        payload.firmwareId = selectedFw.id;
        payload.version = selectedFw.version;
        payload.filename = selectedFw.filename;
      }

      const res = await fetch(`${httpUrl}/api/ota/trigger`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
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
          message: isRootTarget
            ? `Command sent (${jobId}). Waiting for Root Node to start download (Timeout: 10s)...`
            : `Mesh command sent (${jobId}). Waiting for Root to initiate mesh distribution (Timeout: 10s)...`,
        }));
        setJobSummary(`FOTA Job initiated (${jobId}). Waiting for response...`);
        loadJobs();

        // ⏱️ Start 10-second watchdog timer: If no response from Mesh in 10s -> FAIL
        otaTimeoutRef.current = setTimeout(async () => {
          setIsUpdating(false);
          setLiveOta((prev) => {
            if (prev.status === "Dispatched" || prev.status === "Initiating") {
              return {
                ...prev,
                status: "Failed",
                percent: 0,
                message: "Timeout: No OTA response from Mesh Nodes / Root after 10s. Device did not start.",
              };
            }
            return prev;
          });
          setJobSummary("Failed: No response from Mesh Nodes / Root after 10s timeout (Watchdog Timeout).");

          try {
            await fetch(`${httpUrl}/api/ota/jobs/${jobId}/status`, {
              method: "PATCH",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                status: "Failed",
                progress: 0,
                summary: "Failed: Mesh Nodes response timeout after 10s",
                errorMsg: "Timeout: No OTA progress received from Mesh Nodes within 10 seconds",
              }),
            });
            loadJobs();
          } catch (e) {}
        }, 10000);
      } else {
        setIsUpdating(false);
        setLiveOta((prev) => ({
          ...prev,
          status: "Failed",
          message: `Trigger Failed: ${data.error || "Unknown error"}`,
        }));
        setJobSummary(`Trigger Error: ${data.error || "Unknown"}`);
      }
    } catch (err) {
      setIsUpdating(false);
      setLiveOta((prev) => ({
        ...prev,
        status: "Failed",
        message: `Error: ${err.message}`,
      }));
      setJobSummary(`Error: ${err.message}`);
    }
  };

  const handleDeleteFirmware = async (id) => {
    if (!window.confirm("Delete this node firmware binary from SQLite?")) return;
    try {
      await fetch(`${httpUrl}/api/ota/firmwares/${id}`, { method: "DELETE" });
      await loadFirmwares();
    } catch (e) {}
  };

  const selectedFirmwareObj = firmwares.find(f => f.id === Number(selectedFirmwareId));

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
                  <IoLayers size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Mesh Nodes Count</VuiTypography>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                    {stats.total} Nodes Active
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    Wi-Fi Mesh Topology
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(1, 247, 167, 0.15)", border: "1px solid rgba(1, 247, 167, 0.35)", color: "#01f7a7", display: "grid", placeItems: "center" }}>
                  <IoCheckmarkCircle size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Stored Node Binaries</VuiTypography>
                  <VuiTypography variant="h6" color="success" fontWeight="bold">
                    {firmwares.length} Available
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    SQLite Database Stored
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(255, 181, 71, 0.15)", border: "1px solid rgba(255, 181, 71, 0.35)", color: "#ffb547", display: "grid", placeItems: "center" }}>
                  <IoWarning size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Target FOTA Version</VuiTypography>
                  <VuiTypography variant="h6" color="warning" fontWeight="bold">
                    {selectedFirmwareObj ? `v${selectedFirmwareObj.version}` : (firmwares.length > 0 ? `v${firmwares[0].version}` : "None")}
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    Mesh Over-The-Air
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          <Grid item xs={12} sm={6} xl={3}>
            <Card sx={cardSx}>
              <VuiBox display="flex" alignItems="center" gap={2}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(138, 44, 255, 0.15)", border: "1px solid rgba(138, 44, 255, 0.35)", color: "#8a2cff", display: "grid", placeItems: "center" }}>
                  <IoRocket size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text">Mesh FOTA Mode</VuiTypography>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                    Multi-Hop Mesh
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                    HTTP GET + Mesh Forwarding
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>
        </Grid>

        {/* Horizontal Row: Combined Operations Card (Left) & Stored Repository Card (Right) */}
        <Grid container spacing={3} mb={3} alignItems="stretch">
          {/* COMBINED CARD: Step 1 (Upload Node FW) + Step 2 (Select & Start Mesh FOTA) */}
          <Grid item xs={12} lg={7}>
            <Card sx={{ ...cardSx, display: "flex", flexDirection: "column" }}>
              {/* Card Header */}
              <VuiBox display="flex" alignItems="center" gap={1.5} mb={2.5}>
                <VuiBox sx={{ width: 38, height: 38, borderRadius: "10px", background: "rgba(0, 117, 255, 0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                  <IoRocket size="20px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="h5" color="white" fontWeight="bold">
                    Mesh FOTA Operations
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text">
                    Upload Node/Root binaries and dispatch synchronized FOTA across the mesh.
                  </VuiTypography>
                </VuiBox>
              </VuiBox>

              {/* --- SECTION 1: UPLOAD NODE FIRMWARE --- */}
              <VuiBox p={2.5} borderRadius="14px" sx={{ background: "rgba(255, 255, 255, 0.02)", border: "1px solid rgba(255, 255, 255, 0.06)", mb: 2.5, width: "100%", boxSizing: "border-box" }}>
                <VuiBox display="flex" alignItems="center" gap={1} mb={1.5}>
                  <IoCloudUpload color="#8a2cff" size="18px" />
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    1. Upload Node / Root Firmware (.bin)
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
                    "&:hover": { borderColor: "#8a2cff", background: "rgba(138, 44, 255, 0.08)" }
                  }}
                  component="label"
                >
                  <input type="file" accept=".bin" hidden onChange={(e) => {
                    if (e.target.files && e.target.files[0]) {
                      setSelectedFile(e.target.files[0]);
                      setUploadMsg("");
                      setUploadError("");
                    }
                  }} />
                  <IoCloudUpload size="32px" color="#8a2cff" style={{ marginBottom: "4px", display: "inline-block" }} />
                  <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                    {selectedFile ? selectedFile.name : "Select or Drop Node/Root .bin File"}
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", mt: 0.3 }}>
                    {selectedFile ? `Size: ${(selectedFile.size / (1024 * 1024)).toFixed(2)} MB` : "Auto-renames to [name]_[version].bin in SQLite"}
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
                      placeholder="Describe changes in this mesh firmware build (optional)..."
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
                    disabled={uploadingFw || !selectedFile}
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
                    {uploadingFw ? "Saving..." : "Upload & Save Node Firmware"}
                  </Button>
                </VuiBox>
              </VuiBox>

              {/* --- SECTION 2: SELECT VERSION & START MESH FOTA --- */}
              <VuiBox p={2.5} borderRadius="14px" sx={{ background: "rgba(255, 255, 255, 0.02)", border: "1px solid rgba(255, 255, 255, 0.06)", flex: 1, display: "flex", flexDirection: "column", justifyContent: "space-between", width: "100%", boxSizing: "border-box" }}>
                <VuiBox>
                  <VuiBox display="flex" alignItems="center" gap={1} mb={1.5}>
                    <IoSend color={targetScope === "root" ? "#8a2cff" : "#0075ff"} size="16px" />
                    <VuiTypography variant="button" color="white" fontWeight="bold">
                      {targetScope === "root"
                        ? "2. Select Firmware Version & Start Root Node OTA"
                        : "2. Trigger Mesh FOTA Distribution (Root ➔ Mesh Nodes)"}
                    </VuiTypography>
                  </VuiBox>

                  <Grid container spacing={2}>
                    <Grid item xs={12} sm={targetScope === "root" ? 6 : 12}>
                      <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                        Target Scope
                      </VuiTypography>
                      <FormControl fullWidth variant="outlined" sx={formControlSx}>
                        <Select
                          value={targetScope}
                          onChange={(e) => setTargetScope(e.target.value)}
                          MenuProps={selectMenuProps}
                          IconComponent={() => (
                            <IoChevronDown
                              color="rgba(255,255,255,0.7)"
                              size="16px"
                              style={{ position: "absolute", right: "14px", pointerEvents: "none" }}
                            />
                          )}
                        >
                          <MenuItem value="all">🌐 All Mesh Nodes (Broadcast OTA)</MenuItem>
                          <MenuItem value="root">👑 Root Node Only (Level 0 · HTTP Download)</MenuItem>
                          <MenuItem value="leaf">🍃 All Leaf Nodes (Level &gt;= 1)</MenuItem>
                          <MenuItem value="level:1">📶 Level 1 Nodes Only</MenuItem>
                          <MenuItem value="level:2">📶 Level 2 Nodes Only</MenuItem>
                          <MenuItem value="level:3">📶 Level 3 Nodes Only</MenuItem>
                          {activeNodes && activeNodes.length > 0 && (
                            activeNodes.map((n) => (
                              <MenuItem key={n.mac} value={n.mac}>
                                🎯 Single Node: {n.name} ({n.mac})
                              </MenuItem>
                            ))
                          )}
                        </Select>
                      </FormControl>
                    </Grid>

                    {targetScope === "root" && (
                      <Grid item xs={12} sm={6}>
                        <VuiTypography variant="caption" color="text" mb={0.5} display="block" fontWeight="medium">
                          Select Stored Root Firmware (.bin)
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
                              <MenuItem value="">No firmware available (Upload in section 1 above)</MenuItem>
                            ) : (
                              firmwares.map((f) => (
                                <MenuItem key={f.id} value={f.id}>
                                  {f.filename} (v{f.version}) • {(f.fileSize / (1024 * 1024)).toFixed(2)} MB
                                </MenuItem>
                              ))
                            )}
                          </Select>
                        </FormControl>
                      </Grid>
                    )}
                  </Grid>

                  {/* If Target is Root: Show Selected Firmware Preview Card */}
                  {targetScope === "root" && selectedFirmwareObj && (
                    <VuiBox mt={1.5} p={1.5} borderRadius="12px" sx={{ background: "rgba(138, 44, 255, 0.08)", border: "1px solid rgba(138, 44, 255, 0.25)" }}>
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

                  {/* If Target is Nodes (non-root): Show Internal Root Distribution Info Card */}
                  {targetScope !== "root" && (
                    <VuiBox mt={1.5} p={2} borderRadius="12px" sx={{ background: "rgba(0, 117, 255, 0.08)", border: "1px solid rgba(0, 117, 255, 0.25)" }}>
                      <VuiBox display="flex" alignItems="center" gap={1.2} mb={0.8}>
                        <IoShieldCheckmark size="18px" color="#01f7a7" />
                        <VuiTypography variant="button" color="white" fontWeight="bold" sx={{ fontSize: "13px" }}>
                          Automatic Root Firmware Distribution
                        </VuiTypography>
                        <VuiBox sx={{ px: 1, py: 0.2, borderRadius: "6px", background: "rgba(1, 247, 167, 0.15)" }}>
                          <VuiTypography variant="caption" color="success" fontWeight="bold">
                            Internal Mesh Forwarding
                          </VuiTypography>
                        </VuiBox>
                      </VuiBox>
                      <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "11.5px", lineHeight: "1.6" }}>
                        Mesh Nodes do not download from Server HTTP. <strong>Root Node</strong> will automatically read its own active running firmware directly from flash and forward firmware blocks hop-by-hop across the Wi-Fi Mesh network to target: <strong style={{ color: "#0075ff" }}>{targetScope}</strong>.
                      </VuiTypography>
                    </VuiBox>
                  )}

                  {/* REALTIME RICH PROGRESS REPORT FROM MESH NODES */}
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
                      {/* Top Header: Status Badge, Target Scope, Partition Routing & Percent */}
                      <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1}>
                        <VuiBox display="flex" alignItems="center" gap={1} flexWrap="wrap">
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

                          <VuiBox sx={{ px: 1, py: 0.3, borderRadius: "6px", background: "rgba(0, 117, 255, 0.15)", border: "1px solid rgba(0, 117, 255, 0.3)" }}>
                            <VuiTypography variant="caption" color="info" fontWeight="bold" sx={{ fontSize: "11px" }}>
                              Scope: {liveOta.targetDetail}
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
                                ? "linear-gradient(90deg, #01f7a7 0%, #00d68f 100%)"
                                : liveOta.status === "Failed"
                                ? "linear-gradient(90deg, #ff285c 0%, #d61845 100%)"
                                : "linear-gradient(90deg, #0075ff 0%, #01f7a7 100%)",
                            borderRadius: "4px",
                          },
                        }}
                      />

                      {/* Footer Message & Byte Count */}
                      <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                        <VuiTypography variant="caption" color="white" sx={{ fontSize: "11.5px", fontWeight: "500" }}>
                          {liveOta.message}
                        </VuiTypography>
                        {liveOta.total_bytes > 0 && (
                          <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px", fontFamily: "monospace" }}>
                            {(liveOta.bytes_read / 1024).toFixed(1)} KB / {(liveOta.total_bytes / 1024).toFixed(1)} KB
                          </VuiTypography>
                        )}
                      </VuiBox>
                    </VuiBox>
                  )}

                  <VuiBox mt={1.5}>
                    <FormControlLabel
                      control={
                        <Checkbox
                          checked={autoRetry}
                          onChange={(e) => setAutoRetry(e.target.checked)}
                          sx={{ color: "#0075ff", "&.Mui-checked": { color: "#0075ff" } }}
                        />
                      }
                      label={
                        <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                          Enable Multi-Hop Mesh packet loss auto-retry (3 attempts)
                        </VuiTypography>
                      }
                    />
                  </VuiBox>

                  {jobSummary && (
                    <VuiBox mt={1.5} p={1} borderRadius="8px" sx={{ background: "rgba(0, 117, 255, 0.12)", border: "1px solid rgba(0, 117, 255, 0.35)" }}>
                      <VuiTypography variant="caption" color="info" fontWeight="bold">
                        ℹ {jobSummary}
                      </VuiTypography>
                    </VuiBox>
                  )}
                </VuiBox>

                <VuiBox display="flex" justifyContent="flex-end" mt={2}>
                  <Button
                    variant="contained"
                    disabled={isUpdating || (targetScope === "root" && !selectedFirmwareObj)}
                    onClick={handleStartMeshFota}
                    sx={{
                      borderRadius: "12px",
                      background: targetScope === "root"
                        ? "linear-gradient(135deg, #8a2cff 0%, #5b10b0 100%)"
                        : "linear-gradient(135deg, #0075ff 0%, #004ecc 100%)",
                      color: "#ffffff",
                      px: 3.5,
                      height: "42px",
                      fontWeight: "bold",
                      "&:hover": {
                        background: targetScope === "root"
                          ? "linear-gradient(135deg, #7a1fe6 0%, #4d0b96 100%)"
                          : "linear-gradient(135deg, #0060d4 0%, #003fa8 100%)"
                      },
                      "&.Mui-disabled": { background: "rgba(255, 255, 255, 0.12)", color: "rgba(255, 255, 255, 0.4)" }
                    }}
                  >
                    <IoSend size="16px" style={{ marginRight: "8px" }} />
                    {targetScope === "root"
                      ? "Send OTA Command to Root Node (HTTP Download)"
                      : `Dispatch Mesh FOTA (Root ➔ ${targetScope === "all" ? "All Nodes" : targetScope})`}
                  </Button>
                </VuiBox>
              </VuiBox>
            </Card>
          </Grid>

          {/* STORED NODE REPOSITORY CARD (Right side, side-by-side with Operations Card) */}
          <Grid item xs={12} lg={5}>
            <Card sx={{ ...cardSx, display: "flex", flexDirection: "column" }}>
              <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={2}>
                <VuiBox display="flex" alignItems="center" gap={1.5}>
                  <VuiBox sx={{ width: 38, height: 38, borderRadius: "10px", background: "rgba(138, 44, 255, 0.2)", color: "#8a2cff", display: "grid", placeItems: "center" }}>
                    <IoCube size="20px" />
                  </VuiBox>
                  <VuiBox>
                    <VuiTypography variant="h5" color="white" fontWeight="bold">
                      Stored Node Repository
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
                      No Node firmware binaries uploaded yet.
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                      Use the Upload form on the left to add your first node binary.
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
                              Mesh Firmware • {(fw.fileSize / (1024 * 1024)).toFixed(2)} MB • {new Date(fw.uploadedAt).toLocaleDateString()}
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

        {/* Realtime Node Progress Status Table */}
        <Card sx={{ ...cardSx, mb: 3 }}>
          <VuiBox display="flex" alignItems="center" gap={1.5} mb={3}>
            <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0, 117, 255, 0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
              <IoServer size="18px" />
            </VuiBox>
            <VuiTypography variant="h5" color="white" fontWeight="bold">
              Realtime Node-by-Node FOTA Progress
            </VuiTypography>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">DEVICE / MAC</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">MESH LEVEL</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">CURRENT</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">TARGET</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">PROGRESS %</VuiTypography></Grid>
            <Grid item xs={1.5} textAlign="right"><VuiTypography variant="caption" color="text" fontWeight="medium">STATUS</VuiTypography></Grid>
          </Grid>

          {activeNodes.length === 0 ? (
            <VuiBox p={4} textAlign="center" borderRadius="12px" sx={{ background: "rgba(255, 255, 255, 0.02)", border: "1px dashed rgba(255, 255, 255, 0.1)" }}>
              <IoServer size="32px" color="rgba(255, 255, 255, 0.3)" style={{ marginBottom: "8px" }} />
              <VuiTypography variant="button" color="text" display="block">
                No active Mesh Nodes currently detected on the network.
              </VuiTypography>
              <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>
                Ensure Root Node and Mesh Nodes are powered on and sending UDP sensor packets.
              </VuiTypography>
            </VuiBox>
          ) : (
            activeNodes.map((node) => (
              <VuiBox
                key={node.mac}
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
                  <Grid item xs={3}>
                    <VuiTypography variant="button" color="white" fontWeight="bold" display="block">{node.name}</VuiTypography>
                    <VuiTypography variant="caption" color="text" sx={{ fontSize: "11px" }}>{node.mac}</VuiTypography>
                  </Grid>
                  <Grid item xs={1.5}>
                    <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: "rgba(0, 117, 255, 0.15)", display: "inline-block" }}>
                      <VuiTypography variant="caption" color="info" fontWeight="bold">
                        {node.level === 0 ? "Root (0)" : `Level ${node.level}`}
                      </VuiTypography>
                    </VuiBox>
                  </Grid>
                  <Grid item xs={1.5}>
                    <VuiTypography variant="button" color="text">{node.currentVersion}</VuiTypography>
                  </Grid>
                  <Grid item xs={1.5}>
                    <VuiTypography variant="button" color="success" fontWeight="bold">{node.targetVersion}</VuiTypography>
                  </Grid>
                  <Grid item xs={3} pr={2}>
                    <VuiBox display="flex" alignItems="center" gap={1.5}>
                      <VuiBox flex={1}>
                        <LinearProgress
                          variant="determinate"
                          value={node.progress}
                          sx={{
                            height: "6px",
                            borderRadius: "3px",
                            backgroundColor: "rgba(255, 255, 255, 0.1)",
                            "& .MuiLinearProgress-bar": {
                              background: node.progress === 100 ? "#01f7a7" : "#0075ff",
                              borderRadius: "3px",
                            }
                          }}
                        />
                      </VuiBox>
                      <VuiTypography variant="caption" color="white" fontWeight="bold" sx={{ minWidth: 35 }}>
                        {node.progress}%
                      </VuiTypography>
                    </VuiBox>
                  </Grid>
                  <Grid item xs={1.5} textAlign="right">
                    <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: `${node.statusColor}22`, display: "inline-block" }}>
                      <VuiTypography variant="caption" fontWeight="bold" sx={{ color: node.statusColor }}>
                        {node.status}
                      </VuiTypography>
                    </VuiBox>
                  </Grid>
                </Grid>
              </VuiBox>
            ))
          )}
        </Card>

        {/* FOTA Real Jobs History from SQLite */}
        <Card sx={cardSx}>
          <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={3}>
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              <VuiBox sx={{ width: 36, height: 36, borderRadius: "10px", background: "rgba(0, 117, 255, 0.2)", color: "#0075ff", display: "grid", placeItems: "center" }}>
                <IoTime size="18px" />
              </VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                Mesh FOTA Execution History (SQLite Real Data)
              </VuiTypography>
            </VuiBox>
            <IconButton onClick={loadJobs} sx={{ color: "rgba(255,255,255,0.6)" }}>
              <IoReload size="16px" />
            </IconButton>
          </VuiBox>

          <Grid container px={3} py={1} mb={1}>
            <Grid item xs={2.5}><VuiTypography variant="caption" color="text" fontWeight="medium">JOB ID</VuiTypography></Grid>
            <Grid item xs={1.5}><VuiTypography variant="caption" color="text" fontWeight="medium">VERSION</VuiTypography></Grid>
            <Grid item xs={3}><VuiTypography variant="caption" color="text" fontWeight="medium">DOWNLOAD ENDPOINT URL</VuiTypography></Grid>
            <Grid item xs={2.5}><VuiTypography variant="caption" color="text" fontWeight="medium">EXECUTED AT</VuiTypography></Grid>
            <Grid item xs={2.5} textAlign="right"><VuiTypography variant="caption" color="text" fontWeight="medium">RESULT</VuiTypography></Grid>
          </Grid>

          {historyJobs.length === 0 ? (
            <VuiBox p={4} textAlign="center">
              <VuiTypography variant="caption" color="text">
                No Mesh FOTA execution records found in SQLite database.
              </VuiTypography>
            </VuiBox>
          ) : (
            historyJobs.map((job) => {
              const isSuccess = job.status === "Completed" || job.status === "Success";
              const isFailed = job.status === "Failed";
              const color = isSuccess ? "#01f7a7" : isFailed ? "#ff285c" : "#0075ff";

              return (
                <VuiBox
                  key={job.id || job.jobId}
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
                      <VuiTypography variant="button" color="text" fontWeight="bold">{job.jobId}</VuiTypography>
                    </Grid>
                    <Grid item xs={1.5}>
                      <VuiTypography variant="button" color="white" fontWeight="bold">v{job.version}</VuiTypography>
                    </Grid>
                    <Grid item xs={3}>
                      <VuiTypography variant="caption" color="text" sx={{ wordBreak: "break-all" }}>
                        {job.firmwareUrl}
                      </VuiTypography>
                    </Grid>
                    <Grid item xs={2.5}>
                      <VuiTypography variant="caption" color="text">{new Date(job.startedAt).toLocaleString()}</VuiTypography>
                    </Grid>
                    <Grid item xs={2.5} textAlign="right">
                      <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: `${color}22`, display: "inline-block" }}>
                        <VuiTypography variant="caption" fontWeight="bold" sx={{ color: color }}>
                          {job.status?.toUpperCase()}
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

export default OtaNodes;
