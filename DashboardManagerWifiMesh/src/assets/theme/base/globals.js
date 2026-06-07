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
import bgAdmin from "assets/images/body-background.png";

const { info, dark } = colors;
export default {
  html: {
    scrollBehavior: "smooth",
    background: dark.body,
    height: "100%",
  },
  body: {
    background: `url(${bgAdmin})`,
    backgroundSize: "100% auto",
    backgroundRepeat: "repeat-y",
    backgroundPosition: "center top",
    minHeight: "100vh",
    height: "auto",
    scrollbarWidth: "thin",
    scrollbarColor: "rgba(124, 143, 255, 0.68) rgba(17, 22, 58, 0.55)",
  },
  "#root": {
    minHeight: "100vh",
    height: "auto",
  },
  "*": {
    scrollbarWidth: "thin",
    scrollbarColor: "rgba(124, 143, 255, 0.68) rgba(17, 22, 58, 0.55)",
  },
  "*::-webkit-scrollbar": {
    width: "10px",
    height: "10px",
  },
  "*::-webkit-scrollbar-track": {
    background: "rgba(17, 22, 58, 0.55)",
    borderRadius: "999px",
  },
  "*::-webkit-scrollbar-thumb": {
    background: "linear-gradient(180deg, rgba(104, 122, 255, 0.95) 0%, rgba(72, 88, 224, 0.88) 100%)",
    borderRadius: "999px",
    border: "2px solid rgba(17, 22, 58, 0.45)",
  },
  "*::-webkit-scrollbar-thumb:hover": {
    background: "linear-gradient(180deg, rgba(129, 146, 255, 0.98) 0%, rgba(90, 108, 240, 0.94) 100%)",
  },
  "*::-webkit-scrollbar-corner": {
    background: "transparent",
  },
  "*, *::before, *::after": {
    margin: 0,
    padding: 0,
  },
  "a, a:link, a:visited": {
    textDecoration: "none !important",
  },
  "a.link, .link, a.link:link, .link:link, a.link:visited, .link:visited": {
    color: `${dark.main} !important`,
    transition: "color 150ms ease-in !important",
  },
  "a.link:hover, .link:hover, a.link:focus, .link:focus": {
    color: `${info.main} !important`,
  },
};
