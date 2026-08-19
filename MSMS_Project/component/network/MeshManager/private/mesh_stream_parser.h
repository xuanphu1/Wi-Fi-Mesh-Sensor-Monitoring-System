#ifndef MESH_STREAM_PARSER_H
#define MESH_STREAM_PARSER_H

#include "DataManager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*mesh_stream_frame_cb_t)(const uint8_t *frame, size_t length,
                                       void *context);

typedef struct {
  uint8_t buffer[MESH_TELEMETRY_FRAME_SIZE];
  size_t length;
  bool discarding_oversized_frame;
} mesh_stream_parser_t;

void mesh_stream_parser_init(mesh_stream_parser_t *parser);
bool mesh_stream_parser_push(mesh_stream_parser_t *parser,
                             const uint8_t *bytes, size_t length,
                             mesh_stream_frame_cb_t on_frame, void *context);

#endif /* MESH_STREAM_PARSER_H */
