import { useMemo, useState } from "react";

// react-router-dom components
import { NavLink } from "react-router-dom";

// @mui material components
import Card from "@mui/material/Card";
import FormControl from "@mui/material/FormControl";
import Grid from "@mui/material/Grid";
import InputLabel from "@mui/material/InputLabel";
import MenuItem from "@mui/material/MenuItem";
import Select from "@mui/material/Select";
import TextField from "@mui/material/TextField";
import InputAdornment from "@mui/material/InputAdornment";
import Checkbox from "@mui/material/Checkbox";
import Button from "@mui/material/Button";
import Dialog from "@mui/material/Dialog";
import DialogTitle from "@mui/material/DialogTitle";
import DialogContent from "@mui/material/DialogContent";
import DialogActions from "@mui/material/DialogActions";
import IconButton from "@mui/material/IconButton";
import Chip from "@mui/material/Chip";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";
import Table from "examples/Tables/Table";

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";

import {
  IoSearch,
  IoCube,
  IoShareSocial,
  IoCellular,
  IoTime,
  IoChevronForward,
  IoChevronDown,
  IoServer,
  IoGitNetworkOutline,
  IoCheckmarkCircle,
  IoTrashOutline,
} from "react-icons/io5";

/**
 * Theme Vision UI set MuiInputBase: background white + color grey !important —
 * phải ghi đè !important để ô lọc nền tối, chữ sáng.
 */
const nodesFilterFieldSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "16px !important",
    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%) !important",
    backdropFilter: "blur(18px)",
    color: "#ffffff !important",
    height: "56px",
    border: "1px solid rgba(255, 255, 255, 0.10) !important",
    boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    "& fieldset": { 
      borderColor: "transparent !important", 
    },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": {
      borderColor: "#4318ff !important",
      border: "1px solid #4318ff !important",
    }
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    padding: "16px 14px !important",
    "&::placeholder": {
      color: "rgba(255, 255, 255, 0.55) !important",
      opacity: 1,
    },
  },
};

const nodesStatusFormSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "16px !important",
    background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%) !important",
    backdropFilter: "blur(18px)",
    color: "#ffffff !important",
    display: "flex !important",
    alignItems: "center !important",
    minHeight: "56px",
    padding: "0 !important",
    border: "1px solid rgba(255, 255, 255, 0.10) !important",
    boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    "& fieldset": { 
      borderColor: "transparent !important", 
    },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": {
      borderColor: "#4318ff !important",
      border: "1px solid #4318ff !important",
    }
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    display: "flex !important",
    alignItems: "center !important",
    padding: "14px 40px 14px 18px !important",
    minHeight: "24px !important",
    lineHeight: "1.5 !important",
    boxSizing: "border-box",
  },
};

const nodesSelectMenuProps = {
  PaperProps: {
    sx: {
      mt: 1,
      borderRadius: "12px",
      background: "linear-gradient(127deg, rgba(15, 21, 55, 0.88) 0%, rgba(20, 26, 65, 0.88) 100%)",
      backdropFilter: "blur(20px)",
      border: "1px solid rgba(255, 255, 255, 0.12)",
      boxShadow: "0 8px 32px rgba(0, 0, 0, 0.45)",
      "& .MuiMenuItem-root": { color: "#fff", fontSize: "0.875rem" },
      "& .MuiMenuItem-root:hover": { backgroundColor: "rgba(67, 24, 255, 0.22)" },
      "& .MuiMenuItem-root.Mui-selected": { backgroundColor: "rgba(255, 255, 255, 0.20)" },
    },
  },
};

