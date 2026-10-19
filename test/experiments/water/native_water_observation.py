"""Read the separately observed WATER3 epoch using its pinned historical metadata codec."""

import numpy as np
from native_river_inputs import COURSE, SURFACE, geometry, source_reader

SURFACE_V3 = np.dtype(SURFACE.descr + [("input", "<u4")])
COURSE_V3 = np.dtype(COURSE.descr + [("input", "<u4")])
NO_INPUT = (1 << 32) - 1


def boolean(reader):
    value = reader.number("B")
    if value > 1:
        raise ValueError("WATER3 noncanonical boolean")
    return bool(value)


def optional(reader, decode):
    return decode() if boolean(reader) else None


def text(reader):
    count = reader.number("I")
    if count > len(reader.data) - reader.offset:
        raise ValueError("WATER3 text exceeds its byte range")
    raw = reader.data[reader.offset : reader.offset + count]
    reader.offset += count
    # Native std::string stores arbitrary bytes; this conversion remains reversible.
    return raw.decode("utf-8", "surrogateescape")


def source_record(reader):
    origin, kind, zoom, x, y = (
        reader.number(code) for code in ("I", "I", "i", "I", "I")
    )
    cell = optional(reader, lambda: [reader.number("i"), reader.number("i")])
    provider, revision, digest = (text(reader) for _ in range(3))
    features = reader.number("I")
    if (
        origin > 3
        or kind > 4
        or not 0 <= zoom <= 30
        or x >= 1 << zoom
        or y >= 1 << zoom
        or cell is not None
        and not (-90 <= cell[0] < 90 and -180 <= cell[1] < 180)
    ):
        raise ValueError("WATER3 invalid source identity")
    return {
        "origin": origin,
        "kind": kind,
        "tile": [zoom, x, y],
        "cell": cell,
        "provider": provider,
        "revision": revision,
        "digest": digest,
        "features": features,
    }


def input_record(reader):
    source, feature = reader.number("I"), reader.number("I")
    identity = optional(reader, lambda: str(reader.number("Q")))
    layer, kind, crossing = (text(reader) for _ in range(3))
    bridge, tunnel = boolean(reader), boolean(reader)
    intermittent = optional(reader, lambda: boolean(reader))
    return {
        "source": source,
        "feature": feature,
        "provider_feature_id": identity,
        "layer": layer,
        "kind": kind,
        "crossing": crossing,
        "bridge": bridge,
        "tunnel": tunnel,
        "intermittent": intermittent,
    }


def records(reader, minimum, decode):
    count = reader.number("Q")
    if count > (len(reader.data) - reader.offset) // minimum:
        raise ValueError("WATER3 metadata count exceeds its byte range")
    return tuple(decode(reader) for _ in range(count))


def read_v3(path, expected_sha256, *, byte_limit=1 << 20):
    reader = source_reader(path, expected_sha256, "WATER3", 0x335245544157, byte_limit)
    data = geometry(reader, SURFACE_V3, COURSE_V3)
    sources, inputs = (
        records(reader, 37, source_record),
        records(reader, 24, input_record),
    )
    if reader.offset != len(reader.data):
        raise ValueError("WATER3 trailing bytes")
    if sources and len(sources) != len(data["tiles"]):
        raise ValueError("WATER3 source/tile count mismatch")
    if any(
        row["source"] >= len(sources)
        or row["feature"] >= sources[row["source"]]["features"]
        for row in inputs
    ):
        raise ValueError("WATER3 invalid source/feature reference")
    if any(
        np.any((table["input"] != NO_INPUT) & (table["input"] >= len(inputs)))
        for table in (data["surfaces"], data["courses"])
    ):
        raise ValueError("WATER3 invalid geometry/input reference")
    return dict(
        data,
        sources=sources,
        inputs=inputs,
        source_bytes=len(reader.data),
        consumed_bytes=reader.offset,
    )
