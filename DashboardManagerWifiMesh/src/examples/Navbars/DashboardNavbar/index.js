/*!

=========================================================
* Vision UI Free React - v1.0.0
=========================================================

* Product Page: https://www.creative-tim.com/product/vision-ui-free-react
* Copyright 2021 Creative Tim (https://www.creative-tim.com/)
* Licensed under MIT (https://github.com/creativetimofficial/vision-ui-free-react/blob/master LICENSE.md)

* Design and Coded by Simmmple & Creative Tim

=========================================================

* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

*/

import { useState, useEffect } from "react";

// react-router components
import { useLocation } from "react-router-dom";

// prop-types is a library for typechecking of props.
import PropTypes from "prop-types";

// @material-ui core components
import AppBar from "@mui/material/AppBar";
import Toolbar from "@mui/material/Toolbar";
import IconButton from "@mui/material/IconButton";
import Menu from "@mui/material/Menu";
import Icon from "@mui/material/Icon";

// Vision UI Dashboard React components
import VuiBox from "components/VuiBox";
import VuiTypography from "components/VuiTypography";
import VuiInput from "components/VuiInput";

// Vision UI Dashboard React example components
import Breadcrumbs from "examples/Breadcrumbs";
import NotificationItem from "examples/Items/NotificationItem";

// Custom styles for DashboardNavbar
import {
  navbar,
  navbarContainer,
  navbarRow,
  navbarIconButton,
  navbarMobileMenu,
} from "examples/Navbars/DashboardNavbar/styles";

import FormControl from "@mui/material/FormControl";
import Select from "@mui/material/Select";
import MenuItem from "@mui/material/MenuItem";

// Vision UI Dashboard React context
import {
  useVisionUIController,
  setTransparentNavbar,
  setMiniSidenav,
} from "context";
import { useMeshRealtime } from "context/meshRealtime";

// Images
import team2 from "assets/images/team-2.jpg";
import logoSpotify from "assets/images/small-logos/logo-spotify.svg";

