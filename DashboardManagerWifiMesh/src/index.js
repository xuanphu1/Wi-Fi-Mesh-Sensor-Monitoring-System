/*!

=========================================================
* Vision UI Free React - v1.0.0
=========================================================

* Product Page: https://www.creative-tim.com/product/vision-ui-free-react
* Copyright 2021 Creative Tim (https://www.creative-tim.com/)
* Licensed under MIT (https://github.com/creativetimofficial/vision-ui-free-react/blob/master LICENSE.md)

* Design and Coded by Simmmple & Creative Tim

=========================================================

* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the software.

*/

import React from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter } from "react-router-dom";
import App from "App";

// Vision UI Dashboard React Context Provider
import { VisionUIControllerProvider } from "context";
import { MeshRealtimeProvider } from "context/meshRealtime";

// Lấy node gốc trong public/index.html (id="root") để mount toàn bộ React app.
const rootElement = document.getElementById("root");

// React 18 API: tạo root mới để render cây component.
const root = createRoot(rootElement);

// Thứ tự bọc provider rất quan trọng:
// 1) BrowserRouter: cấp context route/url cho toàn app.
// 2) VisionUIControllerProvider: state giao diện chung (sidenav, layout, theme behavior...).
// 3) MeshRealtimeProvider: state realtime toàn cục (websocket, nodes, sensors, latency...).
// 4) App: component khung chính, render route và các màn hình.
root.render(
  <BrowserRouter>
    <VisionUIControllerProvider>
      <MeshRealtimeProvider>
        <App />
      </MeshRealtimeProvider>
    </VisionUIControllerProvider>
  </BrowserRouter>
);

