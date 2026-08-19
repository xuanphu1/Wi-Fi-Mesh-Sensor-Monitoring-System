#include "mesh_tcp_transport.h"

#include "ErrorCodes.h"
#include "esp_log.h"
#include "esp_mesh_lite.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "mesh_telemetry.h"
#include "sdkconfig.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <lwip/inet.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef CONFIG_MESH_TCP_PORT
#define CONFIG_MESH_TCP_PORT 8070
#endif
#ifndef CONFIG_MESH_TCP_SEND_INTERVAL_MS
#define CONFIG_MESH_TCP_SEND_INTERVAL_MS 1000
#endif
#ifndef CONFIG_MESH_TCP_CONNECT_TIMEOUT_MS
#define CONFIG_MESH_TCP_CONNECT_TIMEOUT_MS 5000
#endif
#ifndef CONFIG_MESH_ROOT_FALLBACK_IP
#define CONFIG_MESH_ROOT_FALLBACK_IP "192.168.5.1"
#endif

#define TRANSPORT_EVENT_STOP BIT0
#define TRANSPORT_EVENT_WAKE BIT1
#define TRANSPORT_EVENT_STOPPED BIT2
#define TRANSPORT_POLL_MS 200
#define RECONNECT_MIN_MS 1000
#define RECONNECT_MAX_MS 10000

static const char *TAG = "MeshTCP";

typedef struct {
  mesh_tcp_transport_t *transport;
  int client_index;
} frame_callback_context_t;

static void close_socket(int *fd) {
  if (fd == NULL || *fd < 0) {
    return;
  }
  shutdown(*fd, SHUT_RDWR);
  close(*fd);
  *fd = -1;
}

static void close_client(mesh_tcp_client_t *client) {
  if (client == NULL) {
    return;
  }
  close_socket(&client->fd);
  memset(client, 0, sizeof(*client));
  client->fd = -1;
}

static void close_all_sockets(mesh_tcp_transport_t *transport) {
  close_socket(&transport->node_socket);
  close_socket(&transport->listen_socket);
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    close_client(&transport->clients[i]);
  }
}

static void roll_stats(mesh_tcp_transport_t *transport) {
  TickType_t now = xTaskGetTickCount();
  if ((now - transport->stats_tick) < pdMS_TO_TICKS(1000)) {
    return;
  }
  transport->last_stats = transport->counters;
  memset(&transport->counters, 0, sizeof(transport->counters));
  transport->stats_tick = now;
}

static bool send_all(int socket_fd, const uint8_t *data, size_t length) {
  while (length > 0) {
    int sent = send(socket_fd, data, length, 0);
    if (sent > 0) {
      data += sent;
      length -= (size_t)sent;
      continue;
    }
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

static bool resolve_root_address(struct sockaddr_in *address,
                                 char *text, size_t text_capacity) {
  if (address == NULL) {
    return false;
  }
  memset(address, 0, sizeof(*address));
  address->sin_family = AF_INET;
  address->sin_port = htons(CONFIG_MESH_TCP_PORT);

  esp_ip_addr_t root_ip = {0};
  if (esp_mesh_lite_get_root_ip(IPADDR_TYPE_V4, &root_ip) == ESP_OK &&
      root_ip.u_addr.ip4.addr != 0) {
    /* Preserve the byte-order convention verified by the standalone test. */
    address->sin_addr.s_addr = htonl(root_ip.u_addr.ip4.addr);
  } else {
    esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info = {0};
    if (station != NULL && esp_netif_get_ip_info(station, &info) == ESP_OK &&
        info.gw.addr != 0) {
      address->sin_addr.s_addr = htonl(info.gw.addr);
    } else {
      address->sin_addr.s_addr = inet_addr(CONFIG_MESH_ROOT_FALLBACK_IP);
    }
  }

  if (address->sin_addr.s_addr == INADDR_NONE ||
      address->sin_addr.s_addr == 0) {
    return false;
  }
  if (text != NULL && text_capacity > 0) {
    inet_ntoa_r(address->sin_addr, text, (int)text_capacity);
  }
  return true;
}

static int connect_node_socket(void) {
  struct sockaddr_in address;
  char root_text[16] = {0};
  if (!resolve_root_address(&address, root_text, sizeof(root_text))) {
    return -1;
  }

  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (socket_fd < 0) {
    return -1;
  }

  esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (station != NULL) {
    struct ifreq interface = {0};
    if (esp_netif_get_netif_impl_name(station, interface.ifr_name) == ESP_OK) {
      setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &interface,
                 sizeof(interface));
    }
  }

  struct timeval timeout = {
      .tv_sec = CONFIG_MESH_TCP_CONNECT_TIMEOUT_MS / 1000,
      .tv_usec = (CONFIG_MESH_TCP_CONNECT_TIMEOUT_MS % 1000) * 1000,
  };
  setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  int keepalive = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
             sizeof(keepalive));

  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
  }

  int result = connect(socket_fd, (struct sockaddr *)&address, sizeof(address));
  if (result != 0 && errno == EINPROGRESS) {
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_fd, &write_set);
    result = select(socket_fd + 1, NULL, &write_set, NULL, &timeout);
    if (result > 0) {
      int socket_error = 0;
      socklen_t error_length = sizeof(socket_error);
      if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error,
                     &error_length) != 0 ||
          socket_error != 0) {
        result = -1;
        errno = socket_error;
      } else {
        result = 0;
      }
    } else {
      result = -1;
    }
  }

  if (flags >= 0) {
    fcntl(socket_fd, F_SETFL, flags);
  }
  if (result != 0) {
    ESP_LOGW(TAG, "Connect %s:%d failed: errno=%d", root_text,
             CONFIG_MESH_TCP_PORT, errno);
    close_socket(&socket_fd);
    return -1;
  }

  ESP_LOGI(TAG, "Connected to root %s:%d", root_text, CONFIG_MESH_TCP_PORT);
  return socket_fd;
}

