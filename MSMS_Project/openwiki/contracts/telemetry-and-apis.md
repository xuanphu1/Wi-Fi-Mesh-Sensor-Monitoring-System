---
type: protocol reference
title: Telemetry, gateway, and public API contracts
description: JSON telemetry framing and the source-backed public APIs connecting sensors, mesh TCP, root queues, UART, and errors.
tags: [telemetry, json, api, uart, errors]
---

# Telemetry and public contracts

## Node telemetry wire format

`mesh_telemetry_build` generates compact JSON, then the node transport sends a final newline. The JSON itself has a maximum of `MESH_TELEMETRY_FRAME_SIZE` = **512 bytes**; delimiter is not retained by the root queue. Example (values illustrative, keys/shape source-backed):

```json
{"v":1,"seq":42,"n":2,"M":"AA:BB:CC:DD:EE:FF","t":"2025-01-01T12:00:00","ver":"0.0.1","err":["0x5001"],"p":[[1,1,25.100,1001.200,48.300]]}
```

| Key | Meaning |
|---|---|
| `v` | `MESH_TELEMETRY_JSON_SCHEMA`, currently 1 |
| `seq` | node-local sequence, incremented only after a complete successful TCP send |
| `n` | current Mesh-Lite level |
| `M` | Wi-Fi STA MAC, uppercase colon-separated; root uses it for reconnect deduplication |
| `t` | RTC timestamp where possible; fallback local time, otherwise epoch text |
| `ver` | `DataManager.version` as `major.minor.patch` |
| `err` | non-`MRS_OK` entries from the fixed 10-element `DataManager.error_code` history, rendered as hex strings |
| `p` | selected-port rows `[one_based_port, SensorType_t, value...]`; at most five float fields per driver |

Ports with `SENSOR_NONE`, missing drivers, or failed reads are absent. During a successful read the builder writes `DataManager.port_data[port]`. Bad registry/read conditions add error codes. The root extracts `M`, `seq`, and `n` with simple key search for bookkeeping; it does not validate JSON structure before queueing. `ProcessingDataMesh_ProcessFrame` is a separate JSON utility that computes a 10-second packet-loss window by MAC, but is not used in the gateway forwarding path.

## Framing

Each TCP record is `JSON + LF`. The parser also accepts `CRLF`, removes CR, ignores empty lines, and is independent per accepted client. It must be so because TCP preserves bytes but not message boundaries. A partial JSON stays buffered across receives; two frames in one receive cause two callbacks. A frame exceeding 512 bytes is discarded until its following LF, re-synchronizing the stream rather than treating later bytes as a new message.

## Gateway protocol

Root TX sends each queued JSON and exactly one `\n`. When root mode’s queue remains empty, it sends `No node\n` every 1000 ms. Gateway RX lines use CR or LF and case-insensitive comparison:

| Gateway input | Firmware result |
|---|---|
| `Connected` | refreshes root heartbeat timestamp |
| `Start Root` | if mesh is started, sends `Switching to root mode...\r\n`, requests root role, enables heartbeat watchdog; otherwise sends `Mesh not ready\r\n` |
| unknown | logs warning only |

After `Start Root`, if a heartbeat is absent for more than one second, UART RX requests node role. UART TX only reads MeshManager frames while InternetManager reports mesh mode and MeshManager role is root.

## Cross-boundary APIs

| API | Contract and consumer |
|---|---|
| `MeshManager_StartMesh(data, role)` / `ResetState()` | creates/deletes private queue and starts/stops mesh network plus transport; InternetManager uses it |
| `MeshManager_SwitchRole(role)` | validates root/node, asks network layer to change Mesh-Lite role, signals desired transport role; FunctionManager/UART route uses it indirectly |
| `MeshManager_ReceiveGatewayFrame(frame, timeout)` | root-only public dequeue; UartToGateWay is consumer |
| `MeshManager_GetGatewayStats` / `GetGatewayQueueUsage` | snapshots per-second counters / queue size; UI or UART logging consumers |
| `sensor_registry_get_driver(type)` | returns registry ABI entry; FunctionManager and telemetry builder consume it |
| `UartToGateWay_Init`, `Pause`, `Resume`, `Send` | gateway UART lifecycle/raw send; FunctionManager pauses it when initializing a UART sensor |
| `ErrorCodes_PushError(buffer, capacity, err)` | ignores success; shifts full history and appends error; all layers report through `DataManager.error_code` |

`mesh_gateway_frame_t` contains source IP, length, and up to 512 bytes. The private queue depth is 32. `xQueueSend(..., 0)` means full queues drop rather than block the select loop; `mesh_gateway_stats_t` counts RX, queued, dropped, TX, and reconnect events over the last statistics window. DataManager is deliberately only a status/data consumer: never add a socket, queue handle, parser or transport descriptor to it.

For lifecycle and capacity handling, read [Mesh TCP](../network/mesh-tcp.md); for sensor field producers, read [Sensors and drivers](../hardware/sensors-and-drivers.md).