function getMeshLevelBadgeStyle(type) {
  const t = String(type || "").toUpperCase();
  if (t.includes("ROOT") || t.includes("GATEWAY") || t.includes("L0")) {
    return {
      color: "#01f7a7",
      background: "rgba(1, 247, 167, 0.16)",
      border: "1px solid rgba(1, 247, 167, 0.45)",
      boxShadow: "0 0 8px rgba(1, 247, 167, 0.15)",
    };
  }
  if (t.includes("L1")) {
    return {
      color: "#00d4ff",
      background: "rgba(0, 212, 255, 0.16)",
      border: "1px solid rgba(0, 212, 255, 0.45)",
      boxShadow: "0 0 8px rgba(0, 212, 255, 0.15)",
    };
  }
  if (t.includes("L2")) {
    return {
      color: "#b8a9ff",
      background: "rgba(138, 44, 255, 0.18)",
      border: "1px solid rgba(138, 44, 255, 0.48)",
      boxShadow: "0 0 8px rgba(138, 44, 255, 0.15)",
    };
  }
  if (t.includes("L3")) {
    return {
      color: "#0075ff",
      background: "rgba(0, 117, 255, 0.18)",
      border: "1px solid rgba(0, 117, 255, 0.50)",
      boxShadow: "0 0 8px rgba(0, 117, 255, 0.15)",
    };
  }
  if (t.includes("L4")) {
    return {
      color: "#ffb547",
      background: "rgba(255, 181, 71, 0.16)",
      border: "1px solid rgba(255, 181, 71, 0.45)",
      boxShadow: "0 0 8px rgba(255, 181, 71, 0.15)",
    };
  }
  if (t.includes("L5")) {
    return {
      color: "#ff3b94",
      background: "rgba(255, 59, 148, 0.16)",
      border: "1px solid rgba(255, 59, 148, 0.45)",
      boxShadow: "0 0 8px rgba(255, 59, 148, 0.15)",
    };
  }
  if (t.includes("L6")) {
    return {
      color: "#ff6347",
      background: "rgba(255, 99, 71, 0.16)",
      border: "1px solid rgba(255, 99, 71, 0.45)",
      boxShadow: "0 0 8px rgba(255, 99, 71, 0.15)",
    };
  }
  if (t.includes("L7") || t.includes("L8") || t.includes("L9")) {
    return {
      color: "#a3e635",
      background: "rgba(163, 230, 53, 0.16)",
      border: "1px solid rgba(163, 230, 53, 0.45)",
      boxShadow: "0 0 8px rgba(163, 230, 53, 0.15)",
    };
  }
  return {
    color: "#e2e8f0",
    background: "rgba(255, 255, 255, 0.10)",
    border: "1px solid rgba(255, 255, 255, 0.25)",
    boxShadow: "none",
  };
}

