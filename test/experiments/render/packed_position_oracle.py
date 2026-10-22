"""Independent scalar integer oracle for trusted valid experimental packets."""

import math
import struct


def scalar_decode(payload, spans):
    magic, n, size, *header = struct.unpack_from("<8s8I", payload)
    assert magic == b"OSPDEL1\0"
    bases, widths = header[:3], header[3:]
    result = bytearray()
    cursor, end, vertices = 40, 0, 0
    for start, count, stride in spans:
        result.extend(payload[cursor : cursor + start - end])
        cursor += start - end
        for _ in range(count):
            low, high = struct.unpack_from("<II", payload, cursor)
            bits = low + high * 2**32
            shift = 0
            for base, width in zip(bases, widths, strict=True):
                delta = (bits // 2**shift) % 2**width
                word = base + delta
                encoded = struct.pack("<I", word)
                assert math.isfinite(struct.unpack("<f", encoded)[0])
                result.extend(encoded)
                shift += width
            assert bits < 2**shift
            result.extend(payload[cursor + 8 : cursor + stride - 4])
            cursor += stride - 4
            vertices += 1
        end = start + count * stride
    result.extend(payload[cursor:])
    assert vertices == n
    assert len(result) == size
    return bytes(result)
