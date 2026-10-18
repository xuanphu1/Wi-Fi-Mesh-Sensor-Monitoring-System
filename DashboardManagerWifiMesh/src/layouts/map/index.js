import React, { useState, useEffect, useMemo, useRef } from "react";
import L from "leaflet";
import "leaflet/dist/leaflet.css";

// @mui material components
import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";
import Button from "@mui/material/Button";
import Dialog from "@mui/material/Dialog";
import DialogTitle from "@mui/material/DialogTitle";
import DialogContent from "@mui/material/DialogContent";
import DialogActions from "@mui/material/DialogActions";
import TextField from "@mui/material/TextField";
import MenuItem from "@mui/material/MenuItem";
import Select from "@mui/material/Select";
import FormControl from "@mui/material/FormControl";
import InputLabel from "@mui/material/InputLabel";
import Chip from "@mui/material/Chip";
import IconButton from "@mui/material/IconButton";
import Tooltip from "@mui/material/Tooltip";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import { useMeshNodesFromWebSocket } from "hooks/useMeshNodesFromWebSocket";

import {
  IoMap,
  IoLocate,
  IoAddOutline,
  IoPencilOutline,
  IoCheckmarkCircle,
  IoCloseCircle,
  IoLayersOutline,
  IoGitNetworkOutline,
  IoRefreshOutline,
  IoWifi,
  IoRadio,
  IoNavigate,
} from "react-icons/io5";

// Palette colors for distinct Gateway Mesh Networks
const MESH_COLOR_PALETTES = [
  { primary: "#00d4ff", border: "#00b4d8", bg: "rgba(0, 212, 255, 0.22)", glow: "rgba(0, 212, 255, 0.5)", name: "Xanh Cyan" },
  { primary: "#01f7a7", border: "#00c885", bg: "rgba(1, 247, 167, 0.22)", glow: "rgba(1, 247, 167, 0.5)", name: "Xanh Lục" },
  { primary: "#b8a9ff", border: "#8a2be2", bg: "rgba(138, 44, 255, 0.22)", glow: "rgba(138, 44, 255, 0.5)", name: "Tím Mesh" },
  { primary: "#ffb547", border: "#ff9f1c", bg: "rgba(255, 181, 71, 0.22)", glow: "rgba(255, 181, 71, 0.5)", name: "Cam Hổ Phách" },
  { primary: "#ff3b94", border: "#e61c74", bg: "rgba(255, 59, 148, 0.22)", glow: "rgba(255, 59, 148, 0.5)", name: "Hồng Sen" },
  { primary: "#38bdf8", border: "#0284c7", bg: "rgba(56, 189, 248, 0.22)", glow: "rgba(56, 189, 248, 0.5)", name: "Xanh Dương" },
];

const cardSx = {
  background: "linear-gradient(127deg, rgba(6, 11, 40, 0.45) 0%, rgba(10, 14, 35, 0.35) 100%)",
  border: "1px solid rgba(255, 255, 255, 0.12)",
  boxShadow: "0 8px 32px rgba(0, 0, 0, 0.45)",
  borderRadius: "18px",
  backdropFilter: "blur(20px)",
  p: 2.5,
};

// Form dialog input styles
const formFieldSx = {
  "& .MuiOutlinedInput-root": {
    borderRadius: "12px",
    background: "rgba(15, 21, 55, 0.75)",
    border: "1px solid rgba(255, 255, 255, 0.15)",
    color: "#fff",
    "& fieldset": { borderColor: "transparent" },
    "&:hover fieldset": { borderColor: "transparent" },
    "&.Mui-focused": {
      borderColor: "#00d4ff",
      border: "1px solid #00d4ff",
    },
  },
  "& .MuiInputLabel-root": {
    color: "rgba(255, 255, 255, 0.7)",
    "&.Mui-focused": { color: "#00d4ff" },
  },
};

const MAP_TILE_PROVIDERS = {
  dark: {
    name: "Dark Theme",
    url: "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",
    attribution: '&copy; <a href="https://carto.com/">CARTO</a> &copy; OpenStreetMap',
    subdomains: "abcd",
    maxZoom: 20,
  },
  osm: {
    name: "OpenStreetMap",
    url: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>',
    subdomains: "abc",
    maxZoom: 19,
  },
  satellite: {
    name: "Vệ Tinh (Satellite)",
    url: "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
    attribution: "Tiles &copy; Esri &mdash; Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye",
    maxZoom: 18,
  },
};

