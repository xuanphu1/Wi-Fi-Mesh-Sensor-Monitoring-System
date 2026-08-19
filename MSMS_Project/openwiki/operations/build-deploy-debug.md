---
type: operations guide
title: Build, flash, monitor, and integration validation
description: ESP-IDF build/deployment commands and source-grounded root/node validation and failure diagnosis.
tags: [build, flash, monitor, validation, debugging]
---

# Build, deploy, and debug

## Build and deploy

Activate the project ESP-IDF environment with the required local command, then build:

```sh
esp
idf.py build
```

The configured target is `esp32`; the artifact follows `project(MRS_Project)`, so expect `build/MRS_Project.bin`. To flash and monitor, use the ESP-IDF commands appropriate to the connected serial device:

```sh
idf.py -p PORT flash
idf.py -p PORT monitor
```

Do not change target based only on old README material. Review [Configuration](configuration-and-persistence.md) before changing sdkconfig/Kconfig, particularly socket limits and pin assignments.

## Root and multi-node hardware validation

1. Flash one device, use gateway input `Start Root`, and ensure its logs report root listener TCP port 8070.
2. Flash one or more devices with default mesh startup. Wait for node mesh level greater than 1 and `Connected to root` log.
3. Configure selected sensors from OLED menus; verify root gateway UART receives one newline-terminated JSON record per node interval.
4. Restart/reconnect a node and verify root logs replacement by MAC rather than treating changed peer IP as a new durable identity.
5. Observe `No node` output only when root queue is empty; send `Connected` at least each second after root command to avoid deliberate fallback to node.

No first-party test suites were found in `main` or `component`, so narrow validation is build plus the integration exercises above. Run a clean rebuild after build/config changes; multi-device testing is required for TCP/root changes.

## Failure behavior

| Situation | Source behavior | Investigate |
|---|---|---|
| mesh parent lost | node sees level at most 1, closes socket, polls | Mesh-Lite topology / RSSI |
| root IP changes | next connection resolves Mesh-Lite root, then STA gateway, then fallback | root IP API and fallback config |
| TCP disconnect/connect fail | node closes fd, exponential 1–10 s retry plus 0–249 ms jitter | root listener and socket capacity |
| partial/multiple frames | per-client parser buffers/splits at newline | never assume recv boundary |
| oversized frame | discard through next LF and count dropped | producer must remain at 512 bytes |
| queue full | non-blocking enqueue drops frame | gateway throughput and queue usage |
| client limit/socket exhaustion | accept closes excess; listener plus clients count against LwIP total | `CONFIG_MESH_TCP_MAX_CLIENTS`, `CONFIG_LWIP_MAX_SOCKETS` |
| gateway silent | root watchdog returns to node after >1 s heartbeat absence | UART wiring/peer sends `Connected` |
| UART RX overflow | input flush and event queue reset | baud/flow/data rate |

The root TCP select loop and all descriptor close paths are single-task by design; do not “fix” a failure by adding external `close()` calls. See [Mesh TCP](../network/mesh-tcp.md).