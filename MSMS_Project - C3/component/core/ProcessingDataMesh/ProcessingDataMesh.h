#ifndef PROCESSING_DATA_MESH_H
#define PROCESSING_DATA_MESH_H

#include <stddef.h>
#include <stdint.h>

#include "ErrorCodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process a mesh telemetry JSON frame for gateway UART output.
 *
 * The input frame contains `seq`. This function calculates per-node packet
 * loss percentage in a 10-second window, then emits a compact JSON frame with
 * `packetloss` in place of `seq`.
 *
 * Output key order is fixed:
 *   v -> packetloss -> n -> i -> t -> ver -> err -> p
 */
system_err_t ProcessingDataMesh_ProcessFrame(const uint8_t *input,
                                             size_t input_len,
                                             char *output,
                                             size_t output_cap,
                                             size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif /* PROCESSING_DATA_MESH_H */
