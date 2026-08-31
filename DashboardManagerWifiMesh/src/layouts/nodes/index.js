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

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";
import Table from "examples/Tables/Table";

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";

import { IoSearch, IoCube, IoShareSocial, IoCellular, IoTime, IoChevronForward, IoChevronDown } from "react-icons/io5";

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

  const { nodes } = useMeshNodesFromWebSocket();

  const sortedNodes = useMemo(() => {
    const merged = [...nodes];
    merged.sort((a, b) => {
      const la = a.meshLevel != null && Number.isFinite(a.meshLevel) ? a.meshLevel : 999;
      const lb = b.meshLevel != null && Number.isFinite(b.meshLevel) ? b.meshLevel : 999;
      if (la !== lb) return la - lb;
      return String(a.id).localeCompare(String(b.id));
    });
    return merged;
  }, [nodes]);

  const filtered = useMemo(() => {
    const byStatus =
      status === "all" ? sortedNodes : sortedNodes.filter((n) => (status === "online" ? n.online : !n.online));
    const query = q.trim().toLowerCase();
    if (!query) return byStatus;
    return byStatus.filter((n) => {
      const ip = (n.ip || "").toLowerCase();
      return (
        n.id.toLowerCase().includes(query) ||
        n.name.toLowerCase().includes(query) ||
        (n.location || "").toLowerCase().includes(query) ||
        (n.type || "").toLowerCase().includes(query) ||
        (n.mac || "").toLowerCase().includes(query) ||
        (ip && ip.includes(query))
      );
    });
  }, [status, q, sortedNodes]);

  const columns = useMemo(
    () => [
      { name: "name", align: "left" },
      { name: "mac", align: "left" },
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
          <VuiBox component={NavLink} to={`/nodes/${encodeURIComponent(n.id)}`} display="flex" justifyContent="center" alignItems="center" gap={1} sx={{ cursor: "pointer", textDecoration: "none" }}>
            <VuiTypography variant="button" color="info" fontWeight="bold">
              View
            </VuiTypography>
            <IoChevronForward color="#38bdf8" size="14px" />
          </VuiBox>
        ),
      })),
    [filtered]
  );

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
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
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb="24px" px="8px">
            <VuiBox display="flex" alignItems="center" gap={2}>
              <VuiBox display="flex" justifyContent="center" alignItems="center" width="40px" height="40px" borderRadius="12px" sx={{ background: "rgba(56, 189, 248, 0.15)", border: "1px solid rgba(56, 189, 248, 0.35)" }}>
                <IoCube color="#38bdf8" size="20px" />
              </VuiBox>
              <VuiTypography variant="h6" color="white" fontWeight="bold">
                Nodes (from DB IDs + live WS)
              </VuiTypography>
            </VuiBox>
            <VuiBox px={2} py={0.5} borderRadius="12px" sx={{ background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)" }}>
              <VuiTypography variant="caption" color="white" fontWeight="medium">
                {`Total: ${filtered.length}`}
              </VuiTypography>
            </VuiBox>
          </VuiBox>
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
              }
            }}
          >
            <Table columns={columns} rows={rows} />
          </VuiBox>
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Nodes;
