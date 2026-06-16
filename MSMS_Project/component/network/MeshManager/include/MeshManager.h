#ifndef MESH_MANAGER_H
#define MESH_MANAGER_H

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#include "esp_mac.h"
#include "esp_bridge.h"
#include "esp_mesh_lite.h"
#include "DataManager.h"
#include "SensorRegistry.h"
#include <stdbool.h>

/**
 * @file MeshManager.h
 * @brief ESP Wi‑Fi Mesh Lite child node: join mesh, optional UDP JSON telemetry to root.
 *
 * UDP payload — JSON compact (schema v = MESH_UDP_JSON_SCHEMA trong DataManager.h):
 *   {"v":<schema>,"seq":<counter>,"n":<mesh_level>,"i":"<sta_ipv4>","t":"<rtc_iso>","ver":"<x.x.x>","err":["0xNN",...],"p":[[port,type,v0,v1,...], ...]}
 *
 * - v: phiên bản định dạng (int).
 * - seq: UDP telemetry packet counter, starts at 1 after boot.
 * - n: esp_mesh_lite_get_level() (int).
 * - i: IPv4 của interface STA mesh (esp_netif "WIFI_STA_DEF"), chuỗi dotted-quad; định danh node theo IP được cấp.
 * - t: thời gian cục bộ theo ISO 8601 "YYYY-MM-DDTHH:MM:SS" (RTC/system time).
 * - ver: version firmware từ DataManager.version[3] theo format "major.minor.patch".
 * - err: mảng tối đa 10 mã lỗi từ DataManager.error_code[] (hex string). MRS_OK không lưu.
 * - p: mỗi hàng [port, type, v0, ...]
 *      port: 1..NUM_PORTS (1-based).
 *      type: SensorType_t — 0 BME280, 1 MHZ14A, 2 PMS7003, 3 DHT22,
 *            4 AHT10 (đầy đủ: SensorTypes.h).
 *      v*: float theo thứ tự driver->read()/data_fl[], tối đa 5 phần tử/hàng.
 *
 * Mô tả đầy đủ + ví dụ: comment trước mesh_build_udp_telemetry_json() trong MeshManager.c
 */

struct DataManager_t;

typedef struct {
    uint32_t udp_rx_frames;
    uint32_t queued_frames;
    uint32_t dropped_frames;
} mesh_gateway_stats_t;

/**
 * @brief Start mesh-lite stack, bridge netifs, optional UDP client (child only), and mesh logging timer.
 *
 * @param data Application DataManager (sensor selection, meshIo, task handles TASK_MESH_LINK / TASK_MESH_DATA).
 * @param mesh_as_root MESH_ROLE_ROOT for root; MESH_ROLE_NODE for child node + UDP uplink.
 */
void MeshManager_StartMesh(struct DataManager_t *data, mesh_role_t mesh_as_root);

/**
 * @brief Reset internal mesh manager runtime state after network cleanup.
 *
 * Called before starting mesh again with another role (node/root).
 */
void MeshManager_ResetState(void);

/**
 * @brief Switch mesh role at runtime without full cleanup/re-init.
 *
 * @param role Target role.
 * @return true if switched/applied, false if mesh is not started.
 */
bool MeshManager_SwitchRole(mesh_role_t role);

/**
 * @brief Returns true when mesh manager has been started.
 */
bool MeshManager_IsStarted(void);

/**
 * @brief Number of stations currently associated to this node's mesh SoftAP.
 *
 * Root screen uses this as the connected child-node count.
 */
uint8_t MeshManager_GetConnectedNodeCount(void);

/**
 * @brief Get last 1-second root gateway RX statistics.
 *
 * udp_rx_frames: UDP frames received by root.
 * queued_frames: frames pushed into gateway_rx_queue.
 * dropped_frames: frames dropped because gateway_rx_queue was full.
 */
void MeshManager_GetGatewayStats(mesh_gateway_stats_t *out_stats);

/**
 * @brief Whether this node has joined the mesh (mesh level > 0).
 *
 * @return true if associated to mesh topology, false if isolated / level 0.
 */
bool MeshManager_IsConnected(void);

/**
 * @brief Get the mesh network throughput.
 *
 * @return Current throughput value.
 */
int mesh_manager_get_throughput(void);

#endif /* MESH_MANAGER_H */
