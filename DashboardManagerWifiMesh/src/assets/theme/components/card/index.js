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

// Vision UI Dashboard React Base Styles
import colors from "assets/theme/base/colors";
import linearGradient from "assets/theme/functions/linearGradient";
import borders from "assets/theme/base/borders";
import boxShadows from "assets/theme/base/boxShadows";

// Vision UI Dashboard React Helper Function
import rgba from "assets/theme/functions/rgba";

const { black, gradients } = colors;
const { card } = gradients;
const { borderWidth, borderRadius } = borders;
const { xxl } = boxShadows;

export default {
  styleOverrides: {
    root: {
      display: "flex",
      flexDirection: "column",
      background: "linear-gradient(127deg, rgba(6, 11, 40, 0.32) 0%, rgba(10, 14, 35, 0.20) 100%)",
      backdropFilter: "blur(20px)",
      position: "relative",
      minWidth: 0,
      padding: "22px",
      wordWrap: "break-word",
      backgroundClip: "border-box",
      border: "1px solid rgba(255, 255, 255, 0.08)",
      borderRadius: borderRadius.xl,
      boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    },
  },
};
