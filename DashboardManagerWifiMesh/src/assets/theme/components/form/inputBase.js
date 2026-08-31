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
import typography from "assets/theme/base/typography";
import borders from "assets/theme/base/borders";

// Vision UI Dashboard  helper functions
import pxToRem from "assets/theme/functions/pxToRem";

const { dark, white, grey, inputColors } = colors;
const { size, fontWeightRegular } = typography;
const { borderWidth, borderRadius } = borders;

export default {
  styleOverrides: {
    root: {
      width: "100% !important",
      height: "auto !important",
      fontSize: `${size.sm} !important`,
      fontWeight: `${fontWeightRegular} !important`,
      lineHeight: "1.4 !important",
      color: "#ffffff !important",
      backgroundColor: "linear-gradient(127deg, rgba(6, 11, 40, 0.28) 0%, rgba(10, 14, 35, 0.18) 100%)",
      backdropFilter: "blur(18px)",
      border: `1px solid rgba(255, 255, 255, 0.10)`,
      borderRadius: "14px",
      boxShadow: "0 8px 32px rgba(0, 0, 0, 0.35)",
    },

    input: {
      width: "100% !important",
      color: "#ffffff !important",
      "&::placeholder": {
        color: "rgba(255, 255, 255, 0.55) !important",
        opacity: 1,
      },
    },
  },
};
