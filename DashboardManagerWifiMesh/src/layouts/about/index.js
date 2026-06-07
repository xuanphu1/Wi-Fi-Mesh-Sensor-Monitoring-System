import Card from "@mui/material/Card";
import Grid from "@mui/material/Grid";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";

// Vision UI Dashboard React example components
import DashboardLayout from "examples/LayoutContainers/DashboardLayout";
import DashboardNavbar from "examples/Navbars/DashboardNavbar";
import Footer from "examples/Footer";

import {
  IoShareSocialOutline,
  IoBookOutline,
  IoCodeSlashOutline,
  IoLogoGithub,
  IoChevronForward,
  IoHardwareChip,
  IoMailOutline,
  IoShieldCheckmarkOutline,
  IoOpenOutline,
  IoGlobeOutline
} from "react-icons/io5";

// Simmmple logo like "S"
const LogoIcon = () => (
  <VuiTypography color="white" fontWeight="bold" sx={{ fontSize: "52px", lineHeight: 1, letterSpacing: "-2px" }}>
    S
  </VuiTypography>
);

function QuickLink({ icon, title, subtitle, color, bg }) {
  return (
    <VuiBox display="flex" alignItems="center" justifyContent="space-between" mb={2.5} sx={{ cursor: "pointer", "&:hover": { opacity: 0.8 } }}>
      <VuiBox display="flex" alignItems="center" gap={2}>
        <VuiBox sx={{ width: 40, height: 40, borderRadius: "10px", background: bg, color: color, display: "grid", placeItems: "center" }}>
          {icon}
        </VuiBox>
        <VuiBox>
          <VuiTypography variant="button" color="white" fontWeight="bold" display="block">{title}</VuiTypography>
          <VuiTypography variant="caption" color="text">{subtitle}</VuiTypography>
        </VuiBox>
      </VuiBox>
      <IoChevronForward color="rgba(255,255,255,0.4)" />
    </VuiBox>
  );
}

