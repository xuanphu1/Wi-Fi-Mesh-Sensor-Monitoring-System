#include "mesh_stream_parser.h"

#include <string.h>

void mesh_stream_parser_init(mesh_stream_parser_t *parser) {
  if (parser != NULL) {
    memset(parser, 0, sizeof(*parser));
  }
}

bool mesh_stream_parser_push(mesh_stream_parser_t *parser,
                             const uint8_t *bytes, size_t length,
                             mesh_stream_frame_cb_t on_frame, void *context) {
  if (parser == NULL || (bytes == NULL && length != 0) || on_frame == NULL) {
    return false;
  }

  bool valid = true;
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = bytes[i];

    if (parser->discarding_oversized_frame) {
      if (byte == '\n') {
        parser->discarding_oversized_frame = false;
        parser->length = 0;
      }
      continue;
    }

    if (byte == '\n') {
      size_t frame_length = parser->length;
      if (frame_length > 0 && parser->buffer[frame_length - 1] == '\r') {
        frame_length--;
      }
      if (frame_length > 0 && !on_frame(parser->buffer, frame_length, context)) {
        valid = false;
      }
      parser->length = 0;
      continue;
    }

    if (parser->length >= sizeof(parser->buffer)) {
      parser->length = 0;
      parser->discarding_oversized_frame = true;
      valid = false;
      continue;
    }

    parser->buffer[parser->length++] = byte;
  }

  return valid;
}