static void schedule_reconnect(mesh_tcp_transport_t *transport) {
  uint32_t jitter = esp_random() % 250;
  transport->next_connect_tick =
      xTaskGetTickCount() +
      pdMS_TO_TICKS(transport->reconnect_backoff_ms + jitter);
  if (transport->reconnect_backoff_ms < RECONNECT_MAX_MS) {
    transport->reconnect_backoff_ms *= 2;
    if (transport->reconnect_backoff_ms > RECONNECT_MAX_MS) {
      transport->reconnect_backoff_ms = RECONNECT_MAX_MS;
    }
  }
  transport->counters.reconnects++;
}

static void run_node(mesh_tcp_transport_t *transport) {
  if (esp_mesh_lite_get_level() <= 1) {
    close_socket(&transport->node_socket);
    vTaskDelay(pdMS_TO_TICKS(TRANSPORT_POLL_MS));
    return;
  }

  TickType_t now = xTaskGetTickCount();
  if (transport->node_socket < 0) {
    if ((int32_t)(now - transport->next_connect_tick) < 0) {
      vTaskDelay(pdMS_TO_TICKS(TRANSPORT_POLL_MS));
      return;
    }
    transport->node_socket = connect_node_socket();
    if (transport->node_socket < 0) {
      schedule_reconnect(transport);
      vTaskDelay(pdMS_TO_TICKS(TRANSPORT_POLL_MS));
      return;
    }
    transport->reconnect_backoff_ms = RECONNECT_MIN_MS;
    transport->next_send_tick = 0;
  }

  if ((int32_t)(now - transport->next_send_tick) < 0) {
    vTaskDelay(pdMS_TO_TICKS(TRANSPORT_POLL_MS));
    return;
  }

  char frame[MESH_TELEMETRY_FRAME_SIZE + 1];
  uint32_t sequence = transport->sequence + 1;
  int frame_length = mesh_telemetry_build(frame, sizeof(frame), transport->data,
                                          sequence);
  if (frame_length <= 0) {
    ErrorCodes_PushError(transport->data->error_code,
                         DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
    transport->next_send_tick = now + pdMS_TO_TICKS(CONFIG_MESH_TCP_SEND_INTERVAL_MS);
    return;
  }

  if (!send_all(transport->node_socket, (const uint8_t *)frame,
                (size_t)frame_length) ||
      !send_all(transport->node_socket, (const uint8_t *)"\n", 1)) {
    ESP_LOGW(TAG, "TCP connection lost: errno=%d", errno);
    close_socket(&transport->node_socket);
    schedule_reconnect(transport);
    return;
  }

  transport->sequence = sequence;
  transport->counters.tx_frames++;
  transport->next_send_tick =
      now + pdMS_TO_TICKS(CONFIG_MESH_TCP_SEND_INTERVAL_MS);
}

static int open_server_socket(void) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (socket_fd < 0) {
    return -1;
  }
  int reuse = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(CONFIG_MESH_TCP_PORT),
      .sin_addr.s_addr = htonl(INADDR_ANY),
  };
  if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
      listen(socket_fd, CONFIG_MESH_TCP_MAX_CLIENTS) != 0) {
    ESP_LOGE(TAG, "Open server failed: errno=%d", errno);
    close_socket(&socket_fd);
    return -1;
  }
  ESP_LOGI(TAG, "Root listening on TCP port %d", CONFIG_MESH_TCP_PORT);
  return socket_fd;
}

