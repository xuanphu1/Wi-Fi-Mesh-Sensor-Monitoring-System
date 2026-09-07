import { useMemo, useState, useEffect } from "react";

// react-router-dom components
import { useParams } from "react-router-dom";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Tabs from "@mui/material/Tabs";
import Tab from "@mui/material/Tab";
import Switch from "@mui/material/Switch";
import Button from "@mui/material/Button";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import Chip from "@mui/material/Chip";
import LinearProgress from "@mui/material/LinearProgress";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";
import { useMeshRealtime } from "context/meshRealtime";
import { resolveMrrErrorCode } from "utils/mrsErrorCodes";

import {
  IoCube,
  IoPerson,
  IoPulse,
  IoLayers,
  IoTime,
  IoCode,
  IoBarChart,
  IoHardwareChip,
  IoServer,
  IoWarning,
  IoSpeedometer,
  IoChevronForward,
  IoAnalytics,
  IoReload,
  IoFlash,
  IoBatteryCharging,
  IoToggle,
  IoPower,
  IoCheckmarkCircle,
  IoCloseCircle,
  IoShieldCheckmark,
  IoPlay,
  IoTimer,
} from "react-icons/io5";

const panelSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.10)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
  borderRadius: "16px",
  backdropFilter: "blur(18px)",
  height: "100%",
};

const tabBarSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.45) 0%, rgba(10, 14, 35, 0.35) 100%)",
  backdropFilter: "blur(16px)",
  borderRadius: "14px",
  padding: "6px",
  border: "1px solid rgba(255, 255, 255, 0.08)",
  "& .MuiTabs-indicator": { display: "none" },
};

const tabSx = {
  minHeight: "44px",
  textTransform: "none",
  fontWeight: "bold",
  fontSize: "14px",
  color: "rgba(255, 255, 255, 0.65)",
  borderRadius: "10px",
  px: 2.5,
  mr: 1,
  transition: "all 0.2s ease",
  "&.Mui-selected": {
    color: "#ffffff !important",
    background: "linear-gradient(135deg, rgba(0, 117, 255, 0.4) 0%, rgba(0, 117, 255, 0.15) 100%)",
    border: "1px solid rgba(0, 117, 255, 0.55)",
    boxShadow: "0 0 16px rgba(0, 117, 255, 0.3)",
  },
};

const solidInputSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "10px !important",
    background: "rgba(6, 11, 40, 0.6) !important",
    color: "#ffffff !important",
    height: "42px",
    border: "1px solid rgba(255, 255, 255, 0.12) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#0075ff !important", border: "1.5px solid #0075ff !important" },
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    fontSize: "14px !important",
    fontWeight: "600 !important",
    padding: "8px 12px !important",
  },
  "& .MuiSelect-select": {
    display: "flex",
    alignItems: "center",
    fontSize: "14px !important",
    fontWeight: "600 !important",
  },
};

function formatLatencyText(latencyMs) {
  if (typeof latencyMs !== "number" || Number.isNaN(latencyMs)) return "--";
  if (latencyMs < 1000) return `${Math.round(latencyMs)} ms`;
  return `${(latencyMs / 1000).toFixed(2)} s`;
}

function formatPacketLossText(packetLoss) {
  if (typeof packetLoss !== "number" || !Number.isFinite(packetLoss)) return "-";
  return `${packetLoss}%`;
}

function safeDecodeRouteParam(value) {
  if (typeof value !== "string") return "";
  try {
    return decodeURIComponent(value);
  } catch {
    return value;
  }
}

const iconMap = {
  Name: <IoPerson size="14px" color="#38bdf8" />,
  "Node IP": <VuiTypography variant="caption" fontWeight="bold" color="success">IP</VuiTypography>,
  Status: <IoPulse size="14px" color="#4ade80" />,
  "Mesh level": <IoLayers size="14px" color="#b8a9ff" />,
  "Last seen": <IoTime size="14px" color="rgba(255,255,255,0.7)" />,
  "Reconnect count": <IoReload size="14px" color="#ffb547" />,
  "Schema version": <IoCode size="14px" color="rgba(255,255,255,0.7)" />,
  "Packet loss": <IoBarChart size="14px" color="#38bdf8" />,
  Firmware: <IoHardwareChip size="14px" color="rgba(255,255,255,0.7)" />,
  "Port count": <IoServer size="14px" color="rgba(255,255,255,0.7)" />,
  "Runtime errors": <IoWarning size="14px" color="#ef4444" />,
  "Node RTC": <IoTime size="14px" color="rgba(255,255,255,0.7)" />,
  Latency: <IoSpeedometer size="14px" color="rgba(255,255,255,0.7)" />,
};