function About() {
  return (
    <DashboardLayout>
      <DashboardNavbar />
      <VuiBox py={3}>
        
        {/* Top Card */}
        <Card sx={{
          padding: "32px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <Grid container spacing={4} alignItems="center">
            {/* Logo Section */}
            <Grid item xs={12} md={3} sx={{ textAlign: "center", borderRight: { md: "1px solid rgba(255,255,255,0.05)" } }}>
              <VuiBox display="flex" flexDirection="column" alignItems="center" justifyContent="center">
                <VuiBox sx={{ 
                  width: 90, height: 90, borderRadius: "24px", 
                  background: "linear-gradient(135deg, #0075FF 0%, #8A2CFF 100%)",
                  display: "grid", placeItems: "center", mb: 2,
                  boxShadow: "0 8px 24px rgba(138,44,255,0.4)"
                }}>
                  <LogoIcon />
                </VuiBox>
                <VuiTypography variant="h5" color="white" fontWeight="bold">MSMS Dashboard</VuiTypography>
                <VuiBox sx={{ px: 1.5, py: 0.5, borderRadius: "6px", background: "rgba(138,44,255,0.2)", mt: 1 }}>
                  <VuiTypography variant="caption" fontWeight="bold" sx={{ color: "#8a2cff" }}>v1.0.0</VuiTypography>
                </VuiBox>
              </VuiBox>
            </Grid>

            {/* Description Section */}
            <Grid item xs={12} md={5} sx={{ borderRight: { md: "1px solid rgba(255,255,255,0.05)" }, pr: { md: 4 } }}>
              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>Project name</VuiTypography>
              <VuiTypography variant="h5" color="white" fontWeight="bold" mb={3}>Root/Node Sensor Dashboard</VuiTypography>
              
              <VuiTypography variant="caption" color="text" display="block" mb={1}>Description</VuiTypography>
              <VuiTypography variant="button" color="text" display="block" mb={2}>
                Root provides a Wi-Fi AP, nodes send sensor data to Root, and the dashboard fetches data from Root to display realtime + history + OTA.
              </VuiTypography>
              <VuiTypography variant="button" color="text" display="block">
                We'll add a system diagram, webserver/firmware versions, and a quick-start guide here.
              </VuiTypography>
            </Grid>

            {/* Quick Links Section */}
            <Grid item xs={12} md={4} sx={{ pl: { md: 4 } }}>
              <VuiTypography variant="button" color="white" fontWeight="bold" display="block" mb={3}>Quick Links</VuiTypography>
              
              <QuickLink 
                icon={<IoShareSocialOutline size="20px" />} 
                title="System Diagram" subtitle="View architecture diagram" 
                color="#8a2cff" bg="rgba(138,44,255,0.15)" 
              />
              <QuickLink 
                icon={<IoBookOutline size="20px" />} 
                title="Quick Start Guide" subtitle="Get started in minutes" 
                color="#0075ff" bg="rgba(0,117,255,0.15)" 
              />
              <QuickLink 
                icon={<IoCodeSlashOutline size="20px" />} 
                title="API Documentation" subtitle="View API endpoints" 
                color="#01f7a7" bg="rgba(1,247,167,0.15)" 
              />
              <QuickLink 
                icon={<IoLogoGithub size="20px" />} 
                title="GitHub Repository" subtitle="View source code" 
                color="#ffffff" bg="rgba(255,255,255,0.1)" 
              />
            </Grid>
          </Grid>
        </Card>

        {/* Firmware Device Card */}
        <Card sx={{
          padding: "24px 32px",
          background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
          border: "1px solid rgba(255, 255, 255, 0.08)",
          boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
          borderRadius: "16px",
          backdropFilter: "blur(42px)",
          mb: 3
        }}>
          <VuiTypography variant="h6" color="white" fontWeight="bold" mb={3}>
            Firmware Device
          </VuiTypography>

          <Grid container spacing={3}>
            <Grid item xs={12} md={3} sx={{ borderRight: { md: "1px solid rgba(255,255,255,0.05)" } }}>
              <VuiBox display="flex" gap={2}>
                <VuiBox sx={{ width: 48, height: 48, borderRadius: "12px", background: "rgba(0,117,255,0.15)", color: "#0075ff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                  <IoHardwareChip size="24px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="caption" color="text" display="block">Firmware Version</VuiTypography>
                  <VuiTypography variant="h5" color="white" fontWeight="bold" display="inline-block" mb={1}>v1.0.1 </VuiTypography>
                  <VuiTypography variant="button" sx={{ color: "#01f7a7", ml: 1, fontWeight: "bold" }}>(stable)</VuiTypography>
                  
                  <VuiTypography variant="caption" color="text" display="block">Latest: <span style={{ color: "#0075ff" }}>v1.0.1</span></VuiTypography>
                  <VuiTypography variant="caption" color="text" display="block">Released: 05/24/2026</VuiTypography>
                </VuiBox>
              </VuiBox>
            </Grid>
            
            <Grid item xs={12} md={3} sx={{ borderRight: { md: "1px solid rgba(255,255,255,0.05)" }, px: { md: 3 } }}>
              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>Device Type</VuiTypography>
              <VuiTypography variant="button" color="white" fontWeight="bold" display="block" mb={1.5}>Unified (Root + Node)</VuiTypography>
              <VuiTypography variant="caption" color="text" display="block">This firmware runs on both Root (ESP32) and Nodes.</VuiTypography>
            </Grid>

            <Grid item xs={12} md={3} sx={{ borderRight: { md: "1px solid rgba(255,255,255,0.05)" }, px: { md: 3 } }}>
              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>Build Info</VuiTypography>
              <VuiTypography variant="caption" color="text" display="block" mb={1}>Built with PlatformIO</VuiTypography>
              <VuiTypography variant="caption" color="text" display="block" mb={0.5}>Framework: <span style={{ color: "#fff" }}>Arduino</span></VuiTypography>
              <VuiTypography variant="caption" color="text" display="block">Platform: <span style={{ color: "#fff" }}>ESP32</span></VuiTypography>
            </Grid>

            <Grid item xs={12} md={3} sx={{ pl: { md: 3 } }}>
              <VuiTypography variant="caption" color="text" display="block" mb={1}>Release Notes</VuiTypography>
              <ul style={{ margin: 0, paddingLeft: 16, color: "rgba(255,255,255,0.6)", fontSize: 12 }}>
                <li style={{ paddingBottom: 4 }}>Improved Wi-Fi stability</li>
                <li style={{ paddingBottom: 4 }}>Added sensor auto-reconnect</li>
                <li style={{ paddingBottom: 4 }}>Optimized memory usage</li>
              </ul>
              <VuiBox mt={2}>
                <a href="#" style={{ color: "#0075ff", fontSize: 13, textDecoration: "none", display: "flex", alignItems: "center", gap: 6 }}>
                  View full changelog <IoOpenOutline />
                </a>
              </VuiBox>
            </Grid>
          </Grid>
        </Card>

        {/* Bottom Cards */}
        <Grid container spacing={3}>
          {/* Contact & Support */}
          <Grid item xs={12} md={6}>
            <Card sx={{
              padding: "24px 32px",
              background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
              border: "1px solid rgba(255, 255, 255, 0.08)",
              boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
              borderRadius: "16px",
              backdropFilter: "blur(42px)",
              height: "100%",
              display: "flex",
              flexDirection: "column",
              justifyContent: "space-between"
            }}>
              <VuiBox display="flex" gap={2} mb={4}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(0,117,255,0.15)", color: "#0075ff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                  <IoMailOutline size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">Contact & Support</VuiTypography>
                  <VuiTypography variant="caption" color="text">For support or feedback, please reach out to the project maintainer.</VuiTypography>
                </VuiBox>
              </VuiBox>

              <VuiBox display="flex" gap={4}>
                <a href="#" style={{ color: "#0075ff", fontSize: 13, textDecoration: "none", display: "flex", alignItems: "center", gap: 8 }}>
                  <IoMailOutline size="16px" /> support@rootnode.local
                </a>
                <a href="#" style={{ color: "#0075ff", fontSize: 13, textDecoration: "none", display: "flex", alignItems: "center", gap: 8 }}>
                  <IoGlobeOutline size="16px" /> https://rootnode.local
                </a>
              </VuiBox>
            </Card>
          </Grid>

          {/* Legal */}
          <Grid item xs={12} md={6}>
            <Card sx={{
              padding: "24px 32px",
              background: "linear-gradient(127deg, rgba(6, 11, 40, 0.74) 0%, rgba(10, 14, 35, 0.72) 100%)",
              border: "1px solid rgba(255, 255, 255, 0.08)",
              boxShadow: "0 8px 32px rgba(0, 0, 0, 0.28)",
              borderRadius: "16px",
              backdropFilter: "blur(42px)",
              height: "100%",
              display: "flex",
              flexDirection: "column",
              justifyContent: "space-between"
            }}>
              <VuiBox display="flex" gap={2} mb={4}>
                <VuiBox sx={{ width: 44, height: 44, borderRadius: "12px", background: "rgba(138,44,255,0.15)", color: "#8a2cff", display: "grid", placeItems: "center", flexShrink: 0 }}>
                  <IoShieldCheckmarkOutline size="22px" />
                </VuiBox>
                <VuiBox>
                  <VuiTypography variant="h6" color="white" fontWeight="bold">Legal</VuiTypography>
                  <VuiTypography variant="caption" color="text" display="block">© 2026 Root/Node Project. All rights reserved.</VuiTypography>
                </VuiBox>
              </VuiBox>

              <VuiTypography variant="caption" color="text">This software is provided "as is", without warranty of any kind.</VuiTypography>
            </Card>
          </Grid>
        </Grid>
        
      </VuiBox>
      <Footer />
    </DashboardLayout>
  );
}

export default About;