static int find_free_client(mesh_tcp_transport_t *transport) {
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    if (!transport->clients[i].active) {
      return i;
    }
  }
  return -1;
}

static void accept_client(mesh_tcp_transport_t *transport) {
  struct sockaddr_in peer = {0};
  socklen_t peer_length = sizeof(peer);
  int client_fd = accept(transport->listen_socket, (struct sockaddr *)&peer,
                         &peer_length);
  if (client_fd < 0) {
    return;
  }
  if (client_fd >= FD_SETSIZE) {
    ESP_LOGW(TAG, "Client fd exceeds FD_SETSIZE");
    close_socket(&client_fd);
    return;
  }

  int slot = find_free_client(transport);
  if (slot < 0) {
    ESP_LOGW(TAG, "TCP client limit reached");
    close_socket(&client_fd);
    return;
  }

  int keepalive = 1;
  setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
             sizeof(keepalive));
  mesh_tcp_client_t *client = &transport->clients[slot];
  memset(client, 0, sizeof(*client));
  client->active = true;
  client->fd = client_fd;
  client->peer_ip = peer.sin_addr.s_addr;
  mesh_stream_parser_init(&client->parser);
  ESP_LOGI(TAG, "Accepted TCP node in slot %d", slot);
}

static void deduplicate_client(mesh_tcp_transport_t *transport,
                               int current_index, const char *mac) {
  if (mac == NULL || mac[0] == '\0') {
    return;
  }
  mesh_tcp_client_t *current = &transport->clients[current_index];
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    mesh_tcp_client_t *other = &transport->clients[i];
    if (i == current_index || !other->active || other->mac[0] == '\0' ||
        strcmp(other->mac, mac) != 0) {
      continue;
    }
    ESP_LOGI(TAG, "Replace old connection for node %s", mac);
    close_client(other);
  }
  strlcpy(current->mac, mac, sizeof(current->mac));
}

static bool enqueue_frame(const uint8_t *frame, size_t length, void *context) {
  frame_callback_context_t *callback = context;
  mesh_tcp_transport_t *transport = callback->transport;
  mesh_tcp_client_t *client = &transport->clients[callback->client_index];
  if (!client->active || length == 0 ||
      length > MESH_TELEMETRY_FRAME_SIZE) {
    return false;
  }

  char mac[18] = {0};
  uint32_t sequence = 0;
  int level = -1;
  mesh_telemetry_extract_origin(frame, length, mac, sizeof(mac), &sequence,
                                &level);
  deduplicate_client(transport, callback->client_index, mac);
  if (!client->active) {
    return false;
  }
  client->last_sequence = sequence;

  mesh_gateway_frame_t message = {
      .src_ip = client->peer_ip,
      .len = (uint16_t)length,
  };
  memcpy(message.data, frame, length);
  transport->counters.rx_frames++;
  bool queued = xQueueSend(transport->gateway_queue, &message, 0) == pdPASS;
  if (queued) {
    transport->counters.queued_frames++;
  } else {
    transport->counters.dropped_frames++;
  }
  ESP_LOGD(TAG, "RX node=%s level=%d seq=%lu queued=%d", mac, level,
           (unsigned long)sequence, queued);
  return queued;
}

static void receive_client(mesh_tcp_transport_t *transport, int index) {
  mesh_tcp_client_t *client = &transport->clients[index];
  uint8_t bytes[256];
  int received = recv(client->fd, bytes, sizeof(bytes), 0);
  if (received <= 0) {
    ESP_LOGI(TAG, "TCP node slot %d disconnected", index);
    close_client(client);
    return;
  }

  frame_callback_context_t callback = {
      .transport = transport,
      .client_index = index,
  };
  if (!mesh_stream_parser_push(&client->parser, bytes, (size_t)received,
                               enqueue_frame, &callback)) {
    if (client->parser.discarding_oversized_frame) {
      ESP_LOGW(TAG, "Oversized TCP frame from slot %d", index);
      transport->counters.dropped_frames++;
    }
  }
}

static void run_root(mesh_tcp_transport_t *transport) {
  if (transport->listen_socket < 0) {
    transport->listen_socket = open_server_socket();
    if (transport->listen_socket < 0) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      return;
    }
  }

  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(transport->listen_socket, &read_set);
  int max_fd = transport->listen_socket;
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    int fd = transport->clients[i].fd;
    if (transport->clients[i].active && fd >= 0) {
      FD_SET(fd, &read_set);
      if (fd > max_fd) {
        max_fd = fd;
      }
    }
  }

  struct timeval timeout = {.tv_sec = 0, .tv_usec = TRANSPORT_POLL_MS * 1000};
  int ready = select(max_fd + 1, &read_set, NULL, NULL, &timeout);
  if (ready < 0) {
    if (errno != EINTR) {
      ESP_LOGW(TAG, "select failed: errno=%d", errno);
      close_all_sockets(transport);
    }
    return;
  }
  if (ready == 0) {
    return;
  }
  if (FD_ISSET(transport->listen_socket, &read_set)) {
    accept_client(transport);
  }
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    mesh_tcp_client_t *client = &transport->clients[i];
    if (client->active && client->fd >= 0 && FD_ISSET(client->fd, &read_set)) {
      receive_client(transport, i);
    }
  }
}