function DashboardNavbar({ absolute, light, isMini }) {
  const [navbarType, setNavbarType] = useState();
  const [controller, dispatch] = useVisionUIController();
  const { miniSidenav, transparentNavbar, fixedNavbar } = controller;
  const [openMenu, setOpenMenu] = useState(false);
  const route = useLocation().pathname.split("/").slice(1);
  const { gateways = [], selectedGatewayId = "all", setSelectedGatewayId } = useMeshRealtime() || {};

  useEffect(() => {
    // Setting the navbar type
    if (fixedNavbar) {
      setNavbarType("sticky");
    } else {
      setNavbarType("static");
    }

    // A function that sets the transparent state of the navbar.
    function handleTransparentNavbar() {
      setTransparentNavbar(dispatch, (fixedNavbar && window.scrollY === 0) || !fixedNavbar);
    }

    /** 
     The event listener that's calling the handleTransparentNavbar function when 
     scrolling the window.
    */
    window.addEventListener("scroll", handleTransparentNavbar);

    // Call the handleTransparentNavbar function to set the state with the initial value.
    handleTransparentNavbar();

    // Remove event listener on cleanup
    return () => window.removeEventListener("scroll", handleTransparentNavbar);
  }, [dispatch, fixedNavbar]);

  const handleMiniSidenav = () => setMiniSidenav(dispatch, !miniSidenav);
  const handleOpenMenu = (event) => setOpenMenu(event.currentTarget);
  const handleCloseMenu = () => setOpenMenu(false);

  // Render the notifications menu
  const renderMenu = () => (
    <Menu
      anchorEl={openMenu}
      anchorReference={null}
      anchorOrigin={{
        vertical: "bottom",
        horizontal: "left",
      }}
      open={Boolean(openMenu)}
      onClose={handleCloseMenu}
      sx={{ mt: 2 }}
    >
      <NotificationItem
        image={<img src={team2} alt="person" />}
        title={["New message", "from Laur"]}
        date="13 minutes ago"
        onClick={handleCloseMenu}
      />
      <NotificationItem
        image={<img src={logoSpotify} alt="person" />}
        title={["New album", "by Travis Scott"]}
        date="1 day"
        onClick={handleCloseMenu}
      />
      <NotificationItem
        color="text"
        image={
          <Icon fontSize="small" sx={{ color: ({ palette: { white } }) => white.main }}>
            payment
          </Icon>
        }
        title={["", "Payment successfully completed"]}
        date="2 days"
        onClick={handleCloseMenu}
      />
    </Menu>
  );

  return (
    <AppBar
      position={absolute ? "absolute" : navbarType}
      color="inherit"
      sx={(theme) => navbar(theme, { transparentNavbar, absolute, light })}
    >
      <Toolbar sx={(theme) => navbarContainer(theme)}>
        <VuiBox color="inherit" mb={{ xs: 1, md: 0 }} sx={(theme) => navbarRow(theme, { isMini })}>
          <Breadcrumbs icon="home" title={route[route.length - 1]} route={route} light={light} />
        </VuiBox>
        {isMini ? null : (
          <VuiBox sx={(theme) => navbarRow(theme, { isMini })} display="flex" alignItems="center">
            {/* Global Multi-Gateway / Mesh Selector */}
            {setSelectedGatewayId && (
              <VuiBox pr={{ xs: 1, md: 2 }}>
                <FormControl size="small" sx={{ minWidth: { xs: 150, sm: 240, md: 280 } }}>
                  <Select
                    value={selectedGatewayId}
                    onChange={(e) => setSelectedGatewayId(e.target.value)}
                    displayEmpty
                    sx={{
                      height: "38px",
                      color: "#ffffff !important",
                      background: "linear-gradient(127deg, rgba(6, 11, 40, 0.6) 0%, rgba(10, 14, 35, 0.45) 100%)",
                      border: "1px solid rgba(0, 212, 255, 0.35) !important",
                      borderRadius: "12px",
                      fontSize: "12px",
                      fontWeight: "bold",
                      "& .MuiSvgIcon-root": { color: "#00d4ff" },
                      "& .MuiOutlinedInput-notchedOutline": { border: "none" },
                    }}
                    MenuProps={{
                      PaperProps: {
                        sx: {
                          background: "rgba(10, 14, 35, 0.95)",
                          backdropFilter: "blur(20px)",
                          border: "1px solid rgba(255, 255, 255, 0.15)",
                          borderRadius: "14px",
                          color: "#fff",
                          mt: 1,
                          boxShadow: "0 12px 36px rgba(0,0,0,0.6)",
                          "& .MuiMenuItem-root": {
                            fontSize: "12px",
                            "&:hover": { background: "rgba(0, 117, 255, 0.25)" },
                            "&.Mui-selected": { background: "rgba(0, 212, 255, 0.25)" },
                          },
                        },
                      },
                    }}
                  >
                    <MenuItem value="all">
                      <span style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                        <span>🌐</span>
                        <span style={{ fontWeight: "bold" }}>Tất cả Gateway (Toàn hệ thống)</span>
                      </span>
                    </MenuItem>
                    {gateways.map((gw) => (
                      <MenuItem key={gw.gw_id} value={gw.gw_id}>
                        <span style={{ display: "flex", alignItems: "center", gap: "8px", width: "100%" }}>
                          <span
                            style={{
                              width: 8,
                              height: 8,
                              borderRadius: "50%",
                              backgroundColor: gw.status === "online" ? "#01f7a7" : "#ff5252",
                              boxShadow: gw.status === "online" ? "0 0 6px #01f7a7" : "none",
                              flexShrink: 0,
                            }}
                          />
                          <span style={{ fontWeight: "bold" }}>{gw.name || gw.gw_id}</span>
                          {gw.location && (
                            <span style={{ opacity: 0.6, fontSize: "11px" }}>({gw.location})</span>
                          )}
                          <span
                            style={{
                              marginLeft: "auto",
                              fontSize: "10px",
                              padding: "2px 6px",
                              borderRadius: "6px",
                              background: "rgba(0, 212, 255, 0.15)",
                              color: "#00d4ff",
                            }}
                          >
                            {gw.node_count || 0} nodes
                          </span>
                        </span>
                      </MenuItem>
                    ))}
                  </Select>
                </FormControl>
              </VuiBox>
            )}

            <VuiBox pr={1}>
              <VuiInput
                placeholder="Type here..."
                icon={{ component: "search", direction: "left" }}
                sx={({ breakpoints }) => ({
                  [breakpoints.down("sm")]: {
                    maxWidth: "80px",
                  },
                  [breakpoints.only("sm")]: {
                    maxWidth: "80px",
                  },
                  backgroundColor: "rgba(255, 255, 255, 0.06) !important",
                  border: "1px solid rgba(255, 255, 255, 0.12) !important",
                  backdropFilter: "blur(16px)",
                  borderRadius: "12px !important",
                })}
              />
            </VuiBox>
            <VuiBox color={light ? "white" : "inherit"}>
              <IconButton
                size="small"
                color="inherit"
                sx={navbarMobileMenu}
                onClick={handleMiniSidenav}
              >
                <Icon className={"text-white"}>{miniSidenav ? "menu_open" : "menu"}</Icon>
              </IconButton>

              <IconButton
                size="small"
                color="inherit"
                sx={navbarIconButton}
                aria-controls="notification-menu"
                aria-haspopup="true"
                variant="contained"
                onClick={handleOpenMenu}
              >
                <Icon className={light ? "text-white" : "text-dark"}>notifications</Icon>
              </IconButton>
              {renderMenu()}
            </VuiBox>
          </VuiBox>
        )}
      </Toolbar>
    </AppBar>
  );
}

// Setting default values for the props of DashboardNavbar
DashboardNavbar.defaultProps = {
  absolute: false,
  light: false,
  isMini: false,
};

// Typechecking props for the DashboardNavbar
DashboardNavbar.propTypes = {
  absolute: PropTypes.bool,
  light: PropTypes.bool,
  isMini: PropTypes.bool,
};

export default DashboardNavbar;
