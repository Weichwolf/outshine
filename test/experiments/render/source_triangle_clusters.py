"""Static per-part triangle ranges with the existing native Morton convention."""

from dataclasses import dataclass

import numpy as np


def spread(bits):
    bits = bits & 0x3FF
    for shift, mask in (
        (16, 0x030000FF),
        (8, 0x0300F00F),
        (4, 0x030C30C3),
        (2, 0x09249249),
    ):
        bits = (bits | (bits << shift)) & mask
    return bits


@dataclass(frozen=True)
class TriangleGroups:
    faces: np.ndarray
    offsets: np.ndarray
    bounds: np.ndarray

    @property
    def payload_bytes(self):
        return sum(a.nbytes for a in (self.faces, self.offsets, self.bounds))


def immutable(value, dtype):
    a = np.asarray(value, dtype=dtype)
    return np.frombuffer(a.tobytes(), dtype=dtype).reshape(a.shape)


def build(source, *, leaf_triangles=128, order="morton"):
    positions, triangles = source["positions"], source["indices"]
    if (
        type(leaf_triangles) is not int
        or not 1 <= leaf_triangles <= 256
        or order not in ("morton", "source")
        or positions.dtype != np.dtype("<f4")
        or positions.ndim != 2
        or positions.shape[1] != 3
        or not 1 <= len(positions) <= 262144
        or not np.isfinite(positions).all()
        or triangles.ndim != 2
        or triangles.shape[1] != 3
        or triangles.dtype.kind not in "iu"
        or not 1 <= len(triangles) <= 131072
        or triangles.min() < 0
        or triangles.max() >= len(positions)
    ):
        raise ValueError(
            "Bounded original geometry, leaf limit and declared ordering required"
        )
    parts = source.get(
        "parts", ({"vertices": len(positions), "triangles": len(triangles)},)
    )
    faces, offsets, bounds = [], [0], []
    vertex = first = 0
    for part in parts:
        n, count = part["vertices"], part["triangles"]
        if (
            type(n) is not int
            or type(count) is not int
            or n < 0
            or count < 0
            or vertex + n > len(positions)
            or first + count > len(triangles)
        ):
            raise ValueError("Bounded nonnegative original part ranges required")
        p, t = positions[vertex : vertex + n], triangles[first : first + count]
        if count and (t.min() < vertex or t.max() >= vertex + n):
            raise ValueError("Original triangles must retain their part")
        ids = np.arange(first, first + count, dtype=np.uint32)
        if count and order == "morton":
            xyz = positions[t].astype(float)
            centre = xyz[:, 0] * (1 / 3) + xyz[:, 1] * (1 / 3) + xyz[:, 2] * (1 / 3)
            low, high = p.min(0).astype(float), p.max(0).astype(float)
            fraction = np.divide(
                centre - low, high - low, out=np.zeros_like(centre), where=high > low
            )
            cell = (np.clip(fraction, 0, 1) * 1023).astype(np.uint32)
            code = (
                (spread(cell[:, 0]) << 2)
                | (spread(cell[:, 1]) << 1)
                | spread(cell[:, 2])
            )
            ids = ids[np.argsort(code, kind="stable")]
        for at in range(0, count, leaf_triangles):
            chunk = ids[at : at + leaf_triangles]
            corners = positions[triangles[chunk]]
            bounds.append([corners.min((0, 1)), corners.max((0, 1))])
            faces.extend(chunk.tolist())
            offsets.append(len(faces))
        vertex += n
        first += count
    if (
        vertex != len(positions)
        or first != len(triangles)
        or len(faces) != len(triangles)
    ):
        raise ValueError("Complete original part inventory required")
    return TriangleGroups(
        immutable(faces, "<u4"), immutable(offsets, "<u4"), immutable(bounds, "<f4")
    )


def projected_control(source, groups, centers, radius, raster):
    """Favorable data-dependent rectangles, not a pre-projection accelerator.

    Reads every fine projected triangle. Each leaf rectangle encloses the exact
    union of the baseline's expanded triangle bounding boxes. No GPU IDs/demand.
    """
    vertex = centers[source["indices"]]
    error = radius[source["indices"]].max(1)
    low, high = vertex.min(1) - error, vertex.max(1) + error
    starts = groups.offsets[:-1]
    lower = np.minimum.reduceat(low[groups.faces], starts)
    upper = np.maximum.reduceat(high[groups.faces], starts)
    a = np.maximum(0, np.ceil(np.nextafter(lower - 0.5, -np.inf)))
    b = np.minimum(np.asarray(raster) - 1, np.floor(np.nextafter(upper - 0.5, np.inf)))
    keep = (a <= b).all(1)
    expanded = np.repeat(keep, np.diff(groups.offsets))
    ids = np.sort(groups.faces[expanded])
    return ids, keep
