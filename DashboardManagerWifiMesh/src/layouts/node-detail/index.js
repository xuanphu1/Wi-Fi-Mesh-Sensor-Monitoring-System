import { useMemo, useState, useEffect } from "react";

// react-router-dom components
import { useParams } from "react-router-dom";

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

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";
import { resolveMrrErrorCode } from "utils/mrsErrorCodes";

import { IoCube, IoPerson, IoPulse, IoLayers, IoTime, IoCode, IoBarChart, IoHardwareChip, IoServer, IoWarning, IoSpeedometer, IoChevronForward, IoAnalytics } from "react-icons/io5";

const panelSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.08)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
  borderRadius: "16px",
  backdropFilter: "blur(42px)",
  height: "100%",
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
  "Name": <IoPerson size="14px" color="#38bdf8" />,
  "Node IP": <VuiTypography variant="caption" fontWeight="bold" color="success">IP</VuiTypography>,
  "Status": <IoPulse size="14px" color="#4ade80" />,
  "Mesh level": <IoLayers size="14px" color="#b8a9ff" />,
  "Last seen": <IoTime size="14px" color="rgba(255,255,255,0.7)" />,
  "Schema version": <IoCode size="14px" color="rgba(255,255,255,0.7)" />,
  "Packet loss": <IoBarChart size="14px" color="#38bdf8" />,
  "Firmware": <IoHardwareChip size="14px" color="rgba(255,255,255,0.7)" />,
  "Port count": <IoServer size="14px" color="rgba(255,255,255,0.7)" />,
  "Runtime errors": <IoWarning size="14px" color="#ef4444" />,
  "Node RTC": <IoTime size="14px" color="rgba(255,255,255,0.7)" />,
  "Latency": <IoSpeedometer size="14px" color="rgba(255,255,255,0.7)" />,
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

  const [sensorHistories, setSensorHistories] = useState({});

  const node = useMemo(
    () => nodes.find((n) => n.id === decodedNodeId || n.ip === decodedNodeId),
    [nodes, decodedNodeId]
  );
  
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

  // Record history (max 20 points) for real sparkline
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
        <VuiTypography variant="lg" color="white" fontWeight="bold" mb={3} display="flex" alignItems="center">
          Node detail: <span style={{ color: "#38bdf8", marginLeft: "8px" }}>{decodedNodeId}</span>
        </VuiTypography>

        {!node ? (
          <Card sx={panelSx}>
            <VuiBox p={3}>
              <VuiTypography color="white" variant="button" fontWeight="medium">
                Node not found.
              </VuiTypography>
            </VuiBox>
          </Card>
        ) : (
          <Grid container spacing={3}>
            {/* Left Column: Node info */}
            <Grid item xs={12} lg={4} xl={4}>
              <Card sx={panelSx}>
                <VuiBox p={2.5}>
                  <VuiBox display="flex" alignItems="center" mb={3}>
                    <VuiBox width="32px" height="32px" borderRadius="8px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "#111c44", border: "1px solid rgba(255, 255, 255, 0.05)" }}>
                      <IoCube color="#b8a9ff" size="18px" />
                    </VuiBox>
                    <VuiTypography variant="h6" color="white" fontWeight="bold">
                      Node info
                    </VuiTypography>
                  </VuiBox>

                  <VuiBox display="flex" flexDirection="column">
                    {infoRows.map((row, index) => {
                      let rightElement;
                      if (row.label === "Status") {
                        rightElement = (
                          <VuiBox display="flex" alignItems="center" gap={1}>
                            <VuiBox width="6px" height="6px" borderRadius="50%" bgColor={row.valueColor} sx={{ boxShadow: `0 0 8px ${row.valueColor === "success" ? "#4ade80" : "#ef4444"}` }} />
                            <VuiTypography color={row.valueColor} variant="button" fontWeight="bold">
                              {row.value}
                            </VuiTypography>
                          </VuiBox>
                        );
                      } else if (row.label === "Mesh level" && row.value !== "-") {
                        rightElement = (
                          <VuiBox px={1.5} py={0.2} borderRadius="12px" sx={{ background: "rgba(97, 66, 255, 0.5)" }}>
                            <VuiTypography color="white" variant="caption" fontWeight="bold">
                              {row.value}
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
                          py={1.5}
                          sx={{
                            borderBottom: index !== infoRows.length - 1 ? "1px solid rgba(255,255,255,0.05)" : "none",
                            display: "flex",
                            alignItems: "center",
                            justifyContent: "space-between"
                          }}
                        >
                          <VuiBox display="flex" alignItems="center">
                            <VuiBox width="28px" height="28px" borderRadius="8px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "#111c44", border: "1px solid rgba(255, 255, 255, 0.05)" }}>
                              {iconMap[row.label]}
                            </VuiBox>
                            <VuiTypography color="text" variant="button" fontWeight="medium">
                              {row.label}
                            </VuiTypography>
                          </VuiBox>
                          <VuiBox pl={2}>
                            {rightElement}
                          </VuiBox>
                        </VuiBox>
                      );
                    })}
                  </VuiBox>
                </VuiBox>
              </Card>
            </Grid>

            {/* Right Column */}
            <Grid item xs={12} lg={8} xl={8}>
              <VuiBox display="flex" flexDirection="column" gap={3}>
                
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
                              alignItems: "center"
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
                      <VuiBox width="32px" height="32px" borderRadius="8px" display="flex" justifyContent="center" alignItems="center" mr={2} sx={{ background: "#111c44", border: "1px solid rgba(255, 255, 255, 0.05)" }}>
                        <IoAnalytics color="#38bdf8" size="18px" />
                      </VuiBox>
                      <VuiTypography variant="h6" color="white" fontWeight="bold">
                        Realtime sensor data (from this IP)
                      </VuiTypography>
                    </VuiBox>

                    {sensorRows.length === 0 ? (
                      <VuiBox p={2} textAlign="center">
                        <VuiTypography color="text" variant="button">
                          No sensor data yet.
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
                                overflow: "hidden"
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