export default function MeshMap() {
  const {
    gateways = [],
    nodes = [],
    updateGatewayMeta,
    updateNodeMeta,
  } = useMeshNodesFromWebSocket();

  const mapContainerRef = useRef(null);
  const mapInstanceRef = useRef(null);
  const tileLayerRef = useRef(null);
  const markersLayerRef = useRef(null);
  const linesLayerRef = useRef(null);

  const [tileMode, setTileMode] = useState("dark");
  const [selectedGatewayFilter, setSelectedGatewayFilter] = useState("all");

  // Coordinate dialog state
  const [dialogOpen, setDialogOpen] = useState(false);
  const [editingTarget, setEditingTarget] = useState(null); // { type: 'gateway'|'node', id: string, name: string, lat: number, lon: number, location: string }
  const [inputLat, setInputLat] = useState("");
  const [inputLon, setInputLon] = useState("");
  const [inputLocation, setInputLocation] = useState("");
  const [inputName, setInputName] = useState("");
  const [isPickingFromMap, setIsPickingFromMap] = useState(false);
  const [isSaving, setIsSaving] = useState(false);

  // Assign deterministic color palette per gateway
  const gatewayColorMap = useMemo(() => {
    const map = new Map();
    gateways.forEach((gw, idx) => {
      const palette = MESH_COLOR_PALETTES[idx % MESH_COLOR_PALETTES.length];
      map.set(gw.gw_id, palette);
    });
    return map;
  }, [gateways]);

  // Filtered devices with valid coordinates
  const positionedGateways = useMemo(() => {
    return gateways.filter((g) => {
      const hasCoords = Number.isFinite(Number(g.latitude)) && Number.isFinite(Number(g.longitude)) && (Number(g.latitude) !== 0 || Number(g.longitude) !== 0);
      if (!hasCoords) return false;
      if (selectedGatewayFilter !== "all" && g.gw_id !== selectedGatewayFilter) return false;
      return true;
    });
  }, [gateways, selectedGatewayFilter]);

  const positionedNodes = useMemo(() => {
    return nodes.filter((n) => {
      const hasCoords = Number.isFinite(Number(n.latitude)) && Number.isFinite(Number(n.longitude)) && (Number(n.latitude) !== 0 || Number(n.longitude) !== 0);
      if (!hasCoords) return false;
      if (selectedGatewayFilter !== "all" && (n.gw_id || "gw-default") !== selectedGatewayFilter) return false;
      return true;
    });
  }, [nodes, selectedGatewayFilter]);

  // Unpositioned devices list
  const unpositionedDevices = useMemo(() => {
    const list = [];
    gateways.forEach((g) => {
      const hasCoords = Number.isFinite(Number(g.latitude)) && Number.isFinite(Number(g.longitude)) && (Number(g.latitude) !== 0 || Number(g.longitude) !== 0);
      if (!hasCoords) {
        list.push({ type: "gateway", id: g.gw_id, mac: g.mac || g.root_mac, name: g.name || g.gw_id, gw_id: g.gw_id, location: g.location || "" });
      }
    });
    nodes.forEach((n) => {
      const hasCoords = Number.isFinite(Number(n.latitude)) && Number.isFinite(Number(n.longitude)) && (Number(n.latitude) !== 0 || Number(n.longitude) !== 0);
      if (!hasCoords) {
        list.push({ type: "node", id: n.mac || n.id, mac: n.mac || n.id, name: n.nodeName || n.name || n.mac || n.id, gw_id: n.gw_id || "gw-default", location: n.nodeLocation || n.location || "" });
      }
    });
    return list;
  }, [gateways, nodes]);

  // Open dialog to edit coordinates
  const handleOpenEdit = (device) => {
    setEditingTarget(device);
    setInputLat(device.latitude != null ? String(device.latitude) : "");
    setInputLon(device.longitude != null ? String(device.longitude) : "");
    setInputLocation(device.location || device.nodeLocation || "");
    setInputName(device.name || device.nodeName || device.id || "");
    setIsPickingFromMap(false);
    setDialogOpen(true);
  };

  const handleSaveCoordinates = async () => {
    if (!editingTarget) return;
    setIsSaving(true);
    const lat = inputLat.trim() !== "" ? Number(inputLat.trim()) : null;
    const lon = inputLon.trim() !== "" ? Number(inputLon.trim()) : null;

    try {
      if (editingTarget.type === "gateway") {
        await updateGatewayMeta(editingTarget.id, {
          name: inputName.trim() || editingTarget.name,
          location: inputLocation.trim(),
          latitude: lat,
          longitude: lon,
        });
      } else {
        await updateNodeMeta(editingTarget.id, {
          name: inputName.trim() || editingTarget.name,
          location: inputLocation.trim(),
          latitude: lat,
          longitude: lon,
        });
      }
      setDialogOpen(false);
      setEditingTarget(null);
    } catch (err) {
      console.error("Save coordinates failed:", err);
    } finally {
      setIsSaving(false);
    }
  };

  // Preset location helper
  const handleApplyPreset = (lat, lon, locName) => {
    setInputLat(String(lat));
    setInputLon(String(lon));
    if (!inputLocation && locName) setInputLocation(locName);
  };

  // Get current device GPS
  const handleGetDeviceGps = () => {
    if (!navigator.geolocation) {
      alert("Trình duyệt không hỗ trợ Geolocation");
      return;
    }
    navigator.geolocation.getCurrentPosition(
      (pos) => {
        setInputLat(String(pos.coords.latitude.toFixed(6)));
        setInputLon(String(pos.coords.longitude.toFixed(6)));
      },
      (err) => {
        alert("Không thể lấy toạ độ GPS: " + err.message);
      },
      { enableHighAccuracy: true, timeout: 10000 }
    );
  };

  // 1. Initialize Leaflet Map
  useEffect(() => {
    if (!mapContainerRef.current) return;
    if (mapInstanceRef.current) return;

    // Default center: Vietnam (Default center around Da Lat or Hanoi)
    const map = L.map(mapContainerRef.current, {
      center: [16.0544, 107.5], // Center of Vietnam
      zoom: 6,
      zoomControl: false,
    });

    L.control.zoom({ position: "bottomright" }).addTo(map);

    const provider = MAP_TILE_PROVIDERS[tileMode] || MAP_TILE_PROVIDERS.dark;
    const tileLayer = L.tileLayer(provider.url, {
      attribution: provider.attribution,
      maxZoom: provider.maxZoom,
      subdomains: provider.subdomains || "abc",
    }).addTo(map);

    const linesLayer = L.layerGroup().addTo(map);
    const markersLayer = L.layerGroup().addTo(map);

    tileLayerRef.current = tileLayer;
    linesLayerRef.current = linesLayer;
    markersLayerRef.current = markersLayer;
    mapInstanceRef.current = map;

    // Click handler for interactive point picker
    map.on("click", (e) => {
      if (isPickingFromMap) {
        setInputLat(e.latlng.lat.toFixed(6));
        setInputLon(e.latlng.lng.toFixed(6));
        setIsPickingFromMap(false);
      }
    });

    return () => {
      map.remove();
      mapInstanceRef.current = null;
    };
  }, []);

  // 2. Change Tile Layer
  useEffect(() => {
    if (!mapInstanceRef.current || !tileLayerRef.current) return;
    const provider = MAP_TILE_PROVIDERS[tileMode] || MAP_TILE_PROVIDERS.dark;
    tileLayerRef.current.setUrl(provider.url);
  }, [tileMode]);

  // 3. Render Markers & Topology Lines
  useEffect(() => {
    if (!mapInstanceRef.current || !markersLayerRef.current || !linesLayerRef.current) return;

    const markersGroup = markersLayerRef.current;
    const linesGroup = linesLayerRef.current;

    markersGroup.clearLayers();
    linesGroup.clearLayers();

    const bounds = [];
    const gatewayCoordsMap = new Map();

    // Render Gateways
    positionedGateways.forEach((gw) => {
      const lat = Number(gw.latitude);
      const lon = Number(gw.longitude);
      const latLng = [lat, lon];
      bounds.push(latLng);
      gatewayCoordsMap.set(gw.gw_id, latLng);

      const palette = gatewayColorMap.get(gw.gw_id) || MESH_COLOR_PALETTES[0];
      const isOnline = gw.status === "online";

      // Custom Gateway HTML Marker (Distinctive Hub Icon & Badge)
      const gwIcon = L.divIcon({
        className: "custom-gateway-marker",
        html: `
          <div style="
            position: relative;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
          ">
            <div style="
              width: 44px;
              height: 44px;
              border-radius: 12px;
              background: ${palette.bg};
              border: 2px solid ${palette.primary};
              display: flex;
              flex-direction: column;
              align-items: center;
              justify-content: center;
              box-shadow: 0 4px 14px rgba(0,0,0,0.6);
              transition: transform 0.2s ease;
            ">
              <span style="font-size: 10px; font-weight: 800; color: #fff; letter-spacing: 0.5px;">GW</span>
              <span style="font-size: 8px; font-weight: 700; color: ${palette.primary}; max-width: 38px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">
                ${gw.gw_id}
              </span>
            </div>
            <!-- Status Dot Indicator -->
            <div style="
              position: absolute;
              top: -3px;
              right: -3px;
              width: 12px;
              height: 12px;
              border-radius: 50%;
              background: ${isOnline ? "#01f7a7" : "#a0aec0"};
              border: 2px solid #060b28;
            "></div>
          </div>
        `,
        iconSize: [44, 44],
        iconAnchor: [22, 22],
      });

      const marker = L.marker(latLng, { icon: gwIcon }).addTo(markersGroup);

      // Popup Content for Gateway
      const popupHtml = `
        <div style="
          background: #0f1537;
          color: #ffffff;
          padding: 12px 14px;
          border-radius: 12px;
          border: 1px solid ${palette.primary};
          min-width: 220px;
          font-family: inherit;
        ">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 6px;">
            <span style="font-weight: 800; font-size: 13px; color: ${palette.primary};">
              [TRẠM GATEWAY]
            </span>
            <span style="
              font-size: 10px;
              font-weight: 700;
              padding: 2px 6px;
              border-radius: 6px;
              background: ${isOnline ? "rgba(1,247,167,0.2)" : "rgba(160,174,192,0.2)"};
              color: ${isOnline ? "#01f7a7" : "#a0aec0"};
              border: 1px solid ${isOnline ? "#01f7a7" : "#a0aec0"};
            ">
              ${isOnline ? "ONLINE" : "OFFLINE"}
            </span>
          </div>
          <div style="font-size: 14px; font-weight: bold; margin-bottom: 4px;">${gw.name || gw.gw_id}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 2px;">Vị trí: ${gw.location || "Chưa đặt tên"}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 2px;">MAC: ${gw.mac || gw.root_mac || "N/A"}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 2px;">IP: ${gw.ip || "N/A"}</div>
          <div style="font-size: 11px; color: #00d4ff; font-weight: bold; margin-bottom: 8px;">Mạng Mesh: ${gw.mesh_id || "mesh-default"} (${gw.node_count || 0} nodes)</div>
          <div style="font-size: 10px; color: #718096; margin-bottom: 8px;">Toạ độ: ${lat.toFixed(5)}, ${lon.toFixed(5)}</div>
          <button 
            id="btn-edit-gw-${gw.gw_id}" 
            style="
              width: 100%;
              padding: 6px 10px;
              border-radius: 8px;
              border: none;
              background: ${palette.primary};
              color: #060b28;
              font-weight: bold;
              font-size: 11px;
              cursor: pointer;
            "
          >
            Chỉnh Sửa Toạ Độ
          </button>
        </div>
      `;

      marker.bindPopup(popupHtml, { className: "custom-leaflet-popup" });
      marker.on("popupopen", () => {
        const btn = document.getElementById(`btn-edit-gw-${gw.gw_id}`);
        if (btn) {
          btn.onclick = () => handleOpenEdit({ ...gw, type: "gateway", id: gw.gw_id });
        }
      });
    });

    // Render Nodes (Color-coded to match their Gateway's Mesh Network)
    positionedNodes.forEach((node) => {
      const lat = Number(node.latitude);
      const lon = Number(node.longitude);
      const latLng = [lat, lon];
      bounds.push(latLng);

      const gwId = node.gw_id || "gw-default";
      const palette = gatewayColorMap.get(gwId) || MESH_COLOR_PALETTES[0];
      const isOnline = Boolean(node.online);
      const meshLevel = node.meshLevel != null ? node.meshLevel : 1;

      // Draw dashed topology line connecting Node to its parent Gateway!
      const gwCoords = gatewayCoordsMap.get(gwId);
      if (gwCoords) {
        L.polyline([gwCoords, latLng], {
          color: palette.primary,
          weight: 2,
          opacity: 0.75,
          dashArray: "6, 8",
        }).addTo(linesGroup);
      }

      // Custom Node Marker (Circle with Mesh Level and parent Gateway color)
      const nodeIcon = L.divIcon({
        className: "custom-node-marker",
        html: `
          <div style="
            position: relative;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
          ">
            <div style="
              width: 32px;
              height: 32px;
              border-radius: 50%;
              background: ${palette.bg};
              border: 2px solid ${palette.primary};
              display: flex;
              flex-direction: column;
              align-items: center;
              justify-content: center;
              box-shadow: 0 3px 10px rgba(0,0,0,0.5);
              transition: transform 0.2s ease;
            ">
              <span style="font-size: 9px; font-weight: 800; color: #ffffff;">L${meshLevel}</span>
            </div>
            <!-- Status Dot Indicator -->
            <div style="
              position: absolute;
              bottom: -2px;
              right: -2px;
              width: 9px;
              height: 9px;
              border-radius: 50%;
              background: ${isOnline ? "#01f7a7" : "#ff5252"};
              border: 1.5px solid #060b28;
            "></div>
          </div>
        `,
        iconSize: [32, 32],
        iconAnchor: [16, 16],
      });

      const marker = L.marker(latLng, { icon: nodeIcon }).addTo(markersGroup);

      const popupHtml = `
        <div style="
          background: #0f1537;
          color: #ffffff;
          padding: 12px 14px;
          border-radius: 12px;
          border: 1px solid ${palette.primary};
          min-width: 220px;
          font-family: inherit;
        ">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 6px;">
            <span style="font-weight: 800; font-size: 12px; color: ${palette.primary};">
              [NODE MESH L${meshLevel}]
            </span>
            <span style="
              font-size: 10px;
              font-weight: 700;
              padding: 2px 6px;
              border-radius: 6px;
              background: ${isOnline ? "rgba(1,247,167,0.2)" : "rgba(255,82,82,0.2)"};
              color: ${isOnline ? "#01f7a7" : "#ff5252"};
              border: 1px solid ${isOnline ? "#01f7a7" : "#ff5252"};
            ">
              ${isOnline ? "ONLINE" : "OFFLINE"}
            </span>
          </div>
          <div style="font-size: 13px; font-weight: bold; margin-bottom: 4px;">${node.nodeName || node.name || node.mac || node.id}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 2px;">Vị trí: ${node.nodeLocation || node.location || "Chưa đặt vị trí"}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 2px;">MAC / IP: ${node.mac || node.id}</div>
          <div style="font-size: 11px; color: ${palette.primary}; font-weight: bold; margin-bottom: 2px;">Gateway: ${gwId}</div>
          <div style="font-size: 11px; color: #a0aec0; margin-bottom: 4px;">Cảm biến: ${node.portCount || 0} cổng</div>
          <div style="font-size: 10px; color: #718096; margin-bottom: 8px;">Toạ độ: ${lat.toFixed(5)}, ${lon.toFixed(5)}</div>
          <button 
            id="btn-edit-node-${node.id.replace(/[^a-zA-Z0-9]/g, '_')}" 
            style="
              width: 100%;
              padding: 6px 10px;
              border-radius: 8px;
              border: none;
              background: ${palette.primary};
              color: #060b28;
              font-weight: bold;
              font-size: 11px;
              cursor: pointer;
            "
          >
            Chỉnh Sửa Toạ Độ
          </button>
        </div>
      `;

      marker.bindPopup(popupHtml, { className: "custom-leaflet-popup" });
      marker.on("popupopen", () => {
        const btnId = `btn-edit-node-${node.id.replace(/[^a-zA-Z0-9]/g, '_')}`;
        const btn = document.getElementById(btnId);
        if (btn) {
          btn.onclick = () => handleOpenEdit({ ...node, type: "node", id: node.mac || node.id });
        }
      });
    });

    // Auto fit map view if devices are positioned
    if (bounds.length > 0 && !isPickingFromMap) {
      if (bounds.length === 1) {
        mapInstanceRef.current.setView(bounds[0], 14);
      } else {
        mapInstanceRef.current.fitBounds(bounds, { padding: [50, 50], maxZoom: 16 });
      }
    }
  }, [positionedGateways, positionedNodes, gatewayColorMap, isPickingFromMap]);

  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={2}>
        {/* Header Control Card */}
        <Card sx={{ ...cardSx, mb: 2.5 }}>
          <Grid container spacing={2} alignItems="center">
            {/* Title */}
            <Grid item xs={12} md={5}>
              <VuiBox display="flex" alignItems="center" gap={1.5}>
                <VuiBox
                  sx={{
                    width: 44,
                    height: 44,
                    borderRadius: "12px",
                    background: "rgba(0, 212, 255, 0.15)",
                    border: "1px solid #00d4ff",
                    display: "grid",
                    placeItems: "center",
                  }}
                >
                  <IoMap size="22px" color="#00d4ff" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="h5" color="white" fontWeight="bold">
                    Bản Đồ Phân Bố Mạng Mesh
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text">
                    Định vị toạ độ Gateway, Mesh Nodes & liên kết topo theo từng trạm
                  </VuiTypography>
                </VuiBox>
              </VuiBox>
            </Grid>

            {/* Filter by Gateway */}
            <Grid item xs={12} sm={6} md={3.5}>
              <FormControl fullWidth size="small" sx={formFieldSx}>
                <InputLabel id="map-gw-filter-label">Lọc theo Gateway</InputLabel>
                <Select
                  labelId="map-gw-filter-label"
                  value={selectedGatewayFilter}
                  label="Lọc theo Gateway"
                  onChange={(e) => setSelectedGatewayFilter(e.target.value)}
                  sx={{ color: "#fff", borderRadius: "12px" }}
                >
                  <MenuItem value="all">Tất Cả Mạng Mesh ({gateways.length} Gateway)</MenuItem>
                  {gateways.map((g) => {
                    const palette = gatewayColorMap.get(g.gw_id) || MESH_COLOR_PALETTES[0];
                    return (
                      <MenuItem key={g.gw_id} value={g.gw_id}>
                        <VuiBox display="flex" alignItems="center" gap={1}>
                          <VuiBox sx={{ width: 10, height: 10, borderRadius: "50%", background: palette.primary }} />
                          <span>{g.name || g.gw_id} ({g.gw_id})</span>
                        </VuiBox>
                      </MenuItem>
                    );
                  })}
                </Select>
              </FormControl>
            </Grid>

            {/* Tile Switcher & Add Coordinates Button */}
            <Grid item xs={12} sm={6} md={3.5} display="flex" justifyContent="flex-end" gap={1}>
              {/* Tile Mode Toggle */}
              <Tooltip title="Đổi kiểu bản đồ: Tối / Sáng / Vệ tinh">
                <Button
                  size="small"
                  variant="outlined"
                  onClick={() => {
                    const modes = ["dark", "osm", "satellite"];
                    const next = modes[(modes.indexOf(tileMode) + 1) % modes.length];
                    setTileMode(next);
                  }}
                  sx={{
                    color: "#fff",
                    borderColor: "rgba(255,255,255,0.2)",
                    borderRadius: "10px",
                    textTransform: "none",
                    fontSize: "0.8rem",
                    display: "flex",
                    alignItems: "center",
                    gap: 0.8,
                    "&:hover": { borderColor: "#00d4ff" },
                  }}
                >
                  <IoLayersOutline size="16px" />
                  {MAP_TILE_PROVIDERS[tileMode]?.name}
                </Button>
              </Tooltip>

              <Button
                size="small"
                variant="contained"
                onClick={() => {
                  // Pick first available device
                  const first = unpositionedDevices[0] || (gateways[0] ? { ...gateways[0], type: "gateway", id: gateways[0].gw_id } : null);
                  if (first) handleOpenEdit(first);
                  else setDialogOpen(true);
                }}
                sx={{
                  background: "#0075ff",
                  color: "#fff",
                  borderRadius: "10px",
                  textTransform: "none",
                  fontWeight: "bold",
                  display: "flex",
                  alignItems: "center",
                  gap: 0.8,
                  "&:hover": { background: "#0055c4" },
                }}
              >
                <IoAddOutline size="18px" />
                Khai Báo Toạ Độ
              </Button>
            </Grid>
          </Grid>

          {/* Quick Stats & Mesh Color Legend Bar */}
          <VuiBox
            display="flex"
            flexWrap="wrap"
            alignItems="center"
            justifyContent="space-between"
            mt={2}
            pt={2}
            borderTop="1px solid rgba(255, 255, 255, 0.08)"
            gap={2}
          >
            {/* Stats chips */}
            <VuiBox display="flex" alignItems="center" gap={1.5} flexWrap="wrap">
              <Chip
                size="small"
                label={`Đã định vị: ${positionedGateways.length}/${gateways.length} Gateway · ${positionedNodes.length}/${nodes.length} Nodes`}
                sx={{ background: "rgba(0, 212, 255, 0.15)", color: "#00d4ff", border: "1px solid rgba(0,212,255,0.3)" }}
              />
              {unpositionedDevices.length > 0 && (
                <Chip
                  size="small"
                  label={`Chưa có toạ độ: ${unpositionedDevices.length} thiết bị`}
                  sx={{ background: "rgba(255, 181, 71, 0.15)", color: "#ffb547", border: "1px solid rgba(255,181,71,0.3)" }}
                />
              )}
            </VuiBox>

            {/* Mesh Colors Legend */}
            <VuiBox display="flex" alignItems="center" gap={2} flexWrap="wrap">
              <VuiTypography variant="caption" color="text" fontWeight="bold">
                Màu sắc nhận diện Mesh:
              </VuiTypography>
              {gateways.map((g) => {
                const palette = gatewayColorMap.get(g.gw_id) || MESH_COLOR_PALETTES[0];
                return (
                  <VuiBox key={g.gw_id} display="flex" alignItems="center" gap={0.8}>
                    <VuiBox
                      sx={{
                        width: 14,
                        height: 14,
                        borderRadius: "4px",
                        background: palette.primary,
                        boxShadow: `0 0 8px ${palette.glow}`,
                      }}
                    />
                    <VuiTypography variant="caption" color="white" fontWeight="medium">
                      {g.name || g.gw_id}
                    </VuiTypography>
                  </VuiBox>
                );
              })}
            </VuiBox>
          </VuiBox>
        </Card>

        {/* Map Container Area */}
        <Grid container spacing={2}>
          {/* Main Map Canvas */}
          <Grid item xs={12} lg={unpositionedDevices.length > 0 ? 9 : 12}>
            <Card sx={{ ...cardSx, p: 0, overflow: "hidden", position: "relative" }}>
              {isPickingFromMap && (
                <VuiBox
                  sx={{
                    position: "absolute",
                    top: 16,
                    left: "50%",
                    transform: "translateX(-50%)",
                    zIndex: 1000,
                    background: "rgba(0, 117, 255, 0.9)",
                    backdropFilter: "blur(10px)",
                    color: "#fff",
                    px: 3,
                    py: 1,
                    borderRadius: "20px",
                    boxShadow: "0 4px 20px rgba(0,0,0,0.5)",
                    display: "flex",
                    alignItems: "center",
                    gap: 1.5,
                    border: "1px solid rgba(255,255,255,0.4)",
                  }}
                >
                  <IoLocate size="18px" />
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    Click trên bản đồ để chọn toạ độ cho {editingTarget?.name || "thiết bị"}
                  </VuiTypography>
                  <Button
                    size="small"
                    onClick={() => setIsPickingFromMap(false)}
                    sx={{ color: "#fff", textDecoration: "underline", minWidth: 0, p: 0 }}
                  >
                    Hủy
                  </Button>
                </VuiBox>
              )}

              <div
                ref={mapContainerRef}
                style={{
                  width: "100%",
                  height: "680px",
                  borderRadius: "18px",
                  zIndex: 1,
                }}
              />
            </Card>
          </Grid>

          {/* Sidebar Panel for Unpositioned Devices */}
          {unpositionedDevices.length > 0 && (
            <Grid item xs={12} lg={3}>
              <Card sx={{ ...cardSx, height: "680px", display: "flex", flexDirection: "column" }}>
                <VuiBox mb={2}>
                  <VuiTypography variant="button" color="white" fontWeight="bold">
                    Thiết Bị Cần Đặt Toạ Độ ({unpositionedDevices.length})
                  </VuiTypography>
                  <VuiTypography variant="caption" color="text" display="block">
                    Khai báo toạ độ để thiết bị hiển thị lên bản đồ và kết nối topo
                  </VuiTypography>
                </VuiBox>

                <VuiBox sx={{ flex: 1, overflowY: "auto", pr: 0.5 }}>
                  {unpositionedDevices.map((d) => (
                    <VuiBox
                      key={`${d.type}-${d.id}`}
                      sx={{
                        p: 1.6,
                        mb: 1.2,
                        borderRadius: "12px",
                        background: "rgba(15, 21, 55, 0.6)",
                        border: "1px solid rgba(255, 255, 255, 0.08)",
                        display: "flex",
                        alignItems: "center",
                        justifyContent: "space-between",
                        transition: "all 0.2s ease",
                        "&:hover": {
                          borderColor: "#00d4ff",
                          background: "rgba(0, 212, 255, 0.08)",
                        },
                      }}
                    >
                      <VuiBox>
                        <VuiBox display="flex" alignItems="center" gap={0.8} mb={0.3}>
                          <Chip
                            size="small"
                            label={d.type === "gateway" ? "GATEWAY" : "NODE"}
                            sx={{
                              height: 18,
                              fontSize: "0.65rem",
                              fontWeight: "bold",
                              background: d.type === "gateway" ? "rgba(0,212,255,0.2)" : "rgba(1,247,167,0.2)",
                              color: d.type === "gateway" ? "#00d4ff" : "#01f7a7",
                            }}
                          />
                          <VuiTypography variant="caption" color="white" fontWeight="bold">
                            {d.name}
                          </VuiTypography>
                        </VuiBox>
                        <VuiTypography variant="caption" color="text" sx={{ fontSize: "10px" }} display="block">
                          MAC: {d.mac || d.id}
                        </VuiTypography>
                      </VuiBox>

                      <Button
                        size="small"
                        variant="outlined"
                        onClick={() => handleOpenEdit(d)}
                        sx={{
                          minWidth: 0,
                          px: 1.5,
                          py: 0.5,
                          borderRadius: "8px",
                          borderColor: "rgba(255,255,255,0.2)",
                          color: "#00d4ff",
                          fontSize: "11px",
                          textTransform: "none",
                          "&:hover": { borderColor: "#00d4ff", background: "rgba(0,212,255,0.15)" },
                        }}
                      >
                        + Đặt
                      </Button>
                    </VuiBox>
                  ))}
                </VuiBox>
              </Card>
            </Grid>
          )}
        </Grid>
      </VuiBox>

      {/* Dialog Khai Báo / Chỉnh Sửa Toạ Độ */}
      <Dialog
        open={dialogOpen}
        onClose={() => !isSaving && setDialogOpen(false)}
        maxWidth="sm"
        fullWidth
        PaperProps={{
          sx: {
            background: "linear-gradient(127deg, rgba(15, 21, 55, 0.95) 0%, rgba(20, 26, 65, 0.95) 100%)",
            backdropFilter: "blur(25px)",
            border: "1px solid rgba(255, 255, 255, 0.15)",
            borderRadius: "20px",
            color: "#fff",
            p: 1,
          },
        }}
      >
        <DialogTitle sx={{ color: "#fff", fontWeight: "bold", display: "flex", alignItems: "center", gap: 1 }}>
          <IoLocate size="22px" color="#00d4ff" />
          Khai Báo Toạ Độ Thiết Bị
        </DialogTitle>

        <DialogContent dividers sx={{ borderColor: "rgba(255,255,255,0.1)" }}>
          {/* Target device selector */}
          <FormControl fullWidth size="small" sx={{ ...formFieldSx, mb: 2.5, mt: 1 }}>
            <InputLabel id="target-device-select-label">Chọn Thiết Bị Cần Gán Toạ Độ</InputLabel>
            <Select
              labelId="target-device-select-label"
              value={editingTarget ? `${editingTarget.type}:${editingTarget.id}` : ""}
              label="Chọn Thiết Bị Cần Gán Toạ Độ"
              onChange={(e) => {
                const [type, id] = e.target.value.split(":");
                if (type === "gateway") {
                  const gw = gateways.find((g) => g.gw_id === id);
                  if (gw) handleOpenEdit({ ...gw, type: "gateway", id: gw.gw_id });
                } else {
                  const nd = nodes.find((n) => (n.mac || n.id) === id);
                  if (nd) handleOpenEdit({ ...nd, type: "node", id: nd.mac || nd.id });
                }
              }}
              sx={{ color: "#fff" }}
            >
              <MenuItem disabled value="" sx={{ fontWeight: "bold", color: "#a0aec0" }}>
                -- DANH SÁCH GATEWAYS --
              </MenuItem>
              {gateways.map((g) => (
                <MenuItem key={`gw-${g.gw_id}`} value={`gateway:${g.gw_id}`}>
                  [GATEWAY] {g.name || g.gw_id} ({g.gw_id})
                </MenuItem>
              ))}

              <MenuItem disabled value="" sx={{ fontWeight: "bold", color: "#a0aec0", mt: 1 }}>
                -- DANH SÁCH MESH NODES --
              </MenuItem>
              {nodes.map((n) => (
                <MenuItem key={`node-${n.mac || n.id}`} value={`node:${n.mac || n.id}`}>
                  [NODE] {n.nodeName || n.name || n.mac || n.id} (L{n.meshLevel ?? 1})
                </MenuItem>
              ))}
            </Select>
          </FormControl>

          <Grid container spacing={2} mb={2}>
            {/* Latitude */}
            <Grid item xs={12} sm={6}>
              <TextField
                fullWidth
                size="small"
                label="Vĩ độ (Latitude)"
                placeholder="Ví dụ: 11.940419"
                value={inputLat}
                onChange={(e) => setInputLat(e.target.value)}
                sx={formFieldSx}
              />
            </Grid>

            {/* Longitude */}
            <Grid item xs={12} sm={6}>
              <TextField
                fullWidth
                size="small"
                label="Kinh độ (Longitude)"
                placeholder="Ví dụ: 108.458313"
                value={inputLon}
                onChange={(e) => setInputLon(e.target.value)}
                sx={formFieldSx}
              />
            </Grid>
          </Grid>

          {/* Quick interactive helpers */}
          <VuiBox display="flex" gap={1.2} mb={2.5} flexWrap="wrap">
            <Button
              size="small"
              variant="outlined"
              onClick={() => {
                setDialogOpen(false);
                setIsPickingFromMap(true);
              }}
              sx={{
                color: "#00d4ff",
                borderColor: "rgba(0, 212, 255, 0.4)",
                textTransform: "none",
                fontSize: "11px",
                borderRadius: "8px",
                display: "flex",
                alignItems: "center",
                gap: 0.6,
                "&:hover": { borderColor: "#00d4ff", background: "rgba(0,212,255,0.15)" },
              }}
            >
              <IoNavigate size="14px" />
              Chọn trên bản đồ
            </Button>

            <Button
              size="small"
              variant="outlined"
              onClick={handleGetDeviceGps}
              sx={{
                color: "#01f7a7",
                borderColor: "rgba(1, 247, 167, 0.4)",
                textTransform: "none",
                fontSize: "11px",
                borderRadius: "8px",
                display: "flex",
                alignItems: "center",
                gap: 0.6,
                "&:hover": { borderColor: "#01f7a7", background: "rgba(1,247,167,0.15)" },
              }}
            >
              <IoLocate size="14px" />
              Lấy GPS của tôi
            </Button>

            {/* Presets */}
            <Button
              size="small"
              variant="text"
              onClick={() => handleApplyPreset(11.9404, 108.4583, "Đà Lạt - Lô A1")}
              sx={{ color: "rgba(255,255,255,0.7)", fontSize: "11px", textTransform: "none" }}
            >
              Đà Lạt
            </Button>
            <Button
              size="small"
              variant="text"
              onClick={() => handleApplyPreset(21.0285, 105.8544, "Hà Nội - Khu B2")}
              sx={{ color: "rgba(255,255,255,0.7)", fontSize: "11px", textTransform: "none" }}
            >
              Hà Nội
            </Button>
            <Button
              size="small"
              variant="text"
              onClick={() => handleApplyPreset(13.9833, 108.0, "Gia Lai - Trạm Khí Tượng")}
              sx={{ color: "rgba(255,255,255,0.7)", fontSize: "11px", textTransform: "none" }}
            >
              Gia Lai
            </Button>
          </VuiBox>

          {/* Location Name & Address */}
          <TextField
            fullWidth
            size="small"
            label="Tên Vị Trí / Địa Điểm"
            placeholder="Ví dụ: Vườn Ươm Khu A, Trụ Cột Số 2..."
            value={inputLocation}
            onChange={(e) => setInputLocation(e.target.value)}
            sx={{ ...formFieldSx, mb: 2 }}
          />

          {/* Device Display Name */}
          <TextField
            fullWidth
            size="small"
            label="Tên Gọi Thiết Bị"
            value={inputName}
            onChange={(e) => setInputName(e.target.value)}
            sx={formFieldSx}
          />
        </DialogContent>

        <DialogActions sx={{ p: 2, borderColor: "rgba(255,255,255,0.1)" }}>
          <Button
            onClick={() => setDialogOpen(false)}
            disabled={isSaving}
            sx={{ color: "rgba(255,255,255,0.7)", textTransform: "none" }}
          >
            Hủy
          </Button>
          <Button
            onClick={handleSaveCoordinates}
            disabled={isSaving || !editingTarget}
            variant="contained"
            sx={{
              background: "#0075ff",
              color: "#fff",
              fontWeight: "bold",
              textTransform: "none",
              borderRadius: "10px",
              px: 3,
              "&:hover": { background: "#0055c4" },
            }}
          >
            {isSaving ? "Đang lưu..." : "Lưu Toạ Độ"}
          </Button>
        </DialogActions>
      </Dialog>
      <Footer />
    </DashboardLayout>
  );
}
