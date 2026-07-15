/* libosmmesh/src/pb_stream.h — INTERNAL, not under include/osmmesh/.
 *
 * Minimal Protobuf wire-format reader, scoped exactly to what the MVT 2.1
 * decoder needs: varints, zigzag, length-delimited, fixed32, fixed64, skip.
 * No schema, no reflection, no message dispatch — the caller does that.
 *
 * Errors: all readers return 0 on success, nonzero on short buffer / malformed
 * input. Advance `*pos` only on success; on failure the buffer state is
 * undefined (caller should abort its current parse).
 */

#ifndef OSMMESH_PB_STREAM_H
#define OSMMESH_PB_STREAM_H

#include <stddef.h>
#include <stdint.h>

/* Wire types (protobuf). Only the ones we actually reference by name. */
#define PB_WT_VARINT  0
#define PB_WT_FIXED64 1
#define PB_WT_LEN     2
#define PB_WT_FIXED32 5

/* Decode a varint from buf[*pos..len). Advance *pos on success. */
int pb_read_varint(const uint8_t *buf, size_t len, size_t *pos, uint64_t *out);

/* Read a protobuf tag: field_number << 3 | wire_type. */
int pb_read_tag(const uint8_t *buf, size_t len, size_t *pos,
                uint32_t *field, uint32_t *wire);

/* Decode a length-delimited field: returns a pointer into the buffer and the
 * payload length. The returned pointer is valid for the lifetime of `buf`. */
int pb_read_length_delimited(const uint8_t *buf, size_t len, size_t *pos,
                              const uint8_t **data, size_t *data_len);

/* Read fixed32 / fixed64 little-endian. */
int pb_read_fixed32(const uint8_t *buf, size_t len, size_t *pos, uint32_t *out);
int pb_read_fixed64(const uint8_t *buf, size_t len, size_t *pos, uint64_t *out);

/* Skip an unknown field given its wire type. Advances *pos past the field. */
int pb_skip_field(const uint8_t *buf, size_t len, size_t *pos, uint32_t wire);

/* ZigZag decode. 32-bit via 64-bit for identical expression. */
static inline int64_t pb_zigzag_decode(uint64_t v) {
    /* Standard protobuf zigzag: (v >> 1) ^ -(v & 1) as signed.
     * Compute via int64_t to avoid UB on unary minus of unsigned zero. */
    return (int64_t)(v >> 1) ^ -(int64_t)(v & 1);
}

#endif /* OSMMESH_PB_STREAM_H */
