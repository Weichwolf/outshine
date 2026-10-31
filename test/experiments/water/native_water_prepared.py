"""Read the captured WATER5 metadata/prepared mesh; never reinterpret WATER2/3.

Shared binary prefix and metadata fields match WaterAssetCodec/Metadata.cpp.
This bounded reference is not a replacement native asset loader or admission.
"""

import numpy as np
from native_river_inputs import Reader, geometry
from native_water_observation import (
    COURSE_V3,
    NO_INPUT,
    SURFACE_V3,
    boolean,
    input_record,
    records,
    source_record,
)

VERSION = 0x355245544157


def decode(raw):
    if not isinstance(raw, bytes) or not 8 <= len(raw) <= 16 * 1024**2:
        raise ValueError("Bounded captured native water bytes required")
    reader = Reader(raw, "WATER5")
    if reader.number("Q") != VERSION:
        raise ValueError("Explicit WATER5 capture required")
    data = geometry(reader, SURFACE_V3, COURSE_V3)
    sources = records(reader, 37, source_record)
    inputs = records(reader, 24, input_record)
    if len(sources) != len(data["tiles"]) or any(
        i["source"] >= len(sources) or i["feature"] >= sources[i["source"]]["features"]
        for i in inputs
    ):
        raise ValueError("Native metadata source/feature references differ")
    for table in (data["surfaces"], data["courses"]):
        if np.any((table["input"] != NO_INPUT) & (table["input"] >= len(inputs))):
            raise ValueError("Native geometry input identity is invalid")
    if not boolean(reader):
        raise ValueError("Captured prepared geometry required")
    anchor = np.array([reader.number("d"), reader.number("d")])
    vertices = reader.records(np.dtype(("<f8", (3,))))
    faces = reader.records(np.dtype(("<u4", (4,))))
    nodes = reader.records(np.dtype([("bounds", "<f8", (6,)), ("links", "<u4", (4,))]))
    order = reader.records("<u4", "I")
    rejected = reader.records(np.dtype(("<u4", (2,))))
    offsets, members = reader.records("<u4", "I"), reader.records("<u4", "I")
    if (
        reader.offset != len(raw)
        or not np.isfinite(anchor).all()
        or not np.isfinite(vertices).all()
        or not np.isfinite(nodes["bounds"]).all()
        or not len(vertices)
        or not len(faces)
        or np.any(faces[:, :3] >= len(vertices))
        or np.any(faces[:, 3] >= len(data["surfaces"]))
        or len(order) != len(faces)
        or not np.array_equal(np.sort(order), np.arange(len(faces)))
        or len(offsets) != len(faces) + 1
        or offsets[0] != 0
        or offsets[-1] != len(members)
        or np.any(offsets[1:] <= offsets[:-1])
        or np.any(members >= len(data["surfaces"]))
    ):
        raise ValueError("Complete bound prepared mesh and incidence required")
    return dict(
        data,
        sources=sources,
        inputs=inputs,
        prepared={
            "anchor": anchor,
            "vertices": vertices,
            "faces": faces,
            "nodes": nodes,
            "order": order,
            "rejected": rejected,
            "face_source_offsets": offsets,
            "face_sources": members,
        },
        consumed_bytes=reader.offset,
    )


def bind_export(data, exported):
    """Verify actual exported words, not an inferred coordinate/source relation."""
    prepared = data["prepared"]
    for key, dtype in (
        ("anchor", "<f8"),
        ("vertices", "<f8"),
        ("faces", "<u4"),
        ("face_source_offsets", "<u4"),
        ("face_sources", "<u4"),
    ):
        expected = np.asarray(exported[key], dtype=dtype)
        actual = prepared[key]
        if actual.shape != expected.shape or actual.tobytes() != expected.tobytes():
            raise ValueError("Native/exported prepared words differ: " + key)
