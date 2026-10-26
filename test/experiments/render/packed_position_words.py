"""Lossless Float32 word deltas; two uint32 loads, no table or GPU uint64."""

import struct
from dataclasses import dataclass

import numpy as np

HEADER = struct.Struct("<8s8I")
MAGIC = b"OSPDEL1\0"
MAX_VERTICES = 393216
MAX_BYTES = 64 * 1024**2


@dataclass(frozen=True)
class Candidate:
    payload: bytes | None
    reason: str | None
    report: dict


def checked_spans(spans, size):
    if type(size) is not int or not 0 <= size <= MAX_BYTES:
        raise ValueError("Bounded native core required")
    if not isinstance(spans, (list, tuple)) or not 0 < len(spans) <= 8192:
        raise ValueError("Bounded common native vertex layout required")
    end = total = 0
    for span in spans:
        if (
            not isinstance(span, (list, tuple))
            or len(span) != 3
            or any(type(v) is not int for v in span)
        ):
            raise ValueError("Integer native span metadata required")
        offset, count, stride = span
        if offset < end or count < 0 or stride not in (12, 24):
            raise ValueError("Overlapping or invalid native vertex span")
        end = offset + count * stride
        total += count
        if end > size or total > MAX_VERTICES:
            raise ValueError("Native vertex spans exceed bounded core")
    return total


def position_words(raw, spans):
    if not isinstance(raw, bytes):
        raise TypeError("Owned immutable core bytes required")
    checked_spans(spans, len(raw))
    return np.concatenate(
        [
            np.ndarray((n, 3), "<u4", raw, offset, (stride, 4))
            for offset, n, stride in spans
        ]
    )


def unpack_words(packed, base, widths):
    """Shader-shaped uint32 shifts/OR/add/bitcast, including 32-bit crossings."""
    result = np.empty((len(packed), 3), "<u4")
    shift = 0
    for axis, width in enumerate(widths):
        if not width:
            result[:, axis] = base[axis]
            continue
        if shift < 32:
            delta = packed[:, 0] >> shift
            if shift + width > 32:
                delta |= packed[:, 1] << (32 - shift)
        else:
            delta = packed[:, 1] >> (shift - 32)
        if width < 32:
            delta &= (1 << width) - 1
        if int(delta.max()) + int(base[axis]) > 0xFFFFFFFF:
            raise ValueError("Position word overflow")
        result[:, axis] = delta + base[axis]
        shift += width
    if not np.isfinite(result.view("<f4")).all():
        raise ValueError("Nonfinite decoded native position")
    return result


def encode(raw, spans):
    words = position_words(raw, spans)
    n = len(words)
    if not n:
        return Candidate(None, "empty core keeps Raw", {"vertices": 0})
    if not np.isfinite(words.view("<f4")).all():
        raise ValueError("Finite native Float32 positions required")
    base = words.min(0)
    widths = [
        int(int(high) - int(low)).bit_length() for low, high in zip(base, words.max(0))
    ]
    report = {"vertices": n, "bits": widths, "sum_bits": sum(widths)}
    if sum(widths) > 64:
        return Candidate(None, "position word range exceeds 64 bits", report)
    packed = np.zeros((n, 2), "<u4")
    shift = 0
    for axis, width in enumerate(widths):
        if not width:
            continue
        delta = words[:, axis] - base[axis]
        if shift < 32:
            packed[:, 0] |= delta << shift
            if shift + width > 32:
                packed[:, 1] |= delta >> (32 - shift)
        else:
            packed[:, 1] |= delta << (shift - 32)
        shift += width
    chunks = [HEADER.pack(MAGIC, n, len(raw), *base, *widths)]
    offset = vertex = 0
    for start, count, stride in spans:
        chunks.append(raw[offset:start])
        records = np.empty((count, stride - 4), np.uint8)
        records[:, :8] = packed[vertex : vertex + count].view(np.uint8)
        if count and stride > 12:
            records[:, 8:] = np.ndarray(
                (count, stride - 12), np.uint8, raw, start + 12, (stride, 1)
            )
        chunks.append(records.tobytes())
        offset, vertex = start + count * stride, vertex + count
    chunks.append(raw[offset:])
    payload = b"".join(chunks)
    report.update(
        {
            "header_bytes": HEADER.size,
            "raw_core_bytes": len(raw),
            "packed_core_bytes": len(payload),
            "known_serialization_copy_bytes": 2 * len(payload),
            "vertex_record_bytes": [24, 20],
            "dependent_table_gathers": 0,
        }
    )
    if len(payload) >= len(raw):
        return Candidate(None, "Raw core is no larger", report)
    return Candidate(payload, None, report)


def decode(payload, spans):
    if not isinstance(payload, bytes):
        raise TypeError("Owned immutable packet bytes required")
    if not HEADER.size <= len(payload) <= MAX_BYTES:
        raise ValueError("Invalid bounded position packet length")
    magic, n, size, *values = HEADER.unpack_from(payload)
    base, widths = np.array(values[:3], "<u4"), values[3:]
    if (
        magic != MAGIC
        or not 0 < n <= MAX_VERTICES
        or any(width > 32 for width in widths)
        or sum(widths) > 64
        or len(payload) != HEADER.size + size - n * 4
        or checked_spans(spans, size) != n
    ):
        raise ValueError("Invalid position header/layout")
    packed = np.empty((n, 2), "<u4")
    byte_offset, vertex = HEADER.size, 0
    restored = bytearray(size)
    end = 0
    for start, count, stride in spans:
        gap = start - end
        restored[end:start] = payload[byte_offset : byte_offset + gap]
        byte_offset += gap
        extent = count * (stride - 4)
        block = np.ndarray((count, stride - 4), np.uint8, payload, byte_offset)
        packed[vertex : vertex + count] = block[:, :8].copy().view("<u4")
        if count and stride > 12:
            np.ndarray(
                (count, stride - 12), np.uint8, restored, start + 12, (stride, 1)
            )[:] = block[:, 8:]
        vertex += count
        byte_offset += extent
        end = start + count * stride
    restored[end:] = payload[byte_offset:]
    used = sum(widths)
    if (
        used < 32
        and np.any(packed[:, 0] >> used)
        or used < 64
        and np.any(packed[:, 1] >> max(0, used - 32))
    ):
        raise ValueError("Nonzero unused packed bits")
    words = unpack_words(packed, base, widths)
    vertex = 0
    for offset, count, stride in spans:
        np.ndarray((count, 3), "<u4", restored, offset, (stride, 4))[:] = words[
            vertex : vertex + count
        ]
        vertex += count
    return bytes(restored)
