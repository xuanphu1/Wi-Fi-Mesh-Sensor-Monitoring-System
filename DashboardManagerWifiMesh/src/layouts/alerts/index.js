import { useMemo, useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import Select from "@mui/material/Select";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";
import {
  IoList,
  IoShieldCheckmark,
  IoNotifications,
  IoTime,
  IoServer,
  IoChevronForward,
  IoChevronBack,
} from "react-icons/io5";

const alertsFilterWrapSx = {
  borderRadius: "16px",
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.08)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
};

const alertsFilterSelectSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    backgroundColor: "rgba(15, 18, 42, 0.95) !important",
    color: "#ffffff !important",
    minHeight: "44px",
    border: "1px solid rgba(255, 255, 255, 0.14) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#4318ff !important" },
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    padding: "10px 40px 10px 14px !important",
    minHeight: "24px !important",
    display: "flex !important",
    alignItems: "center !important",
  },
  "& .MuiSelect-icon": {
    display: "block !important",
    color: "rgba(255, 255, 255, 0.75) !important",
    right: 8,
  },
};

const alertsSelectMenuProps = {
  PaperProps: {
    sx: {
      mt: 1,
      borderRadius: "12px",
      background: "linear-gradient(127deg, rgba(20, 21, 55, 0.97) 0%, rgba(25, 26, 65, 0.98) 100%)",
      border: "1px solid rgba(255, 255, 255, 0.12)",
      "& .MuiMenuItem-root": { color: "#fff", fontSize: "0.875rem" },
      "& .MuiMenuItem-root:hover": { backgroundColor: "rgba(67, 24, 255, 0.22)" },
      "& .MuiMenuItem-root.Mui-selected": { backgroundColor: "rgba(255, 255, 255, 0.35)" },
    },
  },
};