function Sparkline({ data = [] }) {
  if (data.length < 2) {
    return (
      <svg viewBox="0 0 100 30" width="100" height="30" preserveAspectRatio="none" style={{ display: "block" }}>
        <line x1="0" y1="15" x2="100" y2="15" stroke="rgba(79, 56, 223, 0.3)" strokeWidth="2" strokeDasharray="4" />
      </svg>
    );
  }

  const width = 100;
  const height = 30;
  const padding = 2;
  const innerHeight = height - padding * 2;

  const min = Math.min(...data);
  const max = Math.max(...data);
  const range = max - min || 1;

  const points = data.map((val, i) => {
    const x = (i / (data.length - 1)) * width;
    const y = padding + innerHeight - ((val - min) / range) * innerHeight;
    return { x, y };
  });

  const linePath = points.map((p, i) => `${i === 0 ? "M" : "L"}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(" ");
  const areaPath = `${linePath} L${width},${height} L0,${height} Z`;

  return (
    <svg viewBox={`0 0 ${width} ${height}`} width="100" height="30" preserveAspectRatio="none" style={{ display: "block" }}>
      <defs>
        <linearGradient id="glowGradient" x1="0" x2="0" y1="0" y2="1">
          <stop offset="0%" stopColor="rgba(79, 56, 223, 0.5)" />
          <stop offset="100%" stopColor="rgba(79, 56, 223, 0.0)" />
        </linearGradient>
      </defs>
      <path d={areaPath} fill="url(#glowGradient)" />
      <path d={linePath} fill="none" stroke="#4F38DF" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

function NodeDetail() {
  const { nodeId } = useParams();
  const decodedNodeId = useMemo(() => safeDecodeRouteParam(nodeId), [nodeId]);
  const { nodes } = useMeshNodesFromWebSocket();
  const { sendMessage, nodePowerStates, nodeActuatorStates } = useMeshRealtime();

  const [activeTab, setActiveTab] = useState("sensors");
  const [sensorHistories, setSensorHistories] = useState({});
  const [lastActionStatus, setLastActionStatus] = useState(null);

  const node = useMemo(
    () => nodes.find((n) => n.id === decodedNodeId || n.ip === decodedNodeId),
    [nodes, decodedNodeId]
  );

  const nodeKey = useMemo(() => {
    return node?.mac || node?.ip || decodedNodeId || "";
  }, [node, decodedNodeId]);

  // --- POWER MANAGER STATE (IP5306 + 5V/3V3 Rails) ---
  const [rail5v, setRail5v] = useState(true);
  const [rail3v3, setRail3v3] = useState(true);
  const [ip5306Boost, setIp5306Boost] = useState(true);
  const [ip5306LightLoad, setIp5306LightLoad] = useState(false);
  const [ip5306KeyMode, setIp5306KeyMode] = useState("double_click");
  const [batteryPercent, setBatteryPercent] = useState(85);
  const [batteryVoltage, setBatteryVoltage] = useState(4.12);
  const [isCharging, setIsCharging] = useState(false);

  // Sync with incoming telemetry if available
  useEffect(() => {
    const pState = nodePowerStates[nodeKey];
    if (pState) {
      if (pState.battery) {
        if (typeof pState.battery.percent === "number") setBatteryPercent(pState.battery.percent);
        if (typeof pState.battery.voltage_v === "number") setBatteryVoltage(pState.battery.voltage_v);
        if (typeof pState.battery.is_charging === "boolean") setIsCharging(pState.battery.is_charging);
      }
      if (pState.rails) {
        if (typeof pState.rails.rail_5v === "boolean") setRail5v(pState.rails.rail_5v);
        if (typeof pState.rails.rail_3v3 === "boolean") setRail3v3(pState.rails.rail_3v3);
      }
      if (pState.ip5306) {
        if (typeof pState.ip5306.boost_enabled === "boolean") setIp5306Boost(pState.ip5306.boost_enabled);
        if (typeof pState.ip5306.light_load_shutdown === "boolean") setIp5306LightLoad(pState.ip5306.light_load_shutdown);
        if (pState.ip5306.key_off_mode) setIp5306KeyMode(pState.ip5306.key_off_mode);
      }
    }
  }, [nodePowerStates, nodeKey]);

  // --- ACTUATOR STATE (3 Channels) ---
  const [actuators, setActuators] = useState([
    { id: 1, name: "Actuator 1 (Relay 1)", state: false, mode: "manual", pulseSec: 5, autoSensor: "temp_C", autoOp: ">", autoVal: 35 },
    { id: 2, name: "Actuator 2 (Relay 2)", state: false, mode: "manual", pulseSec: 5, autoSensor: "co2_ppm", autoOp: ">", autoVal: 1000 },
    { id: 3, name: "Actuator 3 (Relay 3)", state: false, mode: "manual", pulseSec: 5, autoSensor: "pm2_5_ugm3", autoOp: ">", autoVal: 50 },
  ]);

  // Sync with incoming actuator telemetry
  useEffect(() => {
    const aState = nodeActuatorStates[nodeKey];
    if (aState && Array.isArray(aState.actuators)) {
      setActuators((prev) =>
        prev.map((ch) => {
          const remote = aState.actuators.find((r) => r.channel === ch.id);
          if (remote) {
            return {
              ...ch,
              name: remote.name || ch.name,
              state: Boolean(remote.state),
              mode: remote.mode || ch.mode,
            };
          }
          return ch;
        })
      );
    }
  }, [nodeActuatorStates, nodeKey]);

  // Dispatch Power Rails command
  const handleToggleRail = (railName, nextVal) => {
    const next5v = railName === "5v" ? nextVal : rail5v;
    const next3v3 = railName === "3v3" ? nextVal : rail3v3;
    if (railName === "5v") setRail5v(nextVal);
    if (railName === "3v3") setRail3v3(nextVal);

    const payload = {
      type: "node_power_cmd",
      targetMac: node?.mac || node?.ip || decodedNodeId,
      targetIp: node?.ip || "",
      command: "set_power_rails",
      data: {
        rail_5v: next5v ? 1 : 0,
        rail_3v3: next3v3 ? 1 : 0,
      },
      timestamp: Math.floor(Date.now() / 1000),
    };

    sendMessage(payload);
    setLastActionStatus({
      time: new Date().toLocaleTimeString(),
      text: `Power Rails Updated: 5V Rail=${next5v ? "ON" : "OFF"}, 3V3 Rail=${next3v3 ? "ON" : "OFF"}`,
      payload,
    });
  };

  // Dispatch IP5306 Config command
  const handleSaveIp5306Config = () => {
    const payload = {
      type: "node_power_cmd",
      targetMac: node?.mac || node?.ip || decodedNodeId,
      targetIp: node?.ip || "",
      command: "set_ip5306",
      data: {
        boost_enable: ip5306Boost ? 1 : 0,
        light_load_off: ip5306LightLoad ? 1 : 0,
        key_off_mode: ip5306KeyMode,
      },
      timestamp: Math.floor(Date.now() / 1000),
    };

    sendMessage(payload);
    setLastActionStatus({
      time: new Date().toLocaleTimeString(),
      text: `IP5306 Config Dispatched: Boost=${ip5306Boost ? "ON" : "OFF"}, LightLoad=${ip5306LightLoad ? "ON" : "OFF"}, Key=${ip5306KeyMode}`,
      payload,
    });
  };

  // Dispatch Actuator Channel command
  const handleToggleActuator = (chId, nextState) => {
    setActuators((prev) =>
      prev.map((c) => (c.id === chId ? { ...c, state: nextState } : c))
    );
    const targetCh = actuators.find((c) => c.id === chId);

    const payload = {
      type: "node_actuator_cmd",
      targetMac: node?.mac || node?.ip || decodedNodeId,
      targetIp: node?.ip || "",
      channel: chId,
      action: "SET_STATE",
      state: nextState ? 1 : 0,
      mode: targetCh?.mode || "manual",
      timestamp: Math.floor(Date.now() / 1000),
    };

    sendMessage(payload);
    setLastActionStatus({
      time: new Date().toLocaleTimeString(),
      text: `${targetCh?.name || `Channel ${chId}`} State -> ${nextState ? "ON (HIGH)" : "OFF (LOW)"}`,
      payload,
    });
  };

  // Pulse Actuator
  const handlePulseActuator = (chId, seconds) => {
    const targetCh = actuators.find((c) => c.id === chId);
    setActuators((prev) =>
      prev.map((c) => (c.id === chId ? { ...c, state: true } : c))
    );

    const payload = {
      type: "node_actuator_cmd",
      targetMac: node?.mac || node?.ip || decodedNodeId,
      targetIp: node?.ip || "",
      channel: chId,
      action: "PULSE",
      state: 1,
      mode: "pulse",
      pulse_duration_s: seconds,
      timestamp: Math.floor(Date.now() / 1000),
    };

    sendMessage(payload);
    setLastActionStatus({
      time: new Date().toLocaleTimeString(),
      text: `${targetCh?.name || `Channel ${chId}`} Pulse Triggered (${seconds}s)`,
      payload,
    });

    setTimeout(() => {
      setActuators((prev) =>
        prev.map((c) => (c.id === chId ? { ...c, state: false } : c))
      );
    }, seconds * 1000);
  };

  // Emergency All OFF
  const handleEmergencyAllOff = () => {
    setActuators((prev) => prev.map((c) => ({ ...c, state: false })));
    const payload = {
      type: "node_actuator_cmd",
      targetMac: node?.mac || node?.ip || decodedNodeId,
      targetIp: node?.ip || "",
      action: "EMERGENCY_ALL_OFF",
      timestamp: Math.floor(Date.now() / 1000),
    };
    sendMessage(payload);
    setLastActionStatus({
      time: new Date().toLocaleTimeString(),
      text: "EMERGENCY: All 3 Actuators Disengaged (OFF)",
      payload,
    });
  };

  const sensorRows = useMemo(() => {
    if (!node || !Array.isArray(node.ports)) return [];
    const rows = [];
    node.ports.forEach((portRow) => {
      const readings = Array.isArray(portRow.readings) ? portRow.readings : [];
      readings.forEach((r) => {
        rows.push({
          key: `${node.ip || node.id}:${portRow.wirePort}:${r.key}:${r.index}`,
          sensorName: portRow.sensorName || "Unknown sensor",
          port: portRow.wirePort,
          label: r.label || r.key || "value",
          unit: r.unit || "",
          value: r.value,
        });
      });
    });
    return rows;
  }, [node]);

  useEffect(() => {
    if (sensorRows.length === 0) return;

    setSensorHistories((prev) => {
      const next = { ...prev };
      let changed = false;

      sensorRows.forEach((s) => {
        const val = Number(s.value);
        if (Number.isFinite(val)) {
          const history = next[s.key] || [];
          next[s.key] = [...history, val].slice(-20);
          changed = true;
        }
      });

      return changed ? next : prev;
    });
  }, [sensorRows]);

  const infoRows = useMemo(() => {
    if (!node) return [];
    return [
      { label: "Name", value: node.name || "Mesh Node" },
      { label: "Node IP", value: node.ip || "-" },
      {
        label: "Status",
        value: node.online ? "ONLINE" : "OFFLINE",
        valueColor: node.online ? "success" : "error",
      },
      { label: "Mesh level", value: node.meshLevel != null ? String(node.meshLevel) : "-" },
      {
        label: "Last seen",
        value: node.lastSeenIso ? new Date(node.lastSeenIso).toLocaleString() : "-",
      },
      {
        label: "Reconnect count",
        value: `${node.reconnectCount || 0} times`,
        valueColor: (node.reconnectCount || 0) > 0 ? "warning" : "success",
      },
      { label: "Schema version", value: node.schemaVersion != null ? String(node.schemaVersion) : "-" },
      {
        label: "Packet loss",
        value: formatPacketLossText(node.packetLoss),
        valueColor: typeof node.packetLoss === "number" && node.packetLoss === 0 ? "success" : "warning",
      },
      { label: "Firmware", value: node.firmwareVersion || "0.0.0" },
      { label: "Port count", value: node.portCount != null ? String(node.portCount) : "0" },
      {
        label: "Runtime errors",
        value: Array.isArray(node.runtimeErrors) && node.runtimeErrors.length > 0 ? `${node.runtimeErrors.length} code(s)` : "None",
        valueColor:
          Array.isArray(node.runtimeErrors) && node.runtimeErrors.length > 0 ? "warning" : "success",
      },
      { label: "Node RTC", value: node.rtcIso || "-" },
      {
        label: "Latency",
        value: formatLatencyText(node.latencyMs),
        valueColor: typeof node.latencyMs === "number" ? "success" : "warning",
      },
    ];
  }, [node]);

  const runtimeErrorDetails = useMemo(() => {
    const codes = Array.isArray(node?.runtimeErrors) ? node.runtimeErrors : [];
    return codes.map((code) => resolveMrrErrorCode(code));
  }, [node]);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <VuiBox display="flex" justifyContent="space-between" alignItems="center" flexWrap="wrap" gap={2} mb={3}>
          <VuiTypography variant="lg" color="white" fontWeight="bold" display="flex" alignItems="center">
            Node: <span style={{ color: "#38bdf8", marginLeft: "8px" }}>{decodedNodeId}</span>
          </VuiTypography>

          {/* Quick Action feedback badge */}
          {lastActionStatus && (
            <VuiBox
              px={2}
              py={0.8}
              borderRadius="10px"
              display="flex"
              alignItems="center"
              gap={1}
              sx={{
                background: "rgba(0, 117, 255, 0.15)",
                border: "1px solid rgba(0, 117, 255, 0.4)",
              }}
            >
              <IoCheckmarkCircle color="#0075ff" size="16px" />
              <VuiTypography variant="caption" color="white" fontWeight="medium">
                [{lastActionStatus.time}] {lastActionStatus.text}
              </VuiTypography>
            </VuiBox>
          )}
        </VuiBox>

        {!node ? (
          <Card sx={panelSx}>
            <VuiBox p={3}>
              <VuiTypography color="white" variant="button" fontWeight="medium">
                Node not found in current mesh network snapshot.
              </VuiTypography>
            </VuiBox>
          </Card>
        ) : (
          <Grid container spacing={3}>
            {/* Left Column: Node Info (Fixed Specs) */}
            <Grid item xs={12} lg={4} xl={4}>
              <Card sx={panelSx}>
                <VuiBox p={2.5}>
                  <VuiBox display="flex" alignItems="center" mb={3}>
                    <VuiBox
                      width="36px"
                      height="36px"
                      borderRadius="10px"
                      display="flex"
                      justifyContent="center"
                      alignItems="center"
                      mr={2}
                      sx={{ background: "rgba(184, 169, 255, 0.15)", border: "1px solid rgba(184, 169, 255, 0.35)" }}
                    >
                      <IoCube color="#b8a9ff" size="20px" />
                    </VuiBox>
                    <VuiBox>
                      <VuiTypography variant="h6" color="white" fontWeight="bold">
                        Node Metadata
                      </VuiTypography>
                      <VuiTypography variant="caption" color="text">
                        Hardware & network status
                      </VuiTypography>
                    </VuiBox>
                  </VuiBox>

                  <VuiBox display="flex" flexDirection="column">
                    {infoRows.map((row, index) => {
                      let rightElement;
                      if (row.label === "Status") {
                        rightElement = (
                          <VuiBox display="flex" alignItems="center" gap={1}>
                            <VuiBox
                              width="8px"
                              height="8px"
                              borderRadius="50%"
                              bgColor={row.valueColor}
                              sx={{ boxShadow: `0 0 8px ${row.valueColor === "success" ? "#4ade80" : "#ef4444"}` }}
                            />
                            <VuiTypography color={row.valueColor} variant="button" fontWeight="bold">
                              {row.value}
                            </VuiTypography>
                          </VuiBox>
                        );
                      } else if (row.label === "Mesh level" && row.value !== "-") {
                        const levelNum = Number(row.value);
                        const levelColors = {
                          0: { color: "#01f7a7", bg: "rgba(1, 247, 167, 0.20)", border: "1px solid rgba(1, 247, 167, 0.50)" },
                          1: { color: "#00d4ff", bg: "rgba(0, 212, 255, 0.20)", border: "1px solid rgba(0, 212, 255, 0.50)" },
                          2: { color: "#b8a9ff", bg: "rgba(138, 44, 255, 0.20)", border: "1px solid rgba(138, 44, 255, 0.50)" },
                          3: { color: "#38bdf8", bg: "rgba(56, 189, 248, 0.20)", border: "1px solid rgba(56, 189, 248, 0.50)" },
                          4: { color: "#ffb547", bg: "rgba(255, 181, 71, 0.20)", border: "1px solid rgba(255, 181, 71, 0.50)" },
                        };
                        const style = levelColors[levelNum] || { color: "#a3e635", bg: "rgba(163, 230, 53, 0.20)", border: "1px solid rgba(163, 230, 53, 0.50)" };
                        rightElement = (
                          <VuiBox px={1.8} py={0.3} borderRadius="8px" sx={{ background: style.bg, border: style.border, boxShadow: `0 0 8px ${style.color}22` }}>
                            <VuiTypography variant="caption" fontWeight="bold" sx={{ color: style.color }}>
                              Level {row.value}
                            </VuiTypography>
                          </VuiBox>
                        );
                      } else {
                        rightElement = (
                          <VuiTypography
                            color={row.valueColor || "white"}
                            variant="button"
                            fontWeight={row.label === "Name" || row.label === "Node IP" ? "bold" : "medium"}
                            sx={{ textAlign: "right", wordBreak: "break-word" }}
                          >
                            {row.value}
                          </VuiTypography>
                        );
                      }

                      return (
                        <VuiBox
                          key={row.label}
                          py={1.3}
                          sx={{
                            borderBottom: index !== infoRows.length - 1 ? "1px solid rgba(255,255,255,0.05)" : "none",
                            display: "flex",
                            alignItems: "center",
                            justifyContent: "space-between",
                          }}
                        >
                          <VuiBox display="flex" alignItems="center">
                            <VuiBox
                              width="28px"
                              height="28px"
                              borderRadius="8px"
                              display="flex"
                              justifyContent="center"
                              alignItems="center"
                              mr={2}
                              sx={{ background: "rgba(255, 255, 255, 0.05)", border: "1px solid rgba(255, 255, 255, 0.10)" }}
                            >
                              {iconMap[row.label]}
                            </VuiBox>
                            <VuiTypography color="text" variant="button" fontWeight="medium">
                              {row.label}
                            </VuiTypography>
                          </VuiBox>
                          <VuiBox pl={2}>{rightElement}</VuiBox>
                        </VuiBox>
                      );
                    })}
                  </VuiBox>
                </VuiBox>
              </Card>
            </Grid>

            {/* Right Column: Tab Navigation (Sensors / Power Manager / Actuators) */}
            <Grid item xs={12} lg={8} xl={8}>
              <VuiBox display="flex" flexDirection="column" gap={2.5}>
                {/* Navigation Tabs */}
                <Tabs value={activeTab} onChange={(e, val) => setActiveTab(val)} sx={tabBarSx}>
                  <Tab icon={<IoAnalytics size="17px" />} iconPosition="start" label="Sensors & Health" value="sensors" sx={tabSx} />
                  <Tab icon={<IoFlash size="17px" />} iconPosition="start" label="Power Manager (IP5306)" value="power" sx={tabSx} />
                  <Tab icon={<IoToggle size="17px" />} iconPosition="start" label="Actuators (3 Channels)" value="actuator" sx={tabSx} />
                </Tabs>

                {/* TAB 1: SENSORS & HEALTH */}
                {activeTab === "sensors" && (
                  <>
                    {/* Runtime error details */}
                    <Card sx={{ ...panelSx, height: "auto" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={2}>
                          <VuiBox display="flex" alignItems="center">
                            <VuiBox width="32px" height="32px" borderRadius="8px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(239, 68, 68, 0.1)", border: "1px solid rgba(239, 68, 68, 0.2)" }}>
                              <IoWarning color="#ef4444" size="18px" />
                            </VuiBox>
                            <VuiTypography variant="h6" color="white" fontWeight="bold">
                              Runtime error details
                            </VuiTypography>
                          </VuiBox>
                          {runtimeErrorDetails.length > 0 && (
                            <VuiBox px={1.5} py={0.5} borderRadius="8px" sx={{ border: "1px solid rgba(245, 158, 11, 0.5)" }}>
                              <VuiTypography color="warning" variant="caption" fontWeight="bold">
                                {runtimeErrorDetails.length} code(s)
                              </VuiTypography>
                            </VuiBox>
                          )}
                        </VuiBox>

                        {runtimeErrorDetails.length === 0 ? (
                          <VuiBox p={2} textAlign="center">
                            <VuiTypography color="text" variant="button">
                              No runtime errors reported.
                            </VuiTypography>
                          </VuiBox>
                        ) : (
                          <VuiBox display="grid" gap={1.5}>
                            {runtimeErrorDetails.map((item, idx) => (
                              <VuiBox
                                key={`${item.rawCode || "unknown"}-${idx}`}
                                px={2}
                                py={1.5}
                                sx={{
                                  borderRadius: "12px",
                                  border: "1px solid rgba(255,255,255,0.05)",
                                  background: "rgba(255,255,255,0.01)",
                                  display: "flex",
                                  justifyContent: "space-between",
                                  alignItems: "center",
                                }}
                              >
                                <VuiBox>
                                  <VuiTypography color="warning" variant="button" fontWeight="bold" display="block" mb={0.5}>
                                    {item.normalized || String(item.rawCode || "-")} - {item.summary || "Unknown short code"}
                                  </VuiTypography>
                                  {item.matches.length === 0 ? (
                                    <VuiTypography color="text" variant="caption">
                                      No definition matched from ErrorCodes.h mapping.
                                    </VuiTypography>
                                  ) : (
                                    item.matches.slice(0, 3).map((m) => (
                                      <VuiTypography key={`${m.symbol}-${m.codeHex}`} color="text" variant="caption" display="block">
                                        {m.symbol} | {m.codeHex} | {m.moduleLabel} | {m.description}
                                      </VuiTypography>
                                    ))
                                  )}
                                </VuiBox>
                                <VuiBox display="flex" alignItems="center" gap={2}>
                                  <VuiBox display="flex" alignItems="center" gap={0.5}>
                                    <IoTime color="rgba(255,255,255,0.5)" size="12px" />
                                    <VuiTypography color="text" variant="caption">
                                      {node.lastSeenIso ? new Date(node.lastSeenIso).toLocaleString() : "Unknown"}
                                    </VuiTypography>
                                  </VuiBox>
                                  <IoChevronForward color="rgba(255,255,255,0.3)" size="16px" />
                                </VuiBox>
                              </VuiBox>
                            ))}
                          </VuiBox>
                        )}
                      </VuiBox>
                    </Card>

                    {/* Realtime sensor data */}
                    <Card sx={{ ...panelSx, height: "auto", minHeight: "250px" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" alignItems="center" mb={3}>
                          <VuiBox width="32px" height="32px" borderRadius="8px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(56, 189, 248, 0.15)", border: "1px solid rgba(56, 189, 248, 0.35)" }}>
                            <IoAnalytics color="#38bdf8" size="18px" />
                          </VuiBox>
                          <VuiTypography variant="h6" color="white" fontWeight="bold">
                            Realtime sensor data (from this Node)
                          </VuiTypography>
                        </VuiBox>

                        {sensorRows.length === 0 ? (
                          <VuiBox p={2} textAlign="center">
                            <VuiTypography color="text" variant="button">
                              No sensor data received from this node yet.
                            </VuiTypography>
                          </VuiBox>
                        ) : (
                          <Grid container spacing={2}>
                            {sensorRows.map((s) => (
                              <Grid key={s.key} item xs={12} sm={6}>
                                <VuiBox
                                  p={2}
                                  sx={{
                                    height: "100%",
                                    borderRadius: "12px",
                                    border: "1px solid rgba(255,255,255,0.05)",
                                    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.9) 0%, rgba(10, 14, 35, 0.9) 100%)",
                                    position: "relative",
                                    overflow: "hidden",
                                  }}
                                >
                                  <VuiBox display="flex" justifyContent="space-between" alignItems="flex-start" mb={1}>
                                    <VuiBox>
                                      <VuiTypography color="white" variant="button" fontWeight="bold" display="block">
                                        {s.sensorName}
                                      </VuiTypography>
                                      <VuiTypography color="text" variant="caption" display="block" sx={{ opacity: 0.8 }}>
                                        Port {s.port} - {s.label}
                                      </VuiTypography>
                                    </VuiBox>
                                    <VuiTypography color="info" variant="button" fontWeight="bold">
                                      {s.unit}
                                    </VuiTypography>
                                  </VuiBox>
                                  <VuiBox display="flex" justifyContent="space-between" alignItems="flex-end" mt={2}>
                                    <VuiTypography color="white" variant="h4" fontWeight="bold">
                                      {Number.isFinite(Number(s.value)) ? Number(s.value).toFixed(3) : String(s.value)}
                                    </VuiTypography>
                                    <VuiBox opacity={0.9} mb="-4px" width="100px">
                                      <Sparkline data={sensorHistories[s.key]} />
                                    </VuiBox>
                                  </VuiBox>
                                </VuiBox>
                              </Grid>
                            ))}
                          </Grid>
                        )}
                      </VuiBox>
                    </Card>
                  </>
                )}

                {/* TAB 2: POWER MANAGER (IP5306 + POWER RAILS) */}
                {activeTab === "power" && (
                  <>
                    {/* Battery & IP5306 Live Telemetry */}
                    <Card sx={{ ...panelSx, height: "auto" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" justifyContent="space-between" alignItems="center" flexWrap="wrap" gap={1} mb={2.5}>
                          <VuiBox display="flex" alignItems="center">
                            <VuiBox width="36px" height="36px" borderRadius="10px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(1, 247, 167, 0.15)", border: "1px solid rgba(1, 247, 167, 0.35)" }}>
                              <IoBatteryCharging color="#01f7a7" size="20px" />
                            </VuiBox>
                            <VuiBox>
                              <VuiTypography variant="h6" color="white" fontWeight="bold">
                                IP5306 Power & Battery Telemetry
                              </VuiTypography>
                              <VuiTypography variant="caption" color="text">
                                Power Management IC (I2C address 0x75)
                              </VuiTypography>
                            </VuiBox>
                          </VuiBox>
                          <Chip
                            label={isCharging ? "⚡ CHARGING" : "DISCHARGING"}
                            size="small"
                            sx={{
                              background: isCharging ? "rgba(1, 247, 167, 0.2)" : "rgba(0, 117, 255, 0.15)",
                              border: `1px solid ${isCharging ? "#01f7a7" : "#0075ff"}`,
                              color: isCharging ? "#01f7a7" : "#0075ff",
                              fontWeight: "bold",
                            }}
                          />
                        </VuiBox>

                        <Grid container spacing={2.5} alignItems="center">
                          <Grid item xs={12} sm={4}>
                            <VuiBox p={2} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.03)", border: "1px solid rgba(255,255,255,0.08)" }}>
                              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>
                                Battery Level
                              </VuiTypography>
                              <VuiTypography variant="h3" color="white" fontWeight="bold">
                                {batteryPercent}%
                              </VuiTypography>
                              <VuiBox mt={1.5}>
                                <LinearProgress
                                  variant="determinate"
                                  value={batteryPercent}
                                  sx={{
                                    height: 8,
                                    borderRadius: 4,
                                    backgroundColor: "rgba(255,255,255,0.1)",
                                    "& .MuiLinearProgress-bar": {
                                      backgroundColor: batteryPercent > 50 ? "#01f7a7" : batteryPercent > 20 ? "#ffb547" : "#ef4444",
                                    },
                                  }}
                                />
                              </VuiBox>
                            </VuiBox>
                          </Grid>

                          <Grid item xs={6} sm={4}>
                            <VuiBox p={2} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.03)", border: "1px solid rgba(255,255,255,0.08)" }}>
                              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>
                                Battery Voltage
                              </VuiTypography>
                              <VuiTypography variant="h4" color="white" fontWeight="bold">
                                {batteryVoltage.toFixed(2)} V
                              </VuiTypography>
                              <VuiTypography variant="caption" color="text" sx={{ mt: 1, display: "block" }}>
                                Full: 4.20V | Cutoff: 3.00V
                              </VuiTypography>
                            </VuiBox>
                          </Grid>

                          <Grid item xs={6} sm={4}>
                            <VuiBox p={2} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.03)", border: "1px solid rgba(255,255,255,0.08)" }}>
                              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>
                                Charging Mode
                              </VuiTypography>
                              <VuiTypography variant="h5" color="white" fontWeight="bold">
                                {isCharging ? "Constant Current" : "Battery Standalone"}
                              </VuiTypography>
                              <VuiTypography variant="caption" color="text" sx={{ mt: 1, display: "block" }}>
                                Vin: 5.0V USB/Solar
                              </VuiTypography>
                            </VuiBox>
                          </Grid>
                        </Grid>
                      </VuiBox>
                    </Card>

                    {/* ⚡ POWER RAILS CONTROL (5V & 3.3V) */}
                    <Card sx={{ ...panelSx, height: "auto" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" alignItems="center" mb={1}>
                          <VuiBox width="36px" height="36px" borderRadius="10px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(255, 181, 71, 0.15)", border: "1px solid rgba(255, 181, 71, 0.35)" }}>
                            <IoPower color="#ffb547" size="20px" />
                          </VuiBox>
                          <VuiBox>
                            <VuiTypography variant="h6" color="white" fontWeight="bold">
                              Peripheral Power Rails (Sensor Supply)
                            </VuiTypography>
                            <VuiTypography variant="caption" color="text">
                              Toggle power domains to reduce sleep power consumption
                            </VuiTypography>
                          </VuiBox>
                        </VuiBox>

                        <Grid container spacing={2.5} mt={1}>
                          {/* 5V Power Rail */}
                          <Grid item xs={12} sm={6}>
                            <VuiBox
                              p={2.5}
                              borderRadius="14px"
                              sx={{
                                background: rail5v ? "linear-gradient(135deg, rgba(0, 117, 255, 0.16) 0%, rgba(0, 117, 255, 0.04) 100%)" : "rgba(255,255,255,0.02)",
                                border: rail5v ? "1.5px solid rgba(0, 117, 255, 0.45)" : "1px solid rgba(255,255,255,0.08)",
                                transition: "all 0.25s ease",
                              }}
                            >
                              <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1.5}>
                                <VuiBox display="flex" alignItems="center" gap={1.2}>
                                  <VuiBox width="10px" height="10px" borderRadius="50%" sx={{ background: rail5v ? "#01f7a7" : "#64748b", boxShadow: rail5v ? "0 0 10px #01f7a7" : "none" }} />
                                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                                    5V Power Rail
                                  </VuiTypography>
                                </VuiBox>
                                <Switch checked={rail5v} onChange={(e) => handleToggleRail("5v", e.target.checked)} color="primary" />
                              </VuiBox>

                              <Chip
                                label={rail5v ? "5.0V OUTPUT ACTIVE" : "5.0V POWER OFF"}
                                size="small"
                                sx={{
                                  background: rail5v ? "rgba(1, 247, 167, 0.15)" : "rgba(255,255,255,0.05)",
                                  color: rail5v ? "#01f7a7" : "rgba(255,255,255,0.5)",
                                  fontWeight: "bold",
                                  fontSize: "11px",
                                  mb: 1.5,
                                }}
                              />

                              <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "12.5px" }}>
                                Powers high-drain 5V sensors: <strong>PMS7003</strong> (Dust / Particulate) & <strong>MH-Z14A</strong> (CO₂ NDIR).
                              </VuiTypography>
                            </VuiBox>
                          </Grid>

                          {/* 3.3V Power Rail */}
                          <Grid item xs={12} sm={6}>
                            <VuiBox
                              p={2.5}
                              borderRadius="14px"
                              sx={{
                                background: rail3v3 ? "linear-gradient(135deg, rgba(138, 44, 255, 0.16) 0%, rgba(138, 44, 255, 0.04) 100%)" : "rgba(255,255,255,0.02)",
                                border: rail3v3 ? "1.5px solid rgba(138, 44, 255, 0.45)" : "1px solid rgba(255,255,255,0.08)",
                                transition: "all 0.25s ease",
                              }}
                            >
                              <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1.5}>
                                <VuiBox display="flex" alignItems="center" gap={1.2}>
                                  <VuiBox width="10px" height="10px" borderRadius="50%" sx={{ background: rail3v3 ? "#01f7a7" : "#64748b", boxShadow: rail3v3 ? "0 0 10px #01f7a7" : "none" }} />
                                  <VuiTypography variant="h6" color="white" fontWeight="bold">
                                    3.3V Power Rail
                                  </VuiTypography>
                                </VuiBox>
                                <Switch checked={rail3v3} onChange={(e) => handleToggleRail("3v3", e.target.checked)} color="secondary" />
                              </VuiBox>

                              <Chip
                                label={rail3v3 ? "3.3V OUTPUT ACTIVE" : "3.3V POWER OFF"}
                                size="small"
                                sx={{
                                  background: rail3v3 ? "rgba(1, 247, 167, 0.15)" : "rgba(255,255,255,0.05)",
                                  color: rail3v3 ? "#01f7a7" : "rgba(255,255,255,0.5)",
                                  fontWeight: "bold",
                                  fontSize: "11px",
                                  mb: 1.5,
                                }}
                              />

                              <VuiTypography variant="caption" color="text" display="block" sx={{ fontSize: "12.5px" }}>
                                Powers 3.3V I2C/SPI bus sensors: <strong>BME280</strong>, <strong>AHT10</strong>, <strong>DHT22</strong>.
                              </VuiTypography>
                            </VuiBox>
                          </Grid>
                        </Grid>
                      </VuiBox>
                    </Card>

                    {/* IP5306 Registers Configuration */}
                    <Card sx={{ ...panelSx, height: "auto" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" alignItems="center" mb={2.5}>
                          <VuiBox width="36px" height="36px" borderRadius="10px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(56, 189, 248, 0.15)", border: "1px solid rgba(56, 189, 248, 0.35)" }}>
                            <IoHardwareChip color="#38bdf8" size="20px" />
                          </VuiBox>
                          <VuiBox>
                            <VuiTypography variant="h6" color="white" fontWeight="bold">
                              IP5306 Operating Registers Config
                            </VuiTypography>
                            <VuiTypography variant="caption" color="text">
                              Tune boost converter & automatic shutdown parameters
                            </VuiTypography>
                          </VuiBox>
                        </VuiBox>

                        <Grid container spacing={2.5}>
                          <Grid item xs={12} sm={6}>
                            <VuiBox p={2} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.03)", border: "1px solid rgba(255,255,255,0.08)" }}>
                              <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                                <VuiBox>
                                  <VuiTypography variant="button" color="white" fontWeight="bold">
                                    Boost 5V Output Circuit
                                  </VuiTypography>
                                  <VuiTypography variant="caption" color="text" display="block">
                                    Enable synchronous step-up 5V converter
                                  </VuiTypography>
                                </VuiBox>
                                <Switch checked={ip5306Boost} onChange={(e) => setIp5306Boost(e.target.checked)} />
                              </VuiBox>
                            </VuiBox>
                          </Grid>

                          <Grid item xs={12} sm={6}>
                            <VuiBox p={2} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.03)", border: "1px solid rgba(255,255,255,0.08)" }}>
                              <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                                <VuiBox>
                                  <VuiTypography variant="button" color="white" fontWeight="bold">
                                    Light-Load Auto Shutdown
                                  </VuiTypography>
                                  <VuiTypography variant="caption" color="text" display="block">
                                    Shut down if current &lt; 45mA (Recommended OFF)
                                  </VuiTypography>
                                </VuiBox>
                                <Switch checked={ip5306LightLoad} onChange={(e) => setIp5306LightLoad(e.target.checked)} />
                              </VuiBox>
                            </VuiBox>
                          </Grid>

                          <Grid item xs={12} sm={6}>
                            <VuiTypography variant="button" color="white" fontWeight="bold" display="block" mb={1}>
                              Power Key Off Mode
                            </VuiTypography>
                            <TextField
                              select
                              fullWidth
                              value={ip5306KeyMode}
                              onChange={(e) => setIp5306KeyMode(e.target.value)}
                              sx={solidInputSx}
                            >
                              <MenuItem value="double_click">Double Click to Shutdown</MenuItem>
                              <MenuItem value="long_press">Long Press (&gt; 2s) to Shutdown</MenuItem>
                            </TextField>
                          </Grid>

                          <Grid item xs={12} sm={6} display="flex" alignItems="flex-end">
                            <Button
                              fullWidth
                              variant="contained"
                              onClick={handleSaveIp5306Config}
                              sx={{
                                height: "42px",
                                background: "linear-gradient(135deg, #0075ff 0%, #0056cc 100%)",
                                color: "#fff",
                                fontWeight: "bold",
                                borderRadius: "10px",
                                textTransform: "none",
                              }}
                            >
                              Apply IP5306 Configuration
                            </Button>
                          </Grid>
                        </Grid>
                      </VuiBox>
                    </Card>
                  </>
                )}

                {/* TAB 3: ACTUATOR CONTROL (3 CHANNELS) */}
                {activeTab === "actuator" && (
                  <>
                    <Card sx={{ ...panelSx, height: "auto" }}>
                      <VuiBox p={2.5}>
                        <VuiBox display="flex" justifyContent="space-between" alignItems="center" flexWrap="wrap" gap={1} mb={2.5}>
                          <VuiBox display="flex" alignItems="center">
                            <VuiBox width="36px" height="36px" borderRadius="10px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "rgba(138, 44, 255, 0.15)", border: "1px solid rgba(138, 44, 255, 0.35)" }}>
                              <IoToggle color="#8a2cff" size="20px" />
                            </VuiBox>
                            <VuiBox>
                              <VuiTypography variant="h6" color="white" fontWeight="bold">
                                Actuators (3 Hardware Channels)
                              </VuiTypography>
                              <VuiTypography variant="caption" color="text">
                                Direct control for Relays, Solenoid Valves, Fans & Alarms
                              </VuiTypography>
                            </VuiBox>
                          </VuiBox>

                          <Button
                            variant="outlined"
                            color="error"
                            size="small"
                            onClick={handleEmergencyAllOff}
                            sx={{
                              borderColor: "rgba(239, 68, 68, 0.5)",
                              color: "#ef4444",
                              textTransform: "none",
                              fontWeight: "bold",
                              borderRadius: "8px",
                              "&:hover": { background: "rgba(239, 68, 68, 0.15)", borderColor: "#ef4444" },
                            }}
                          >
                            🚨 Emergency All OFF
                          </Button>
                        </VuiBox>

                        <Grid container spacing={2.5}>
                          {actuators.map((ch) => (
                            <Grid item xs={12} key={ch.id}>
                              <VuiBox
                                p={2.5}
                                borderRadius="14px"
                                sx={{
                                  background: ch.state ? "linear-gradient(135deg, rgba(1, 247, 167, 0.10) 0%, rgba(1, 247, 167, 0.02) 100%)" : "rgba(255,255,255,0.02)",
                                  border: ch.state ? "1.5px solid rgba(1, 247, 167, 0.45)" : "1px solid rgba(255,255,255,0.08)",
                                  boxShadow: ch.state ? "0 0 20px rgba(1, 247, 167, 0.12)" : "none",
                                  transition: "all 0.25s ease",
                                }}
                              >
                                <Grid container spacing={2} alignItems="center">
                                  {/* Channel Header & Name */}
                                  <Grid item xs={12} sm={4}>
                                    <VuiBox display="flex" alignItems="center" gap={1.5} mb={1}>
                                      <Chip
                                        label={`CH ${ch.id}`}
                                        size="small"
                                        sx={{
                                          background: "rgba(0, 117, 255, 0.2)",
                                          border: "1px solid rgba(0, 117, 255, 0.5)",
                                          color: "#0075ff",
                                          fontWeight: "bold",
                                        }}
                                      />
                                      <VuiTypography variant="h6" color="white" fontWeight="bold">
                                        {ch.name}
                                      </VuiTypography>
                                    </VuiBox>
                                    <VuiTypography variant="caption" color="text">
                                      Status:{" "}
                                      <span style={{ color: ch.state ? "#01f7a7" : "#ef4444", fontWeight: "bold" }}>
                                        {ch.state ? "ACTIVE (ON)" : "INACTIVE (OFF)"}
                                      </span>
                                    </VuiTypography>
                                  </Grid>

                                  {/* Mode Selection */}
                                  <Grid item xs={12} sm={4}>
                                    <VuiTypography variant="caption" color="text" display="block" mb={0.5}>
                                      Operating Mode
                                    </VuiTypography>
                                    <TextField
                                      select
                                      fullWidth
                                      value={ch.mode}
                                      onChange={(e) =>
                                        setActuators((prev) =>
                                          prev.map((c) => (c.id === ch.id ? { ...c, mode: e.target.value } : c))
                                        )
                                      }
                                      sx={solidInputSx}
                                    >
                                      <MenuItem value="manual">Manual (Direct Toggle)</MenuItem>
                                      <MenuItem value="pulse">Pulse / Timer</MenuItem>
                                      <MenuItem value="auto">Auto Threshold (Sensor)</MenuItem>
                                    </TextField>
                                  </Grid>

                                  {/* Toggle Switch / Trigger Controls */}
                                  <Grid item xs={12} sm={4} display="flex" justifyContent="flex-end" alignItems="center" gap={1.5}>
                                    {ch.mode === "pulse" ? (
                                      <VuiBox display="flex" alignItems="center" gap={1}>
                                        <Button
                                          variant="contained"
                                          size="small"
                                          onClick={() => handlePulseActuator(ch.id, 5)}
                                          sx={{
                                            background: "rgba(0, 117, 255, 0.25)",
                                            border: "1px solid rgba(0, 117, 255, 0.5)",
                                            color: "#fff",
                                            textTransform: "none",
                                          }}
                                        >
                                          Pulse 5s
                                        </Button>
                                        <Button
                                          variant="contained"
                                          size="small"
                                          onClick={() => handlePulseActuator(ch.id, 10)}
                                          sx={{
                                            background: "rgba(0, 117, 255, 0.25)",
                                            border: "1px solid rgba(0, 117, 255, 0.5)",
                                            color: "#fff",
                                            textTransform: "none",
                                          }}
                                        >
                                          Pulse 10s
                                        </Button>
                                      </VuiBox>
                                    ) : (
                                      <VuiBox display="flex" alignItems="center" gap={1.5}>
                                        <VuiTypography variant="button" color={ch.state ? "success" : "text"} fontWeight="bold">
                                          {ch.state ? "ON" : "OFF"}
                                        </VuiTypography>
                                        <Switch
                                          checked={ch.state}
                                          onChange={(e) => handleToggleActuator(ch.id, e.target.checked)}
                                          color="success"
                                        />
                                      </VuiBox>
                                    )}
                                  </Grid>
                                </Grid>
                              </VuiBox>
                            </Grid>
                          ))}
                        </Grid>
                      </VuiBox>
                    </Card>

                    {/* Collapsible Packet Inspection Card */}
                    {lastActionStatus && (
                      <Card sx={{ ...panelSx, height: "auto", mt: 1 }}>
                        <VuiBox p={2}>
                          <VuiTypography variant="caption" color="text" display="block" mb={0.5} sx={{ fontFamily: "monospace" }}>
                            📡 Last Outgoing WebSocket Packet (Sent to Gateway):
                          </VuiTypography>
                          <VuiBox
                            p={1.5}
                            borderRadius="8px"
                            sx={{
                              background: "#080c20",
                              border: "1px solid rgba(255, 255, 255, 0.08)",
                              fontFamily: "monospace",
                              fontSize: "12px",
                              color: "#01f7a7",
                              overflowX: "auto",
                              whiteSpace: "pre-wrap",
                            }}
                          >
                            {JSON.stringify(lastActionStatus.payload, null, 2)}
                          </VuiBox>
                        </VuiBox>
                      </Card>
                    )}
                  </>
                )}
              </VuiBox>
            </Grid>
          </Grid>
        )}
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default NodeDetail;
