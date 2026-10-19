"""Read the temporary accepted-PieceMesh capture from #819, never a cache format."""

import hashlib
import struct

import numpy as np

CAPTURE_SHA256 = "789773c9a20398d2aa8837a0449539c1138162743203f56a1b83326fd7e248bc"
CLUSTER = np.dtype([("bounds", "<f4", 10), ("range", "<u4", 2), ("level", "<u4")])


def records(data):
    offset, ordinal = 0, 0
    while offset < len(data):
        if len(data) - offset < 56:
            raise ValueError("truncated capture header/footer")
        magic, nv, ni, nc, nr, sv, sc = struct.unpack_from("<7Q", data, offset)
        offset += 56
        if (
            magic != 0x819B0A0D
            or sv != 24
            or sc != 52
            or min(nv, ni, nc, nr) == 0
            or ni % 3
        ):
            raise ValueError("invalid clustered capture header/strides")
        sizes = (nv * sv, ni * 4, nc * sc, nr * 64)
        if sum(sizes) > len(data) - offset:
            raise ValueError("truncated capture arrays")
        positions = np.frombuffer(data, "<f4", nv * 6, offset).reshape(-1, 6)[:, :3]
        offset += sizes[0]
        indices = np.frombuffer(data, "<u4", ni, offset)
        offset += sizes[1]
        clusters = np.frombuffer(data, CLUSTER, nc, offset)
        offset += sizes[2]
        instances = np.frombuffer(data, "<f4", nr * 16, offset).reshape(-1, 4, 4)
        offset += sizes[3]
        ranges = clusters["range"].astype(np.int64)
        if (
            indices.max() >= nv
            or not np.isfinite(positions).all()
            or (ranges % 3).any()
            or (ranges[:, 1] == 0).any()
            or (ranges.sum(1) > ni).any()
        ):
            raise ValueError("invalid capture index/position/range data")
        # Independent coverage oracle, including overlap and every recorded LOD.
        difference = np.zeros(ni + 1, np.int64)
        np.add.at(difference, ranges[:, 0], 1)
        np.add.at(difference, ranges.sum(1), -1)
        coverage = np.cumsum(difference[:-1])
        yield {
            "key": f"accepted-piece-{ordinal}",
            "group": "native-Flensburg-capture",
            "provenance": {
                "capture_record": ordinal,
                "scope": "accepted-piece order, no OSM/source owner inference",
                "cluster_levels": sorted({int(v) for v in clusters["level"]}),
                "uncovered_index_references": int((coverage == 0).sum()),
            },
            "vertices": positions,
            "indices": indices.reshape(-1, 3),
            "ranges": [(int(f) // 3, int(n) // 3) for f, n in ranges],
            "spheres": clusters["bounds"][:, :4],
            "instances": instances,
        }
        ordinal += 1


def capture(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != CAPTURE_SHA256:
        raise ValueError("original accepted-PieceMesh capture hash changed")
    yield from records(data)