function Nodes() {
  const [status, setStatus] = useState("all");
  const [q, setQ] = useState("");

  const {
    nodes = [],
    gateways = [],
    selectedGatewayId = "all",
    setSelectedGatewayId,
    deleteNodes,
    deleteGateway,
  } = useMeshNodesFromWebSocket();

  const [selectedMacs, setSelectedMacs] = useState([]);
  const [confirmDelete, setConfirmDelete] = useState(null); // { type: 'single' | 'batch', macs: [] }
  const [confirmDeleteGateway, setConfirmDeleteGateway] = useState(null); // gateway object to delete
  const [isDeleting, setIsDeleting] = useState(false);

  // Compute per-gateway node counts
  const gatewayStats = useMemo(() => {
    const stats = {
      all: { total: nodes.length, online: nodes.filter((n) => n.online).length },
    };
    gateways.forEach((gw) => {
      const gNodes = nodes.filter((n) => (n.gw_id || "gw-default") === gw.gw_id);
      stats[gw.gw_id] = {
        total: gNodes.length,
        online: gNodes.filter((n) => n.online).length,
      };
    });
    return stats;
  }, [nodes, gateways]);

  const sortedNodes = useMemo(() => {
    let sourceNodes = [...(nodes || [])];
    if (selectedGatewayId && selectedGatewayId !== "all") {
      sourceNodes = sourceNodes.filter((n) => (n.gw_id || "gw-default") === selectedGatewayId);
    }
    sourceNodes.sort((a, b) => {
      const la = a.meshLevel != null && Number.isFinite(a.meshLevel) ? a.meshLevel : 999;
      const lb = b.meshLevel != null && Number.isFinite(b.meshLevel) ? b.meshLevel : 999;
      if (la !== lb) return la - lb;
      return String(a.id).localeCompare(String(b.id));
    });
    return sourceNodes;
  }, [nodes, selectedGatewayId]);

  const filtered = useMemo(() => {
    const byStatus =
      status === "all" ? sortedNodes : sortedNodes.filter((n) => (status === "online" ? n.online : !n.online));
    const query = q.trim().toLowerCase();
    if (!query) return byStatus;
    return byStatus.filter((n) => {
      const ip = (n.ip || "").toLowerCase();
      const gw = (n.gw_id || "").toLowerCase();
      return (
        (n.id || "").toLowerCase().includes(query) ||
        (n.name || "").toLowerCase().includes(query) ||
        (n.location || "").toLowerCase().includes(query) ||
        (n.type || "").toLowerCase().includes(query) ||
        (n.mac || "").toLowerCase().includes(query) ||
        gw.includes(query) ||
        (ip && ip.includes(query))
      );
    });
  }, [status, q, sortedNodes]);

  const selectedGatewayObj = useMemo(() => {
    if (!selectedGatewayId || selectedGatewayId === "all") return null;
    return gateways.find((g) => g.gw_id === selectedGatewayId) || null;
  }, [gateways, selectedGatewayId]);

  // Checkbox select all visible nodes
  const allFilteredSelected = filtered.length > 0 && filtered.every((n) => selectedMacs.includes(n.id));
  const someFilteredSelected = filtered.some((n) => selectedMacs.includes(n.id)) && !allFilteredSelected;

  const handleToggleSelectAll = (e) => {
    if (e.target.checked) {
      const next = Array.from(new Set([...selectedMacs, ...filtered.map((n) => n.id)]));
      setSelectedMacs(next);
    } else {
      const filteredIds = new Set(filtered.map((n) => n.id));
      setSelectedMacs((prev) => prev.filter((id) => !filteredIds.has(id)));
    }
  };

  const handleToggleSelectOne = (id) => {
    setSelectedMacs((prev) =>
      prev.includes(id) ? prev.filter((m) => m !== id) : [...prev, id]
    );
  };

  const handleDeleteConfirmed = async () => {
    if (!confirmDelete || !deleteNodes) return;
    setIsDeleting(true);
    try {
      await deleteNodes(confirmDelete.macs);
      setSelectedMacs((prev) => prev.filter((m) => !confirmDelete.macs.includes(m)));
    } finally {
      setIsDeleting(false);
      setConfirmDelete(null);
    }
  };

  const handleDeleteGatewayConfirmed = async () => {
    if (!confirmDeleteGateway || !deleteGateway) return;
    setIsDeleting(true);
    try {
      await deleteGateway(confirmDeleteGateway.gw_id);
      if (selectedGatewayId === confirmDeleteGateway.gw_id) {
        if (setSelectedGatewayId) setSelectedGatewayId("all");
      }
    } finally {
      setIsDeleting(false);
      setConfirmDeleteGateway(null);
    }
  };

  const columns = useMemo(
    () => [
      {
        name: "select",
        align: "center",
        width: "50px",
      },
      { name: "name", align: "left" },
      { name: "mac", align: "left" },
      { name: "gateway", align: "left" },
      { name: "location", align: "left" },
      { name: "status", align: "left" },
      { name: "rssi", align: "left" },
      { name: "lastSeen", align: "left" },
      { name: "firmware", align: "center" },
      { name: "type", align: "center" },
      { name: "actions", align: "center" },
    ],
    []
  );

  const rows = useMemo(
    () =>
      filtered.map((n) => ({
        select: (
          <Checkbox
            checked={selectedMacs.includes(n.id)}
            onChange={() => handleToggleSelectOne(n.id)}
            sx={{
              color: "rgba(255,255,255,0.4) !important",
              "&.Mui-checked": { color: "#00d4ff !important" },
              padding: 0,
            }}
          />
        ),
        name: (
          <VuiBox display="flex" alignItems="center">
            <VuiBox mr={2} display="flex" justifyContent="center" alignItems="center" width="32px" height="32px" borderRadius="8px" sx={{ background: "rgba(97, 66, 255, 0.18)", border: "1px solid rgba(97, 66, 255, 0.40)", color: "#b8a9ff" }}>
              <IoShareSocial color="#b8a9ff" size="16px" />
            </VuiBox>
            <VuiTypography variant="button" color="white" fontWeight="bold">
              {n.name}
            </VuiTypography>
          </VuiBox>
        ),
        mac: (
          <VuiTypography variant="button" color="text" sx={{ fontFamily: "monospace" }}>
            {n.id}
          </VuiTypography>
        ),
        gateway: (
          <VuiBox
            onClick={() => setSelectedGatewayId && setSelectedGatewayId(n.gw_id || "gw-default")}
            sx={{
              px: 1.2,
              py: 0.4,
              borderRadius: "8px",
              background: "rgba(0, 212, 255, 0.12)",
              border: "1px solid rgba(0, 212, 255, 0.35)",
              display: "inline-block",
              cursor: "pointer",
              transition: "all 0.2s ease",
              "&:hover": {
                background: "rgba(0, 212, 255, 0.25)",
                borderColor: "#00d4ff",
              },
            }}
          >
            <VuiTypography variant="caption" sx={{ color: "#00d4ff", fontWeight: "bold" }}>
              {n.gw_id || "gw-default"}
            </VuiTypography>
          </VuiBox>
        ),
        location: (
          <VuiTypography variant="button" color="text">
            {n.location || "-"}
          </VuiTypography>
        ),
        status: (
          <VuiBox display="flex" alignItems="center" gap={1}>
            <VuiBox width="8px" height="8px" borderRadius="50%" bgColor={n.online ? "success" : "error"} sx={{ boxShadow: `0 0 8px ${n.online ? "#4ade80" : "#ef4444"}` }} />
            <VuiTypography variant="button" color={n.online ? "success" : "error"} fontWeight="bold">
              {n.online ? "ONLINE" : "OFFLINE"}
            </VuiTypography>
          </VuiBox>
        ),
        rssi: (
          <VuiBox display="flex" alignItems="center" gap={1}>
            {typeof n.rssi === "number" ? <IoCellular color="#38bdf8" size="14px" /> : null}
            <VuiTypography variant="button" color="white" fontWeight="medium">
              {typeof n.rssi === "number" ? `${n.rssi} dBm` : "—"}
            </VuiTypography>
          </VuiBox>
        ),
        lastSeen: (
          <VuiBox display="flex" alignItems="center" gap={1}>
            {n.lastSeenIso ? <IoTime color="rgba(255,255,255,0.7)" size="16px" /> : null}
            <VuiBox display="flex" flexDirection="column">
              <VuiTypography variant="caption" color="text">
                {n.lastSeenIso ? new Date(n.lastSeenIso).toLocaleDateString() : "—"}
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                {n.lastSeenIso ? new Date(n.lastSeenIso).toLocaleTimeString() : ""}
              </VuiTypography>
            </VuiBox>
          </VuiBox>
        ),
        firmware: (
          <VuiTypography variant="button" color="white" fontWeight="medium">
            {n.firmwareVersion || "—"}
          </VuiTypography>
        ),
        type: (
          n.type ? (
            (() => {
              const badgeStyle = getMeshLevelBadgeStyle(n.type);
              return (
                <VuiBox 
                  px={1.6} py={0.5} 
                  borderRadius="8px" 
                  display="inline-block"
                  sx={{ 
                    background: badgeStyle.background,
                    border: badgeStyle.border,
                    boxShadow: badgeStyle.boxShadow,
                  }}
                >
                  <VuiTypography variant="caption" fontWeight="bold" sx={{ color: badgeStyle.color, letterSpacing: "0.5px" }}>
                    {n.type}
                  </VuiTypography>
                </VuiBox>
              );
            })()
          ) : (
            <VuiTypography variant="button" color="text">—</VuiTypography>
          )
        ),
        actions: (
          <VuiBox display="flex" justifyContent="center" alignItems="center" gap={1.2}>
            <VuiBox component={NavLink} to={`/nodes/${encodeURIComponent(n.id)}`} display="flex" alignItems="center" gap={0.5} sx={{ cursor: "pointer", textDecoration: "none" }}>
              <VuiTypography variant="caption" color="info" fontWeight="bold">
                View
              </VuiTypography>
              <IoChevronForward color="#38bdf8" size="14px" />
            </VuiBox>
            <IconButton
              size="small"
              onClick={() => setConfirmDelete({ type: "single", macs: [n.id], name: n.name || n.id })}
              sx={{
                color: "#ff5252",
                p: "4px",
                borderRadius: "8px",
                border: "1px solid rgba(255, 82, 82, 0.25)",
                "&:hover": { background: "rgba(255, 82, 82, 0.15)" },
              }}
            >
              <IoTrashOutline size="15px" />
            </IconButton>
          </VuiBox>
        ),
      })),
    [filtered, selectedMacs, setSelectedGatewayId]
  );

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        {/* Gateway Selection Section */}
        <VuiBox mb={3}>
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb={1.5} flexWrap="wrap" gap={1}>
            <VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                Phân Loại Node Theo Trạm Gateway
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                Nhấp chọn một Gateway bên dưới để lọc danh sách các Node cảm biến trực thuộc trạm đó
              </VuiTypography>
            </VuiBox>
            {selectedGatewayId !== "all" && (
              <Chip
                label="Hiển thị tất cả Gateway"
                onClick={() => setSelectedGatewayId && setSelectedGatewayId("all")}
                onDelete={() => setSelectedGatewayId && setSelectedGatewayId("all")}
                sx={{
                  background: "rgba(0, 212, 255, 0.2)",
                  color: "#00d4ff",
                  border: "1px solid #00d4ff",
                  fontWeight: "bold",
                  cursor: "pointer",
                }}
              />
            )}
          </VuiBox>

          <Grid container spacing={2}>
            {/* All Gateways Card */}
            <Grid item xs={12} sm={6} md={3}>
              <Card
                onClick={() => setSelectedGatewayId && setSelectedGatewayId("all")}
                sx={{
                  background: selectedGatewayId === "all"
                    ? "linear-gradient(135deg, rgba(0, 117, 255, 0.35) 0%, rgba(0, 212, 255, 0.20) 100%)"
                    : "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
                  border: selectedGatewayId === "all" ? "2px solid #00d4ff" : "1px solid rgba(255, 255, 255, 0.10)",
                  boxShadow: selectedGatewayId === "all" ? "0 0 20px rgba(0, 212, 255, 0.3)" : "0 8px 32px rgba(0, 0, 0, 0.35)",
                  borderRadius: "16px",
                  backdropFilter: "blur(18px)",
                  cursor: "pointer",
                  p: 2,
                  transition: "all 0.25s ease",
                  "&:hover": {
                    borderColor: "#00d4ff",
                    transform: "translateY(-2px)",
                  },
                }}
              >
                <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                  <VuiBox display="flex" alignItems="center" gap={1.5}>
                    <VuiBox
                      width="38px"
                      height="38px"
                      borderRadius="10px"
                      display="flex"
                      alignItems="center"
                      justifyContent="center"
                      sx={{ background: "rgba(0, 212, 255, 0.15)", border: "1px solid rgba(0, 212, 255, 0.35)", color: "#00d4ff" }}
                    >
                      <IoServer size="20px" />
                    </VuiBox>
                    <VuiBox>
                      <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                        Tất cả Gateway
                      </VuiTypography>
                      <VuiTypography variant="caption" color="text">
                        Toàn hệ thống
                      </VuiTypography>
                    </VuiBox>
                  </VuiBox>
                  <VuiBox textAlign="right">
                    <VuiTypography variant="h5" color="#00d4ff" fontWeight="bold">
                      {gatewayStats.all?.total || 0}
                    </VuiTypography>
                    <VuiTypography variant="caption" color="text">
                      nodes
                    </VuiTypography>
                  </VuiBox>
                </VuiBox>
              </Card>
            </Grid>

            {/* Individual Gateway Cards */}
            {gateways.map((gw) => {
              const isSelected = selectedGatewayId === gw.gw_id;
              const gStat = gatewayStats[gw.gw_id] || { total: 0, online: 0 };
              const isOnline = gw.status === "online";

              return (
                <Grid item xs={12} sm={6} md={3} key={gw.gw_id}>
                  <Card
                    onClick={() => setSelectedGatewayId && setSelectedGatewayId(isSelected ? "all" : gw.gw_id)}
                    sx={{
                      background: isSelected
                        ? "linear-gradient(135deg, rgba(0, 212, 255, 0.30) 0%, rgba(1, 247, 167, 0.15) 100%)"
                        : "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
                      border: isSelected ? "2px solid #01f7a7" : "1px solid rgba(255, 255, 255, 0.10)",
                      boxShadow: isSelected ? "0 0 20px rgba(1, 247, 167, 0.3)" : "0 8px 32px rgba(0, 0, 0, 0.35)",
                      borderRadius: "16px",
                      backdropFilter: "blur(18px)",
                      cursor: "pointer",
                      p: 2,
                      transition: "all 0.25s ease",
                      "&:hover": {
                        borderColor: "#01f7a7",
                        transform: "translateY(-2px)",
                      },
                    }}
                  >
                    <VuiBox display="flex" justifyContent="space-between" alignItems="center">
                      <VuiBox display="flex" alignItems="center" gap={1.5} maxWidth="60%">
                        <VuiBox
                          width="38px"
                          height="38px"
                          borderRadius="10px"
                          display="flex"
                          alignItems="center"
                          justifyContent="center"
                          sx={{
                            background: isOnline ? "rgba(1, 247, 167, 0.15)" : "rgba(255, 82, 82, 0.15)",
                            border: `1px solid ${isOnline ? "rgba(1, 247, 167, 0.35)" : "rgba(255, 82, 82, 0.35)"}`,
                            color: isOnline ? "#01f7a7" : "#ff5252",
                            flexShrink: 0,
                          }}
                        >
                          <IoServer size="20px" />
                        </VuiBox>
                        <VuiBox overflow="hidden">
                          <VuiTypography variant="button" color="white" fontWeight="bold" display="block" noWrap>
                            {gw.name || gw.gw_id}
                          </VuiTypography>
                          <VuiTypography variant="caption" color="text" display="block" noWrap sx={{ fontSize: "11px" }}>
                            {gw.mac || gw.root_mac ? `MAC: ${gw.mac || gw.root_mac}` : (gw.location || gw.gw_id)}
                          </VuiTypography>
                        </VuiBox>
                      </VuiBox>
                      <VuiBox display="flex" alignItems="center" gap={1}>
                        <VuiBox textAlign="right">
                          <VuiTypography variant="h5" color={isSelected ? "#01f7a7" : "white"} fontWeight="bold">
                            {gStat.total}
                          </VuiTypography>
                          <VuiTypography variant="caption" color="text">
                            nodes ({gStat.online} ON)
                          </VuiTypography>
                        </VuiBox>
                        <IconButton
                          size="small"
                          onClick={(e) => {
                            e.stopPropagation();
                            setConfirmDeleteGateway(gw);
                          }}
                          sx={{
                            color: "rgba(255, 82, 82, 0.6)",
                            p: "4px",
                            borderRadius: "8px",
                            "&:hover": { color: "#ff5252", background: "rgba(255, 82, 82, 0.15)" },
                          }}
                          title="Xóa Gateway này"
                        >
                          <IoTrashOutline size="16px" />
                        </IconButton>
                      </VuiBox>
                    </VuiBox>
                  </Card>
                </Grid>
              );
            })}
          </Grid>
        </VuiBox>

        <Grid container spacing={2} mb={3} alignItems="center">
          <Grid item xs={12} md={7}>
            <TextField
              fullWidth
              variant="outlined"
              value={q}
              onChange={(e) => setQ(e.target.value)}
              placeholder="Tìm kiếm (tên / ID / IP / MAC / loại)..."
              sx={nodesFilterFieldSx}
              InputProps={{
                startAdornment: (
                  <InputAdornment position="start">
                    <IoSearch color="rgba(255,255,255,0.5)" size="18px" style={{ marginLeft: "4px" }} />
                  </InputAdornment>
                ),
              }}
            />
          </Grid>
          <Grid item xs={12} md={5}>
            <FormControl fullWidth variant="outlined" sx={nodesStatusFormSx}>
              <Select
                id="nodes-filter-status"
                value={status}
                onChange={(e) => setStatus(e.target.value)}
                MenuProps={nodesSelectMenuProps}
                IconComponent={(props) => <IoChevronDown {...props} color="rgba(255,255,255,0.6)" size="16px" style={{ marginRight: "12px", cursor: "pointer", position: "absolute", right: 0 }} />}
              >
                <MenuItem value="all">Tất cả trạng thái</MenuItem>
                <MenuItem value="online">Online</MenuItem>
                <MenuItem value="offline">Offline</MenuItem>
              </Select>
            </FormControl>
          </Grid>
        </Grid>

        <Card
          sx={{
            background: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
            border: "1px solid rgba(255, 255, 255, 0.10)",
            boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
            borderRadius: "16px",
            backdropFilter: "blur(18px)",
          }}
        >
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb="20px" px="8px" flexWrap="wrap" gap={1}>
            <VuiBox display="flex" alignItems="center" gap={2}>
              <VuiBox display="flex" justifyContent="center" alignItems="center" width="40px" height="40px" borderRadius="12px" sx={{ background: "rgba(56, 189, 248, 0.15)", border: "1px solid rgba(56, 189, 248, 0.35)" }}>
                <IoCube color="#38bdf8" size="20px" />
              </VuiBox>
              <VuiBox>
                <VuiTypography variant="h6" color="white" fontWeight="bold">
                  Danh sách Nodes {selectedGatewayObj ? `trực thuộc: ${selectedGatewayObj.name || selectedGatewayObj.gw_id}` : "(Toàn mạng Mesh)"}
                </VuiTypography>
                {selectedGatewayObj && (
                  <VuiTypography variant="caption" color="text">
                    ID: {selectedGatewayObj.mac || selectedGatewayObj.root_mac || selectedGatewayObj.gw_id} | Vị trí: {selectedGatewayObj.location || "N/A"} | Mạng: {selectedGatewayObj.mesh_id || "mesh-default"}
                  </VuiTypography>
                )}
              </VuiBox>
            </VuiBox>
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              {selectedMacs.length > 0 && (
                <Button
                  variant="contained"
                  size="small"
                  onClick={() => setConfirmDelete({ type: "batch", macs: selectedMacs })}
                  sx={{
                    background: "linear-gradient(135deg, #ff5252 0%, #ff1744 100%) !important",
                    color: "#fff !important",
                    fontWeight: "bold",
                    borderRadius: "10px",
                    px: 2,
                    textTransform: "none",
                    boxShadow: "0 0 12px rgba(255, 82, 82, 0.4)",
                  }}
                >
                  <IoTrashOutline size="16px" style={{ marginRight: 6 }} /> Xóa ({selectedMacs.length}) Node đã chọn
                </Button>
              )}
              <VuiBox px={2} py={0.5} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)" }}>
                <VuiTypography variant="caption" color="white" fontWeight="medium">
                  {`Total: ${filtered.length} nodes`}
                </VuiTypography>
              </VuiBox>
            </VuiBox>
          </VuiBox>

          {/* Quick Select All Toolbar if there are items */}
          {filtered.length > 0 && (
            <VuiBox display="flex" alignItems="center" px="16px" py="8px" mb="8px" borderRadius="10px" sx={{ background: "rgba(255,255,255,0.02)", border: "1px solid rgba(255,255,255,0.05)" }}>
              <Checkbox
                checked={allFilteredSelected}
                indeterminate={someFilteredSelected}
                onChange={handleToggleSelectAll}
                sx={{
                  color: "rgba(255,255,255,0.4) !important",
                  "&.Mui-checked, &.MuiCheckbox-indeterminate": { color: "#00d4ff !important" },
                  padding: "4px 8px 4px 0",
                }}
              />
              <VuiTypography variant="caption" color="text" sx={{ cursor: "pointer", userSelect: "none" }} onClick={() => handleToggleSelectAll({ target: { checked: !allFilteredSelected } })}>
                {allFilteredSelected ? "Bỏ chọn tất cả" : `Chọn tất cả (${filtered.length} node hiển thị)`}
              </VuiTypography>
              {selectedMacs.length > 0 && (
                <VuiTypography variant="caption" sx={{ color: "#00d4ff", ml: "auto", fontWeight: "bold" }}>
                  Đã chọn {selectedMacs.length} node
                </VuiTypography>
              )}
            </VuiBox>
          )}

          {filtered.length === 0 ? (
            <VuiBox py={6} px={3} textAlign="center">
              <VuiBox
                display="inline-flex"
                alignItems="center"
                justifyContent="center"
                p={2}
                borderRadius="50%"
                sx={{ background: "rgba(255, 255, 255, 0.05)", mb: 2 }}
              >
                <IoCube color="rgba(255, 255, 255, 0.3)" size="36px" />
              </VuiBox>
              <VuiTypography variant="h6" color="white" fontWeight="medium">
                Không có Node cảm biến nào
              </VuiTypography>
              <VuiTypography variant="caption" color="text" display="block" mt={0.5}>
                {selectedGatewayObj
                  ? `Trạm Gateway "${selectedGatewayObj.name || selectedGatewayObj.gw_id}" hiện chưa có Node nào kết nối hoặc gateway đang ngoại tuyến.`
                  : "Không tìm thấy Node nào phù hợp với bộ lọc hiện tại."}
              </VuiTypography>
              {selectedGatewayId !== "all" && (
                <VuiBox mt={2.5}>
                  <Button
                    variant="outlined"
                    size="small"
                    onClick={() => setSelectedGatewayId && setSelectedGatewayId("all")}
                    sx={{
                      color: "#00d4ff !important",
                      borderColor: "rgba(0, 212, 255, 0.4) !important",
                      borderRadius: "10px",
                      textTransform: "none",
                      "&:hover": { borderColor: "#00d4ff !important" },
                    }}
                  >
                    Hiển thị tất cả Gateway
                  </Button>
                </VuiBox>
              )}
            </VuiBox>
          ) : (
            <VuiBox
              sx={{
                "& th": {
                  borderBottom: "1px solid rgba(255, 255, 255, 0.05)",
                  color: "rgba(255, 255, 255, 0.5)",
                  fontSize: "0.65rem",
                  textTransform: "uppercase",
                },
                "& .MuiTableRow-root:not(:last-child)": {
                  "& td": {
                    borderBottom: "1px solid rgba(255, 255, 255, 0.05)",
                  },
                },
                "& .MuiTableRow-root:hover": {
                  backgroundColor: "rgba(255, 255, 255, 0.02)",
                },
                "& td": {
                  padding: "12px 16px",
                },
              }}
            >
              <Table columns={columns} rows={rows} />
            </VuiBox>
          )}
        </Card>

        {/* Delete Nodes Confirmation Dialog */}
        <Dialog
          open={Boolean(confirmDelete)}
          onClose={() => setConfirmDelete(null)}
          PaperProps={{
            sx: {
              background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
              backdropFilter: "blur(20px)",
              border: "1px solid rgba(255, 82, 82, 0.35)",
              borderRadius: "18px",
              color: "#fff",
              minWidth: { xs: "90%", sm: "400px" },
              p: 2,
            },
          }}
        >
          <DialogTitle sx={{ color: "#ff5252", fontWeight: "bold", display: "flex", alignItems: "center", gap: 1 }}>
            <IoTrashOutline size="22px" /> Xác nhận xóa Node?
          </DialogTitle>
          <DialogContent>
            <VuiTypography variant="button" color="text" display="block">
              {confirmDelete?.type === "batch"
                ? `Bạn có chắc chắn muốn xóa ${confirmDelete?.macs?.length} Node đã chọn khỏi hệ thống?`
                : `Bạn có chắc chắn muốn xóa Node: ${confirmDelete?.name || confirmDelete?.macs?.[0]} (${confirmDelete?.macs?.[0]})?`}
            </VuiTypography>
            <VuiTypography variant="caption" color="text" display="block" mt={1} sx={{ opacity: 0.8 }}>
              Các bản ghi dữ liệu và snapshot lịch sử của Node này trong cơ sở dữ liệu sẽ bị xóa hoàn toàn.
            </VuiTypography>
          </DialogContent>
          <DialogActions sx={{ px: 3, pb: 2 }}>
            <Button onClick={() => setConfirmDelete(null)} sx={{ color: "rgba(255,255,255,0.6)" }}>
              Hủy
            </Button>
            <Button
              variant="contained"
              disabled={isDeleting}
              onClick={handleDeleteConfirmed}
              sx={{
                background: "linear-gradient(135deg, #ff5252 0%, #ff1744 100%) !important",
                color: "#fff",
                fontWeight: "bold",
                borderRadius: "10px",
                px: 3,
              }}
            >
              {isDeleting ? "Đang xóa..." : "Xác nhận xóa"}
            </Button>
          </DialogActions>
        </Dialog>

        {/* Delete Gateway Confirmation Dialog */}
        <Dialog
          open={Boolean(confirmDeleteGateway)}
          onClose={() => setConfirmDeleteGateway(null)}
          PaperProps={{
            sx: {
              background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
              backdropFilter: "blur(20px)",
              border: "1px solid rgba(255, 82, 82, 0.35)",
              borderRadius: "18px",
              color: "#fff",
              minWidth: { xs: "90%", sm: "400px" },
              p: 2,
            },
          }}
        >
          <DialogTitle sx={{ color: "#ff5252", fontWeight: "bold", display: "flex", alignItems: "center", gap: 1 }}>
            <IoTrashOutline size="22px" /> Xác nhận xóa Gateway?
          </DialogTitle>
          <DialogContent>
            <VuiTypography variant="button" color="text" display="block">
              Bạn có chắc chắn muốn xóa Gateway: <strong>{confirmDeleteGateway?.name || confirmDeleteGateway?.gw_id}</strong> ({confirmDeleteGateway?.gw_id})?
            </VuiTypography>
            <VuiTypography variant="caption" color="text" display="block" mt={1} sx={{ opacity: 0.8 }}>
              Hành động này sẽ xóa Gateway khỏi danh mục theo dõi và cơ sở dữ liệu.
            </VuiTypography>
          </DialogContent>
          <DialogActions sx={{ px: 3, pb: 2 }}>
            <Button onClick={() => setConfirmDeleteGateway(null)} sx={{ color: "rgba(255,255,255,0.6)" }}>
              Hủy
            </Button>
            <Button
              variant="contained"
              disabled={isDeleting}
              onClick={handleDeleteGatewayConfirmed}
              sx={{
                background: "linear-gradient(135deg, #ff5252 0%, #ff1744 100%) !important",
                color: "#fff",
                fontWeight: "bold",
                borderRadius: "10px",
                px: 3,
              }}
            >
              {isDeleting ? "Đang xóa..." : "Xác nhận xóa"}
            </Button>
          </DialogActions>
        </Dialog>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Nodes;
