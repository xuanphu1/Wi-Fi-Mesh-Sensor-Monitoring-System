/**
 * Compact UDP mesh sensor frame — sensor_id matches MRS SensorTypes.h (0..13) + ANALOG=14 + GENERIC=0xFE.
 * Keep in sync with MeshGateWay/.../include/mesh_sensor_compact.h
 */
#ifndef MESH_SENSOR_COMPACT_H
#define MESH_SENSOR_COMPACT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MESH_COMPACT_MAGIC0     0xEDu
#define MESH_COMPACT_MAGIC1     0x10u
#define MESH_COMPACT_VER        1u
#define MESH_COMPACT_MAX_PORTS  8u
#define MESH_COMPACT_MAX_FIELDS 8u

typedef enum {
    MESH_SEN_BME280  = 0,
    MESH_SEN_MHZ14A  = 1,
    MESH_SEN_PMS7003 = 2,
    MESH_SEN_DHT22   = 3,
    MESH_SEN_MQ2     = 4,
    MESH_SEN_MQ3     = 5,
    MESH_SEN_MQ4     = 6,
    MESH_SEN_MQ5     = 7,
    MESH_SEN_MQ6     = 8,
    MESH_SEN_MQ7     = 9,
    MESH_SEN_MQ8     = 10,
    MESH_SEN_MQ9     = 11,
    MESH_SEN_MQ135   = 12,
    MESH_SEN_AHT10   = 13,
    MESH_SEN_ANALOG  = 14,
    MESH_SEN_GENERIC = 0xFEu,
} mesh_compact_sensor_id_t;

static inline bool mesh_compact_is_frame(const uint8_t *data, size_t len)
{
    return len >= 5u && data[0] == MESH_COMPACT_MAGIC0 && data[1] == MESH_COMPACT_MAGIC1 && data[2] == MESH_COMPACT_VER;
}

static inline uint8_t mesh_compact_expected_nfields(uint8_t sensor_id)
{
    switch (sensor_id) {
    case MESH_SEN_BME280:
        return 3u;
    case MESH_SEN_MHZ14A:
        return 1u;
    case MESH_SEN_PMS7003:
        return 3u;
    case MESH_SEN_DHT22:
        return 2u;
    case MESH_SEN_AHT10:
        return 2u;
    case MESH_SEN_ANALOG:
        return 3u;
    case MESH_SEN_GENERIC:
        return 0u;
    default:
        if (sensor_id >= MESH_SEN_MQ2 && sensor_id <= MESH_SEN_MQ135) {
            return 0u;
        }
        return 0u;
    }
}

static inline void mesh_compact_put_u8(uint8_t *buf, size_t *off, size_t cap, uint8_t v)
{
    if (*off < cap) {
        buf[(*off)++] = v;
    }
}

static inline void mesh_compact_put_f32le(uint8_t *buf, size_t *off, size_t cap, float f)
{
    if (*off + 4u > cap) {
        return;
    }
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    buf[(*off)++] = (uint8_t)(u & 0xffu);
    buf[(*off)++] = (uint8_t)((u >> 8) & 0xffu);
    buf[(*off)++] = (uint8_t)((u >> 16) & 0xffu);
    buf[(*off)++] = (uint8_t)((u >> 24) & 0xffu);
}

static inline bool mesh_compact_begin(uint8_t *buf, size_t cap, size_t *off, uint8_t node_level, uint8_t n_ports)
{
    *off = 0;
    if (cap < 5u || n_ports > MESH_COMPACT_MAX_PORTS) {
        return false;
    }
    mesh_compact_put_u8(buf, off, cap, MESH_COMPACT_MAGIC0);
    mesh_compact_put_u8(buf, off, cap, MESH_COMPACT_MAGIC1);
    mesh_compact_put_u8(buf, off, cap, MESH_COMPACT_VER);
    mesh_compact_put_u8(buf, off, cap, node_level);
    mesh_compact_put_u8(buf, off, cap, n_ports);
    return true;
}

static inline bool mesh_compact_append_port(uint8_t *buf, size_t cap, size_t *off,
                                            uint8_t port_1based, uint8_t sensor_id,
                                            const float *vals, uint8_t n_fields)
{
    if (port_1based == 0u || n_fields == 0u || n_fields > MESH_COMPACT_MAX_FIELDS || vals == NULL) {
        return false;
    }
    if (sensor_id != MESH_SEN_GENERIC) {
        uint8_t exp = mesh_compact_expected_nfields(sensor_id);
        if (exp != 0u && n_fields != exp) {
            return false;
        }
    }
    if (*off + 3u + (size_t)n_fields * 4u > cap) {
        return false;
    }
    mesh_compact_put_u8(buf, off, cap, port_1based);
    mesh_compact_put_u8(buf, off, cap, sensor_id);
    mesh_compact_put_u8(buf, off, cap, n_fields);
    for (uint8_t i = 0; i < n_fields; i++) {
        mesh_compact_put_f32le(buf, off, cap, vals[i]);
    }
    return true;
}

#endif
