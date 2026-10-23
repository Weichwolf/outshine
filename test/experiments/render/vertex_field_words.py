"""Bit-exact field dictionaries; experimental word buffers, not a native format."""

import struct
from dataclasses import dataclass

import numpy as np

MAGIC = b"OSVFW01\0"
HEADER = struct.Struct("<8s10I")
FIELDS = ("positions", "uv", "normal", "metres")
WIDTHS = (3, 2, 1, 2)
MAX_VERTICES = 393216
MAX_TRIANGLES = 131072


@dataclass(frozen=True)
class Candidate:
    payload: bytes | None
    reason: str | None
    report: dict


def words(source):
    n = len(source["positions"])
    if not 0 <= n <= MAX_VERTICES:
        raise ValueError("Bounded native vertex inputs required")
    result = {}
    for name, width in zip((*FIELDS, "colour"), (*WIDTHS, 4), strict=True):
        values = np.asarray(source[name])
        shape = (n,) if name == "normal" else (n, width)
        dtype = np.dtype("<u4" if name == "normal" else "<f4")
        if values.shape != shape or values.dtype != dtype:
            raise ValueError("Native field shape or word type changed")
        if name != "normal" and not np.isfinite(values).all():
            raise ValueError("Nonfinite native field")
        result[name] = np.ascontiguousarray(values).view("<u4").reshape(n, width)
    indices = np.asarray(source["indices"])
    if (
        indices.ndim != 2
        or indices.shape[1:] != (3,)
        or len(indices) > MAX_TRIANGLES
        or indices.dtype.kind not in "iu"
        or len(indices)
        and (indices.min() < 0 or indices.max() >= n)
    ):
        raise ValueError("Bounded valid original triangle indices required")
    return result, indices


def prefix_count(values, default):
    different = np.flatnonzero(np.any(values != default, axis=1))
    return int(different[-1]) + 1 if len(different) else 0


