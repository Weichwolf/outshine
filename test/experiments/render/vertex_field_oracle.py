"""Scalar protocol interpreter, independent of encoder/vector decoder helpers."""

import struct

import numpy as np


def scalar_decode(payload):
    header = struct.unpack_from("<8s10I", payload)
    magic, n, triangles, width, mask, colours, metres, *sizes = header
    if magic != b"OSVFW01\0" or mask not in (5, 15):
        raise ValueError("Unknown scalar field packet")
    lanes = 2 if mask == 15 else 1
    at = 48 + n * lanes * 4
    tables = []
    for count, components in zip(sizes, (3, 2, 1, 2), strict=True):
        tables.append((at, count, components))
        at += count * components * 4
    uv_at, metres_at = at, at + n * 8
    if mask == 5:
        at += n * 8 + metres * 8
    colour_at = at
    indices_at = at + colours * 16
    if indices_at + triangles * 3 * width != len(payload):
        raise ValueError("Scalar packet size mismatch")
    rows = {name: [] for name in ("positions", "uv", "normal", "metres", "colour")}
    for vertex in range(n):
        first = struct.unpack_from("<I", payload, 48 + vertex * lanes * 4)[0]
        pos, normal = first & 65535, first >> 16
        ids = [pos, None, normal, None]
        if mask == 15:
            second = struct.unpack_from("<I", payload, 52 + vertex * 8)[0]
            ids[1], ids[3] = second & 65535, second >> 16
        for name, index, (start, count, components) in zip(
            ("positions", "uv", "normal", "metres"), ids, tables, strict=True
        ):
            if index is not None:
                if index >= count:
                    raise ValueError("Scalar dictionary ID outside table")
                value = struct.unpack_from(
                    f"<{components}I", payload, start + index * components * 4
                )
            elif name == "uv":
                value = struct.unpack_from("<2I", payload, uv_at + vertex * 8)
            elif vertex < metres:
                value = struct.unpack_from("<2I", payload, metres_at + vertex * 8)
            else:
                value = (0, 0)
            rows[name].append(value)
        rows["colour"].append(
            struct.unpack_from("<4I", payload, colour_at + vertex * 16)
            if vertex < colours
            else (0x3F800000,) * 4
        )
    result = {name: np.asarray(values, "<u4") for name, values in rows.items()}
    result["normal"] = result["normal"].reshape(n)
    for name in ("positions", "uv", "metres", "colour"):
        result[name] = result[name].view("<f4")
    code = "H" if width == 2 else "I"
    indices = struct.unpack_from(f"<{triangles * 3}{code}", payload, indices_at)
    result["indices"] = np.asarray(indices, f"<u{width}").reshape(-1, 3)
    return result
