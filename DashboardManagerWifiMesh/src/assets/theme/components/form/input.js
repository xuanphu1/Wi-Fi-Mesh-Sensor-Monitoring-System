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
import borders from "assets/theme/base/borders";

// Vision UI Dashboard  helper functions
import pxToRem from "assets/theme/functions/pxToRem";

const { inputColors } = colors;
const { borderWidth, borderRadius } = borders;

export default {
  styleOverrides: {
    root: {
      border: `1px solid rgba(255, 255, 255, 0.10)`,
      borderRadius: "14px !important",
      color: "#ffffff !important",

      "& fieldset": {
        border: "none",
      },
    },

    input: {
      color: "#ffffff !important",
      "&::placeholder": {
        color: "rgba(255, 255, 255, 0.55) !important",
        opacity: 1,
      },
    },
  },
};
