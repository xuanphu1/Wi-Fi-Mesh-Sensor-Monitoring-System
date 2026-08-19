---
type: application subsystem
title: Shared application state and control services
description: DataManager boundaries, FunctionManager actions, and shared error utility contracts.
tags: [core, state, sensors, errors, control]
---

# Application state and control

`DataManager_t` is the process-wide application model declared in `component/core/DataManager/DataManager.h`; `main/main.h` provides the static instance. It contains generic/latest and per-port sensor data, selected sensors, UI/button/menu state, display-facing battery/Wi-Fi/mesh info, callback hooks, task handles, version, and a 10-entry error history. It exposes no socket or queue ownership. `meshIo` only carries volatile `link_up` and role.

`FunctionManager` implements menu actions. `wifi_config_callback` changes to Wi-Fi mode; root/node callbacks allocate `MeshJoinTaskArg_t` and switch mode/role; root mode starts a display task. Sensor selection validates port/driver, pauses gateway UART for a UART sensor, initializes its registry wrapper, commits selection only on success, and starts a one-second read task. Reset deinitializes selected drivers, clears ports/tasks, and resumes gateway UART when applicable. Actuator callbacks configure GPIO and set its level. All failures are logged and commonly appended with `ErrorCodes_PushError`.

`ErrorCodes.h` defines `system_err_t` as `esp_err_t`, module-encoded custom errors, conversion helpers, and the append/shift behavior of `ErrorCodes_PushError`. This is an error history, not a synchronized event queue. `BitManager` is a small utility component; consult the [File atlas](../reference/file-atlas.md) for its exact file API.

**Change rule:** modify shared UI/sensor/status fields here only when every writer and reader remains safe. Add network transport state to MeshManager instead; see [Mesh TCP](../network/mesh-tcp.md).