function AlertRow({ item }) {
  const isError = item.level === "error";
  const levelBg = isError ? "rgba(255, 40, 92, 0.15)" : "rgba(255, 181, 71, 0.15)";
  const levelColor = isError ? "#ff285c" : "#ffb547";

  const statusBg = item.active ? "rgba(255, 181, 71, 0.15)" : "rgba(1, 247, 167, 0.15)";
  const statusColor = item.active ? "#ffb547" : "#01f7a7";

  return (
    <VuiBox
      sx={{
        display: "flex",
        alignItems: "center",
        mb: 1.5,
        p: 2,
        px: 3,
        borderRadius: "12px",
        background: "rgba(255, 255, 255, 0.025)",
        border: "1px solid rgba(255, 255, 255, 0.05)",
        transition: "all 0.2s ease",
        "&:hover": {
          background: "rgba(255, 255, 255, 0.045)",
          transform: "translateY(-1px)",
        },
      }}
    >
      <Grid container spacing={2} alignItems="center">
        {/* TIME */}
        <Grid item xs={2.5}>
          <VuiBox display="flex" alignItems="center" gap={1.5}>
            <VuiBox
              sx={{
                width: 28,
                height: 28,
                borderRadius: "8px",
                background: "rgba(67, 24, 255, 0.2)",
                color: "#8a2cff",
                display: "grid",
                placeItems: "center",
              }}
            >
              <IoTime size="16px" />
            </VuiBox>
            <VuiTypography variant="caption" color="white" fontWeight="medium">
              {new Date(item.tsIso).toLocaleString()}
            </VuiTypography>
          </VuiBox>
        </Grid>

        {/* LEVEL */}
        <Grid item xs={1}>
          <VuiBox
            px={1.25}
            py={0.25}
            sx={{
              display: "inline-block",
              background: levelBg,
              borderRadius: "4px",
              border: `1px solid ${levelColor}40`,
            }}
          >
            <VuiTypography variant="caption" style={{ color: levelColor }} fontWeight="bold">
              {item.level.toUpperCase()}
            </VuiTypography>
          </VuiBox>
        </Grid>

        {/* TYPE */}
        <Grid item xs={2}>
          <VuiTypography variant="caption" color="text">
            {item.type}
          </VuiTypography>
        </Grid>

        {/* NODE */}
        <Grid item xs={2}>
          <VuiBox display="flex" alignItems="center" gap={1}>
            <IoServer color="#a0aec0" size="14px" />
            <VuiTypography variant="caption" color="white" fontWeight="medium">
              {item.nodeId || "-"}
            </VuiTypography>
          </VuiBox>
        </Grid>

        {/* SENSOR */}
        <Grid item xs={1}>
          <VuiTypography variant="caption" color="text">
            {item.sensorId || "-"}
          </VuiTypography>
        </Grid>

        {/* VALUE */}
        <Grid item xs={1}>
          <VuiTypography variant="caption" color="white" fontWeight="medium">
            {item.value ?? "-"}
          </VuiTypography>
        </Grid>

        {/* THRESHOLD */}
        <Grid item xs={1}>
          <VuiTypography variant="caption" color="text">
            {item.threshold ?? "-"}
          </VuiTypography>
        </Grid>

        {/* STATUS */}
        <Grid item xs={1}>
          <VuiBox
            px={1.25}
            py={0.25}
            sx={{
              display: "inline-block",
              background: statusBg,
              borderRadius: "4px",
              border: `1px solid ${statusColor}40`,
            }}
          >
            <VuiTypography variant="caption" style={{ color: statusColor }} fontWeight="bold">
              {item.active ? "ACTIVE" : "RESOLVED"}
            </VuiTypography>
          </VuiBox>
        </Grid>

        {/* CHEVRON */}
        <Grid item xs={0.5} textAlign="right">
          <IoChevronForward color="#8fa7d6" />
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function Alerts() {
  const [active, setActive] = useState("all");
  const [level, setLevel] = useState("all");
  const [page, setPage] = useState(1);
  const rowsPerPage = 9;

  const { nodes } = useMeshNodesFromWebSocket();

  const filtered = useMemo(() => {
    const realtimeAlerts = [];
    nodes.forEach((node) => {
      const nodeId = node.ip || node.id || "-";
      const tsIso = node.lastSeenIso || new Date().toISOString();
      if (!node.online) {
        realtimeAlerts.push({
          id: `offline:${nodeId}`,
          tsIso,
          level: "error",
          type: "node_offline",
          nodeId,
          sensorId: null,
          value: null,
          threshold: null,
          active: true,
        });
      }
      const runtimeErrors = Array.isArray(node.runtimeErrors) ? node.runtimeErrors : [];
      runtimeErrors.forEach((code) => {
        realtimeAlerts.push({
          id: `runtime_error:${nodeId}:${code}`,
          tsIso,
          level: "error",
          type: "runtime_error",
          nodeId,
          sensorId: "err",
          value: code,
          threshold: null,
          active: true,
        });
      });
    });

    let list = realtimeAlerts;
    if (active !== "all") list = list.filter((a) => (active === "active" ? a.active : !a.active));
    if (level !== "all") list = list.filter((a) => a.level === level);
    return list.slice().sort((a, b) => new Date(b.tsIso).getTime() - new Date(a.tsIso).getTime());
  }, [active, level, nodes]);

  const totalPages = Math.ceil(filtered.length / rowsPerPage) || 1;
  const paginated = filtered.slice((page - 1) * rowsPerPage, page * rowsPerPage);

  const handleNextPage = () => {
    if (page < totalPages) setPage(page + 1);
  };
  const handlePrevPage = () => {
    if (page > 1) setPage(page - 1);
  };

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <VuiBox mb={3} p={3} sx={alertsFilterWrapSx}>
          <Grid container spacing={3}>
            <Grid item xs={12} md={4} xl={3}>
              <VuiBox display="flex" alignItems="center" gap={1.5} mb={1.5}>
                <VuiBox
                  sx={{
                    width: 28,
                    height: 28,
                    borderRadius: "8px",
                    background: "rgba(0,117,255,0.2)",
                    color: "#0075ff",
                    display: "grid",
                    placeItems: "center",
                  }}
                >
                  <IoList size="16px" />
                </VuiBox>
                <VuiTypography variant="button" color="white" fontWeight="bold">
                  Status
                </VuiTypography>
              </VuiBox>
              <FormControl fullWidth variant="outlined" sx={alertsFilterSelectSx}>
                <Select
                  value={active}
                  onChange={(e) => {
                    setActive(e.target.value);
                    setPage(1);
                  }}
                  MenuProps={alertsSelectMenuProps}
                >
                  <MenuItem value="all">All</MenuItem>
                  <MenuItem value="active">Active</MenuItem>
                  <MenuItem value="resolved">Resolved</MenuItem>
                </Select>
              </FormControl>
            </Grid>
            <Grid item xs={12} md={4} xl={3}>
              <VuiBox display="flex" alignItems="center" gap={1.5} mb={1.5}>
                <VuiBox
                  sx={{
                    width: 28,
                    height: 28,
                    borderRadius: "8px",
                    background: "rgba(138, 44, 255, 0.2)",
                    color: "#8a2cff",
                    display: "grid",
                    placeItems: "center",
                  }}
                >
                  <IoShieldCheckmark size="16px" />
                </VuiBox>
                <VuiTypography variant="button" color="white" fontWeight="bold">
                  Severity
                </VuiTypography>
              </VuiBox>
              <FormControl fullWidth variant="outlined" sx={alertsFilterSelectSx}>
                <Select
                  value={level}
                  onChange={(e) => {
                    setLevel(e.target.value);
                    setPage(1);
                  }}
                  MenuProps={alertsSelectMenuProps}
                >
                  <MenuItem value="all">All</MenuItem>
                  <MenuItem value="error">Error</MenuItem>
                  <MenuItem value="warning">Warning</MenuItem>
                  <MenuItem value="info">Info</MenuItem>
                </Select>
              </FormControl>
            </Grid>
          </Grid>
        </VuiBox>

        <Card
          sx={{
            padding: "24px 20px",
            background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
            border: "1px solid rgba(255, 255, 255, 0.08)",
            boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
            borderRadius: "16px",
            backdropFilter: "blur(42px)",
          }}
        >
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb="24px">
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              <VuiBox
                sx={{
                  width: 32,
                  height: 32,
                  borderRadius: "50%",
                  background: "#0075ff",
                  color: "#fff",
                  display: "grid",
                  placeItems: "center",
                  boxShadow: "0 0 10px rgba(0,117,255,0.5)",
                }}
              >
                <IoNotifications size="18px" />
              </VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                Alerts / Errors
              </VuiTypography>
            </VuiBox>
            <VuiBox
              px={2}
              py={0.5}
              sx={{
                background: "rgba(67, 24, 255, 0.15)",
                borderRadius: "16px",
              }}
            >
              <VuiTypography variant="caption" color="white" fontWeight="bold">
                Total: {filtered.length}
              </VuiTypography>
            </VuiBox>
          </VuiBox>

          <Grid container spacing={2} px={3} mb={1}>
            <Grid item xs={2.5}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                TIME <span style={{ opacity: 0.5, marginLeft: 4 }}>&#8693;</span>
              </VuiTypography>
            </Grid>
            <Grid item xs={1}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                LEVEL
              </VuiTypography>
            </Grid>
            <Grid item xs={2}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                TYPE
              </VuiTypography>
            </Grid>
            <Grid item xs={2}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                NODE
              </VuiTypography>
            </Grid>
            <Grid item xs={1}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                SENSOR
              </VuiTypography>
            </Grid>
            <Grid item xs={1}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                VALUE
              </VuiTypography>
            </Grid>
            <Grid item xs={1}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                THRESHOLD
              </VuiTypography>
            </Grid>
            <Grid item xs={1}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                STATUS
              </VuiTypography>
            </Grid>
          </Grid>

          <VuiBox display="flex" flexDirection="column" sx={{ minHeight: "300px" }}>
            {paginated.map((s, idx) => (
              <AlertRow key={s.id + idx} item={s} />
            ))}
            {paginated.length === 0 && (
              <VuiBox textAlign="center" py={4}>
                <VuiTypography color="text">No alerts right now.</VuiTypography>
              </VuiBox>
            )}
          </VuiBox>

          {filtered.length > 0 && (
            <VuiBox display="flex" justifyContent="space-between" alignItems="center" mt={3} px={1}>
              <VuiTypography variant="caption" color="text">
                Showing {(page - 1) * rowsPerPage + 1} to {Math.min(page * rowsPerPage, filtered.length)} of{" "}
                {filtered.length} alerts
              </VuiTypography>
              <VuiBox display="flex" gap={1}>
                <VuiBox
                  onClick={handlePrevPage}
                  sx={{
                    width: 32,
                    height: 32,
                    borderRadius: "8px",
                    background: page > 1 ? "rgba(255,255,255,0.05)" : "transparent",
                    color: page > 1 ? "#fff" : "rgba(255,255,255,0.2)",
                    display: "grid",
                    placeItems: "center",
                    cursor: page > 1 ? "pointer" : "default",
                    "&:hover": { background: page > 1 ? "rgba(255,255,255,0.1)" : "transparent" },
                  }}
                >
                  <IoChevronBack />
                </VuiBox>
                <VuiBox
                  sx={{
                    width: 32,
                    height: 32,
                    borderRadius: "8px",
                    background: "#0075ff",
                    color: "#fff",
                    display: "grid",
                    placeItems: "center",
                  }}
                >
                  <VuiTypography variant="caption" color="white" fontWeight="bold">
                    {page}
                  </VuiTypography>
                </VuiBox>
                <VuiBox
                  onClick={handleNextPage}
                  sx={{
                    width: 32,
                    height: 32,
                    borderRadius: "8px",
                    background: page < totalPages ? "rgba(255,255,255,0.05)" : "transparent",
                    color: page < totalPages ? "#fff" : "rgba(255,255,255,0.2)",
                    display: "grid",
                    placeItems: "center",
                    cursor: page < totalPages ? "pointer" : "default",
                    "&:hover": { background: page < totalPages ? "rgba(255,255,255,0.1)" : "transparent" },
                  }}
                >
                  <IoChevronForward />
                </VuiBox>
              </VuiBox>
            </VuiBox>
          )}
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Alerts;

