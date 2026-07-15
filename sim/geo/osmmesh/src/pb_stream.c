/* libosmmesh/src/pb_stream.c — see pb_stream.h for scope. */

#include "pb_stream.h"

int pb_read_varint(const uint8_t *buf, size_t len, size_t *pos, uint64_t *out) {
    uint64_t v = 0;
    unsigned shift = 0;
    size_t p = *pos;
    while (p < len) {
        uint8_t b = buf[p++];
        v |= (uint64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            *out = v;
            *pos = p;
            return 0;
        }
        shift += 7;
        if (shift > 63) return -1; /* varint too long */
    }
    return -1; /* truncated */
}

int pb_read_tag(const uint8_t *buf, size_t len, size_t *pos,
                uint32_t *field, uint32_t *wire) {
    uint64_t t = 0;
    if (pb_read_varint(buf, len, pos, &t) != 0) return -1;
    *wire  = (uint32_t)(t & 0x7);
    *field = (uint32_t)(t >> 3);
    return 0;
}

int pb_read_length_delimited(const uint8_t *buf, size_t len, size_t *pos,
                              const uint8_t **data, size_t *data_len) {
    uint64_t n = 0;
    if (pb_read_varint(buf, len, pos, &n) != 0) return -1;
    if (n > (uint64_t)(len - *pos)) return -1; /* truncated */
    *data     = buf + *pos;
    *data_len = (size_t)n;
    *pos     += (size_t)n;
    return 0;
}

int pb_read_fixed32(const uint8_t *buf, size_t len, size_t *pos, uint32_t *out) {
    if (len - *pos < 4) return -1;
    const uint8_t *p = buf + *pos;
    *out = (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
    *pos += 4;
    return 0;
}

int pb_read_fixed64(const uint8_t *buf, size_t len, size_t *pos, uint64_t *out) {
    if (len - *pos < 8) return -1;
    const uint8_t *p = buf + *pos;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    *out = v;
    *pos += 8;
    return 0;
}

int pb_skip_field(const uint8_t *buf, size_t len, size_t *pos, uint32_t wire) {
    switch (wire) {
    case PB_WT_VARINT: {
        uint64_t tmp;
        return pb_read_varint(buf, len, pos, &tmp);
    }
    case PB_WT_FIXED64: {
        uint64_t tmp;
        return pb_read_fixed64(buf, len, pos, &tmp);
    }
    case PB_WT_LEN: {
        const uint8_t *d; size_t dn;
        return pb_read_length_delimited(buf, len, pos, &d, &dn);
    }
    case PB_WT_FIXED32: {
        uint32_t tmp;
        return pb_read_fixed32(buf, len, pos, &tmp);
    }
    default:
        /* groups (3,4) are deprecated and never appear in MVT 2.1. */
        return -1;
    }
}
