import { useMemo, useState } from "react";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import FormControl from "@mui/material/FormControl";
import InputLabel from "@mui/material/InputLabel";
import Select from "@mui/material/Select";
import InputAdornment from "@mui/material/InputAdornment";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useDashboardRealtime } from "hooks/useDashboardRealtime";
import { IoPulse, IoSearch, IoThermometer, IoServer } from "react-icons/io5";

const sensorFilterFieldSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    backgroundColor: "rgba(15, 18, 42, 0.95) !important",
    color: "#ffffff !important",
    height: "56px",
    border: "1px solid rgba(255, 255, 255, 0.14) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": {
      borderColor: "#4318ff !important",
    },
  },
  "& .MuiInputLabel-root": {
    color: "rgba(255, 255, 255, 0.45) !important",
    "&.Mui-focused, &.MuiInputLabel-shrink": {
      color: "#ffffff !important",
      backgroundColor: "#0f122a",
      padding: "0 8px !important",
      transform: "translate(14px, -11px) scale(0.75) !important",
    },
  },
  "& .MuiInputBase-input": {
    color: "#ffffff !important",
    padding: "16px 14px !important",
    "&::placeholder": {
      color: "rgba(255, 255, 255, 0.45) !important",
      opacity: 1,
    },
  },
};

const sensorFilterSelectSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px !important",
    backgroundColor: "rgba(15, 18, 42, 0.95) !important",
    color: "#ffffff !important",
    minHeight: "56px",
    border: "1px solid rgba(255, 255, 255, 0.14) !important",
    "& fieldset": { borderColor: "transparent !important" },
    "&:hover fieldset": { borderColor: "transparent !important" },
    "&.Mui-focused": { borderColor: "#4318ff !important" },
  },
  "& .MuiInputLabel-root": {
    color: "rgba(255, 255, 255, 0.72) !important",
    "&.Mui-focused, &.MuiInputLabel-shrink": {
      color: "#ffffff !important",
      backgroundColor: "#0f122a",
      padding: "0 8px !important",
      transform: "translate(14px, -11px) scale(0.75) !important",
    },
  },
  "& .MuiSelect-select": {
    color: "#ffffff !important",
    padding: "14px 40px 14px 14px !important",
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

const sensorSelectMenuProps = {
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

function SensorRow({ item }) {
  const isAHT = item.name.toLowerCase().includes("aht");
  const iconBg = isAHT ? "rgba(0, 209, 167, 0.15)" : "rgba(67, 24, 255, 0.2)";
  const iconColor = isAHT ? "#00d1a7" : "#0075ff";

  let valueColor = "white";
  if (item.unit === "%") valueColor = "#00d1a7";
  else if (item.unit === "hPa") valueColor = "#8a2cff";
  else if (item.sensorId.toLowerCase().includes("temp") && isAHT) valueColor = "#00d1a7";

  const valFormatted = typeof item.value === "number" ? item.value.toFixed(3) : item.value;

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
        }
      }}
    >
      <Grid container spacing={2} alignItems="center">
        <Grid item xs={3}>
          <VuiBox display="flex" alignItems="center" gap={2}>
            <VuiBox
              sx={{
                width: 40,
                height: 40,
                minWidth: 40,
                borderRadius: "10px",
                display: "grid",
                placeItems: "center",
                background: iconBg,
                color: iconColor,
              }}
            >
              <IoThermometer size="20px" />
            </VuiBox>
            <VuiBox>
              <VuiTypography variant="button" color="white" fontWeight="bold" display="block">
                {item.name}
              </VuiTypography>
              <VuiTypography variant="caption" color="text">
                {item.sensorId}
              </VuiTypography>
            </VuiBox>
          </VuiBox>
        </Grid>
        <Grid item xs={3}>
          <VuiBox display="flex" alignItems="center" gap={1}>
            <IoServer color="#a0aec0" size="14px" />
            <VuiTypography variant="button" color="text" fontWeight="medium">
              {item.nodeId}
            </VuiTypography>
          </VuiBox>
        </Grid>
        <Grid item xs={2}>
          <VuiTypography variant="button" color={valueColor} fontWeight="bold">
            {valFormatted} {item.unit}
          </VuiTypography>
        </Grid>
        <Grid item xs={2}>
          <VuiTypography variant="button" color="text">
            -
          </VuiTypography>
        </Grid>
        <Grid item xs={2}>
          <VuiTypography variant="button" color="text">
            -
          </VuiTypography>
        </Grid>
      </Grid>
    </VuiBox>
  );
}