static void transport_task(void *parameter) {
  mesh_tcp_transport_t *transport = parameter;
  transport->active_role = transport->desired_role;

  while ((xEventGroupGetBits(transport->events) & TRANSPORT_EVENT_STOP) == 0) {
    if (transport->active_role != transport->desired_role) {
      close_all_sockets(transport);
      transport->active_role = transport->desired_role;
      transport->reconnect_backoff_ms = RECONNECT_MIN_MS;
      transport->next_connect_tick = 0;
      ESP_LOGI(TAG, "Transport role is now %s",
               transport->active_role == MESH_ROLE_ROOT ? "root" : "node");
    }

    bool link_up = esp_mesh_lite_get_level() > 0;
    transport->data->meshIo.link_up = link_up;
    if (transport->active_role == MESH_ROLE_ROOT) {
      run_root(transport);
    } else {
      run_node(transport);
    }
    roll_stats(transport);
  }

  close_all_sockets(transport);
  transport->data->meshIo.link_up = false;
  transport->running = false;
  transport->task = NULL;
  xEventGroupSetBits(transport->events, TRANSPORT_EVENT_STOPPED);
  vTaskDelete(NULL);
}

esp_err_t mesh_tcp_transport_start(mesh_tcp_transport_t *transport,
                                   DataManager_t *data,
                                   QueueHandle_t gateway_queue,
                                   mesh_role_t initial_role) {
  if (transport == NULL || data == NULL || gateway_queue == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(transport, 0, sizeof(*transport));
  transport->data = data;
  transport->gateway_queue = gateway_queue;
  transport->desired_role = initial_role;
  transport->active_role = initial_role;
  transport->node_socket = -1;
  transport->listen_socket = -1;
  transport->reconnect_backoff_ms = RECONNECT_MIN_MS;
  transport->stats_tick = xTaskGetTickCount();
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    transport->clients[i].fd = -1;
  }

  transport->events = xEventGroupCreate();
  if (transport->events == NULL) {
    return ESP_ERR_NO_MEM;
  }
  transport->running = true;
  if (xTaskCreate(transport_task, "mesh_tcp", 6144, transport, 5,
                  &transport->task) != pdPASS) {
    transport->running = false;
    vEventGroupDelete(transport->events);
    transport->events = NULL;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

esp_err_t mesh_tcp_transport_stop(mesh_tcp_transport_t *transport,
                                  TickType_t timeout) {
  if (transport == NULL || transport->events == NULL) {
    return ESP_OK;
  }
  if (transport->running) {
    xEventGroupSetBits(transport->events,
                       TRANSPORT_EVENT_STOP | TRANSPORT_EVENT_WAKE);
    EventBits_t result = xEventGroupWaitBits(
        transport->events, TRANSPORT_EVENT_STOPPED, pdFALSE, pdTRUE, timeout);
    if ((result & TRANSPORT_EVENT_STOPPED) == 0) {
      return ESP_ERR_TIMEOUT;
    }
  }
  vEventGroupDelete(transport->events);
  transport->events = NULL;
  return ESP_OK;
}

void mesh_tcp_transport_set_role(mesh_tcp_transport_t *transport,
                                 mesh_role_t role) {
  if (transport == NULL) {
    return;
  }
  transport->desired_role = role;
  if (transport->events != NULL) {
    xEventGroupSetBits(transport->events, TRANSPORT_EVENT_WAKE);
  }
}

uint8_t mesh_tcp_transport_client_count(const mesh_tcp_transport_t *transport) {
  if (transport == NULL) {
    return 0;
  }
  uint8_t count = 0;
  for (int i = 0; i < CONFIG_MESH_TCP_MAX_CLIENTS; i++) {
    if (transport->clients[i].active) {
      count++;
    }
  }
  return count;
}

void mesh_tcp_transport_get_stats(const mesh_tcp_transport_t *transport,
                                  mesh_gateway_stats_t *stats) {
  if (stats == NULL) {
    return;
  }
  if (transport == NULL) {
    memset(stats, 0, sizeof(*stats));
    return;
  }
  *stats = transport->last_stats;
}