def encode(source, *, mode="all", raw_core_bytes=None):
    data, indices = words(source)
    if mode not in ("all", "position-normal"):
        raise ValueError("Unknown field dictionary control")
    if raw_core_bytes is not None and (
        type(raw_core_bytes) is not int or raw_core_bytes < 0
    ):
        raise ValueError("Native control extent must be a nonnegative integer")
    if not len(indices):
        return Candidate(
            None,
            "empty body keeps native raw representation",
            {"vertices": len(data["positions"])},
        )
    indexed = (
        ("positions", "normal", "uv", "metres")
        if mode == "all"
        else ("positions", "normal")
    )
    tables, ids = {}, {}
    report = {
        "mode": mode,
        "vertices": len(data["positions"]),
        "processed_field_records": 0,
        "tables": {},
    }
    for name in indexed:
        table, inverse = np.unique(data[name], axis=0, return_inverse=True)
        report["processed_field_records"] += len(data[name])
        report["tables"][name] = {"records": len(table), "bytes": table.nbytes}
        if len(table) > 65536:
            return Candidate(None, f"{name} dictionary exceeds u16", report)
        tables[name], ids[name] = table, inverse.astype("<u4")
    n = len(data["positions"])
    tokens = np.empty((n, len(indexed) // 2), "<u4")
    for column in range(tokens.shape[1]):
        tokens[:, column] = ids[indexed[2 * column]] | (
            ids[indexed[2 * column + 1]] << 16
        )
    colour_count = prefix_count(data["colour"], np.uint32(0x3F800000))
    metre_count = prefix_count(data["metres"], np.uint32(0)) if mode != "all" else 0
    index_width = 2 if indices.max() <= 65535 else 4
    topology = indices.astype(f"<u{index_width}")
    mask = 15 if mode == "all" else 5
    counts = [len(tables.get(name, ())) for name in FIELDS]
    header = HEADER.pack(
        MAGIC, n, len(indices), index_width, mask, colour_count, metre_count, *counts
    )
    chunks = [header, tokens.tobytes()]
    chunks.extend(tables[name].tobytes() for name in FIELDS if name in tables)
    if mode != "all":
        chunks.extend((data["uv"].tobytes(), data["metres"][:metre_count].tobytes()))
    chunks.extend((data["colour"][:colour_count].tobytes(), topology.tobytes()))
    payload = b"".join(chunks)
    report.update(
        {
            "payload_bytes": len(payload),
            "header_bytes": HEADER.size,
            "token_bytes": tokens.nbytes,
            "index_bytes": topology.nbytes,
            "raw_colour_bytes": colour_count * 16,
            "raw_uv_bytes": 0 if mode == "all" else n * 8,
            "raw_metre_bytes": metre_count * 8,
            "raw_core_bytes": raw_core_bytes,
            "smaller_than_native_core": len(payload) < raw_core_bytes
            if raw_core_bytes is not None
            else None,
            "logical_field_read_bytes_per_wall_vertex": 40 if mode == "all" else 36,
            "native_logical_field_read_bytes_per_wall_vertex": 32,
        }
    )
    # A size decision is experimental eligibility, never GPU/product admission.
    if raw_core_bytes is not None and len(payload) >= raw_core_bytes:
        return Candidate(None, "native raw core is no larger", report)
    return Candidate(payload, None, report)


def layout(payload):
    if not isinstance(payload, bytes):
        raise TypeError("Owned immutable packet bytes required")
    if len(payload) < HEADER.size or len(payload) > 64 * 1024**2:
        raise ValueError("Invalid bounded field packet length")
    magic, n, triangles, index_width, mask, colours, metres, *counts = (
        HEADER.unpack_from(payload)
    )
    if (
        magic != MAGIC
        or not 0 < n <= MAX_VERTICES
        or not 0 < triangles <= MAX_TRIANGLES
        or index_width not in (2, 4)
        or mask not in (5, 15)
        or colours > n
        or metres > n
    ):
        raise ValueError("Invalid field packet header")
    indexed = (
        ("positions", "normal", "uv", "metres")
        if mask == 15
        else ("positions", "normal")
    )
    for name, count in zip(FIELDS, counts, strict=True):
        if (
            name in indexed
            and not 0 < count <= min(n, 65536)
            or name not in indexed
            and count != 0
        ):
            raise ValueError("Invalid dictionary extent")
    if mask == 15 and metres:
        raise ValueError("Indexed metre field cannot have raw prefix")
    size = (
        HEADER.size
        + n * (8 if mask == 15 else 4)
        + sum(count * width * 4 for count, width in zip(counts, WIDTHS, strict=True))
        + (n * 8 + metres * 8 if mask == 5 else 0)
        + colours * 16
        + triangles * 3 * index_width
    )
    if size != len(payload):
        raise ValueError("Field packet truncation or trailing data")
    offset = HEADER.size
    tokens = np.frombuffer(payload, "<u4", n * len(indexed) // 2, offset).reshape(n, -1)
    offset += tokens.nbytes
    tables = {}
    for name, count, width in zip(FIELDS, counts, WIDTHS, strict=True):
        if count:
            tables[name] = np.frombuffer(payload, "<u4", count * width, offset).reshape(
                count, width
            )
            offset += tables[name].nbytes
    raw = {}
    if mask == 5:
        raw["uv"] = np.frombuffer(payload, "<u4", n * 2, offset).reshape(n, 2)
        offset += raw["uv"].nbytes
        raw["metres"] = np.frombuffer(payload, "<u4", metres * 2, offset).reshape(
            metres, 2
        )
        offset += raw["metres"].nbytes
    raw["colour"] = np.frombuffer(payload, "<u4", colours * 4, offset).reshape(
        colours, 4
    )
    offset += raw["colour"].nbytes
    topology = np.frombuffer(
        payload, f"<u{index_width}", triangles * 3, offset
    ).reshape(-1, 3)
    if topology.max() >= n:
        raise ValueError("Field packet triangle index out of range")
    return n, indexed, tokens, tables, raw, topology


def decode(payload):
    n, indexed, tokens, tables, raw, topology = layout(payload)
    result = {}
    for slot, name in enumerate(indexed):
        ids = (tokens[:, slot // 2] >> (16 * (slot % 2))) & 65535
        if ids.max() >= len(tables[name]):
            raise ValueError("Field dictionary ID out of range")
        result[name] = tables[name][ids]
    if "uv" in raw:
        result["uv"] = raw["uv"].copy()
        result["metres"] = np.zeros((n, 2), "<u4")
        result["metres"][: len(raw["metres"])] = raw["metres"]
    result["colour"] = np.full((n, 4), 0x3F800000, "<u4")
    result["colour"][: len(raw["colour"])] = raw["colour"]
    for name in (*FIELDS, "colour"):
        if name == "normal":
            result[name] = result[name].reshape(n)
        else:
            result[name] = result[name].view("<f4")
            if not np.isfinite(result[name]).all():
                raise ValueError("Nonfinite decoded field")
    result["indices"] = topology
    return result
