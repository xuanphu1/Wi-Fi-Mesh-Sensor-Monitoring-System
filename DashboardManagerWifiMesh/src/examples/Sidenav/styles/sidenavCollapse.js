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

function collapseItem(theme, ownerState) {
  const { transitions, breakpoints, borders, functions } = theme;
  const { active } = ownerState;

  const { borderRadius } = borders;
  const { pxToRem } = functions;

  return {
    background: active
      ? "linear-gradient(135deg, rgba(0, 117, 255, 0.40) 0%, rgba(0, 117, 255, 0.18) 100%)"
      : "transparent",
    border: active ? "1px solid rgba(0, 160, 255, 0.55)" : "1px solid transparent",
    backdropFilter: active ? "blur(14px)" : "none",
    color: "#ffffff",
    display: "flex",
    alignItems: "center",
    width: "100%",
    padding: `${pxToRem(10)} ${pxToRem(12)} ${pxToRem(10)} ${pxToRem(14)}`,
    margin: `0 ${pxToRem(14)}`,
    borderRadius: borderRadius.lg,
    cursor: "pointer",
    userSelect: "none",
    whiteSpace: "nowrap",
    boxShadow: active
      ? "0 4px 20px rgba(0, 117, 255, 0.35), inset 0 0 12px rgba(0, 117, 255, 0.12)"
      : "none",
    transition: transitions.create(["background", "border-color", "box-shadow"], {
      easing: transitions.easing.easeInOut,
      duration: transitions.duration.shorter,
    }),
    "&:hover": {
      background: active
        ? "linear-gradient(135deg, rgba(0, 117, 255, 0.50) 0%, rgba(0, 117, 255, 0.25) 100%)"
        : "rgba(255, 255, 255, 0.06)",
    },
  };
}

function collapseIconBox(theme, ownerState) {
  const { transitions, borders, functions } = theme;
  const { active } = ownerState;

  const { borderRadius } = borders;
  const { pxToRem } = functions;

  return {
    background: active
      ? "linear-gradient(135deg, #0075FF 0%, #00B2FE 100%)"
      : "rgba(255, 255, 255, 0.05)",
    border: active ? "1px solid rgba(255, 255, 255, 0.35)" : "1px solid rgba(255, 255, 255, 0.08)",
    minWidth: pxToRem(36),
    minHeight: pxToRem(36),
    borderRadius: borderRadius.button,
    display: "grid",
    placeItems: "center",
    boxShadow: active ? "0 0 16px rgba(0, 117, 255, 0.65), 0 2px 6px rgba(0, 0, 0, 0.3)" : "none",
    transition: transitions.create(["margin", "background", "border", "box-shadow"], {
      easing: transitions.easing.easeInOut,
      duration: transitions.duration.standard,
    }),

    "& svg, & svg g, & i, & span": {
      fill: active ? "#ffffff !important" : "rgba(255, 255, 255, 0.75) !important",
      color: active ? "#ffffff !important" : "rgba(255, 255, 255, 0.75) !important",
    },
  };
}

const collapseIcon = ({ palette: { white, gradients } }, { active }) => ({
  color: active ? white.main : gradients.dark.state,
});

function collapseText(theme, ownerState) {
  const { transitions, breakpoints, functions } = theme;
  const { miniSidenav, active } = ownerState;

  const { pxToRem } = functions;

  return {
    marginLeft: pxToRem(14),

    [breakpoints.up("xl")]: {
      opacity: miniSidenav || miniSidenav ? 0 : 1,
      maxWidth: miniSidenav || miniSidenav ? 0 : "100%",
      marginLeft: miniSidenav || miniSidenav ? 0 : pxToRem(14),
      transition: transitions.create(["opacity", "margin"], {
        easing: transitions.easing.easeInOut,
        duration: transitions.duration.standard,
      }),
    },

    "& span": {
      fontWeight: active ? 700 : 500,
      fontSize: "16px",
      lineHeight: 1.2,
      letterSpacing: "0.2px",
      color: active ? "#ffffff" : "rgba(255, 255, 255, 0.85)",
    },
  };
}

export { collapseItem, collapseIconBox, collapseIcon, collapseText };