function Sensors() {
  const [nodeId, setNodeId] = useState("all");
  const [q, setQ] = useState("");
  const { latestSensors } = useDashboardRealtime();

  const nodes = useMemo(() => {
    const byNode = new Map();
    latestSensors.forEach((s) => {
      const id = s.nodeIp || "unknown";
      if (!byNode.has(id)) {
        byNode.set(id, {
          id,
          name: s.meshLevel != null ? `Mesh L${s.meshLevel}` : "Mesh Node",
        });
      }
    });
    return Array.from(byNode.values()).sort((a, b) => a.id.localeCompare(b.id));
  }, [latestSensors]);

  const sensorRows = useMemo(
    () =>
      latestSensors.map((s) => ({
        rowId: s.key,
        name: s.sensor || "Unknown",
        sensorId: s.label || "value",
        nodeId: s.nodeIp || "unknown",
        value: Number(s.value),
        unit: s.unit || "",
      })),
    [latestSensors]
  );

  const filtered = useMemo(() => {
    const list = nodeId === "all" ? sensorRows : sensorRows.filter((s) => s.nodeId === nodeId);
    const query = q.trim().toLowerCase();
    if (!query) return list;
    return list.filter(
      (s) =>
        s.name.toLowerCase().includes(query) ||
        s.sensorId.toLowerCase().includes(query) ||
        s.nodeId.toLowerCase().includes(query)
    );
  }, [nodeId, q, sensorRows]);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        <Grid container spacing={3} mb={3}>
          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              variant="outlined"
              value={q}
              onChange={(e) => setQ(e.target.value)}
              placeholder="Search sensors (name/field/node)"
              sx={sensorFilterFieldSx}
              InputProps={{
                startAdornment: (
                  <InputAdornment position="start">
                    <IoSearch size="18px" color="rgba(255,255,255,0.45)" />
                  </InputAdornment>
                ),
              }}
            />
          </Grid>
          <Grid item xs={12} md={4} xl={3}>
            <FormControl fullWidth variant="outlined" sx={sensorFilterSelectSx}>
              <InputLabel id="sensor-filter-node-label" shrink>
                Filter by node
              </InputLabel>
              <Select
                labelId="sensor-filter-node-label"
                id="sensor-filter-node"
                value={nodeId}
                label="Filter by node"
                onChange={(e) => setNodeId(e.target.value)}
                MenuProps={sensorSelectMenuProps}
              >
                <MenuItem value="all">All nodes</MenuItem>
                {nodes.map((n) => (
                  <MenuItem key={n.id} value={n.id}>
                    {n.name} ({n.id})
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
        </Grid>

        <Card sx={{ padding: "24px 20px" }}>
          <VuiBox display="flex" justifyContent="space-between" alignItems="center" mb="24px">
            <VuiBox display="flex" alignItems="center" gap={1.5}>
              <VuiBox
                sx={{
                  width: 32,
                  height: 32,
                  borderRadius: "50%",
                  display: "grid",
                  placeItems: "center",
                  color: "#00d1a7",
                  background: "rgba(0, 209, 167, 0.15)",
                }}
              >
                <IoPulse size="18px" />
              </VuiBox>
              <VuiTypography variant="h5" color="white" fontWeight="bold">
                Sensor Data
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
            <Grid item xs={3}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                SENSOR
              </VuiTypography>
            </Grid>
            <Grid item xs={3}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                NODE
              </VuiTypography>
            </Grid>
            <Grid item xs={2}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                VALUE
              </VuiTypography>
            </Grid>
            <Grid item xs={2}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                THRESHOLD
              </VuiTypography>
            </Grid>
            <Grid item xs={2}>
              <VuiTypography variant="caption" color="text" fontWeight="bold" sx={{ textTransform: "uppercase" }}>
                MIN/MAX AVG
              </VuiTypography>
            </Grid>
          </Grid>

          <VuiBox display="flex" flexDirection="column">
            {filtered.map((s, idx) => (
              <SensorRow key={s.rowId} item={s} />
            ))}
            {filtered.length === 0 && (
              <VuiBox textAlign="center" py={4}>
                <VuiTypography color="text">No sensors found.</VuiTypography>
              </VuiBox>
            )}
          </VuiBox>
        </Card>
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default Sensors;

