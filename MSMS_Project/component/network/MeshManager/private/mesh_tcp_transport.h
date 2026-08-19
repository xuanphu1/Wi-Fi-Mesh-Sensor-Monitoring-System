#ifndef MESH_TCP_TRANSPORT_H
#define MESH_TCP_TRANSPORT_H

#include "MeshManager.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "mesh_stream_parser.h"

#ifndef CONFIG_MESH_TCP_MAX_CLIENTS
#define CONFIG_MESH_TCP_MAX_CLIENTS 5
#endif

typedef struct {
  bool active;
  int fd;
  uint32_t peer_ip;
  char mac[18];
  uint32_t last_sequence;
  mesh_stream_parser_t parser;
} mesh_tcp_client_t;

typedef struct {
  DataManager_t *data;
  QueueHandle_t gateway_queue;
  EventGroupHandle_t events;
  TaskHandle_t task;

  volatile mesh_role_t desired_role;
  mesh_role_t active_role;
  volatile bool running;

  int node_socket;
  int listen_socket;
  mesh_tcp_client_t clients[CONFIG_MESH_TCP_MAX_CLIENTS];

  uint32_t sequence;
  uint32_t reconnect_backoff_ms;
  TickType_t next_connect_tick;
  TickType_t next_send_tick;

  mesh_gateway_stats_t counters;
  mesh_gateway_stats_t last_stats;
  TickType_t stats_tick;
} mesh_tcp_transport_t;

esp_err_t mesh_tcp_transport_start(mesh_tcp_transport_t *transport,
                                   DataManager_t *data,
                                   QueueHandle_t gateway_queue,
                                   mesh_role_t initial_role);
esp_err_t mesh_tcp_transport_stop(mesh_tcp_transport_t *transport,
                                  TickType_t timeout);
void mesh_tcp_transport_set_role(mesh_tcp_transport_t *transport,
                                 mesh_role_t role);
uint8_t mesh_tcp_transport_client_count(const mesh_tcp_transport_t *transport);
void mesh_tcp_transport_get_stats(const mesh_tcp_transport_t *transport,
                                  mesh_gateway_stats_t *stats);

#endif /* MESH_TCP_TRANSPORT_H */
