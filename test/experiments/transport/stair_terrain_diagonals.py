"""Choose bounded terrain diagonals with unchanged vertices and positive D/Q facets."""

from fractions import Fraction

import numpy as np
from stair_terrain_overlap import side


def areas(xy, faces):
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    return a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]


def alternative(first, second):
    shared = set(map(int, first)) & set(map(int, second))
    if len(shared) != 2:
        return None
    u = next(int(k) for k in first if k not in shared)
    index = list(first).index(u)
    a, b = int(first[(index + 1) % 3]), int(first[(index + 2) % 3])
    v = next(int(k) for k in second if k not in shared)
    index = list(second).index(v)
    if (int(second[(index + 1) % 3]), int(second[(index + 2) % 3])) != (b, a):
        return None
    return np.array([[u, a, v], [u, v, b]], dtype=first.dtype)


def checked_coordinates(xy, faces, classes, max_swaps, max_tests):
    if (
        not isinstance(xy, np.ndarray)
        or xy.dtype != np.dtype(float)
        or xy.ndim != 2
        or xy.shape[1:] != (2,)
        or not 3 <= len(xy) <= 16384
        or not np.isfinite(xy).all()
        or not isinstance(faces, np.ndarray)
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or faces.dtype.kind not in "iu"
        or not 1 <= len(faces) <= 32768
        or faces.min() < 0
        or faces.max() >= len(xy)
        or not isinstance(classes, np.ndarray)
        or classes.shape != (len(faces),)
        or classes.dtype.kind not in "iu"
        or np.any(classes > 2)
        or np.any(classes < 0)
        or type(max_swaps) is not int
        or not 1 <= max_swaps <= 64
        or type(max_tests) is not int
        or not 1 <= max_tests <= 192
    ):
        raise ValueError("bounded finite terrain facets and explicit classes required")
    with np.errstate(over="ignore"):
        quantized = xy.astype("f4").astype(float)
    if not np.isfinite(quantized).all() or np.any(areas(xy, faces) <= 0):
        raise ValueError("positive finite original terrain facets required")
    bad = np.flatnonzero(areas(quantized, faces) <= 0)
    if len(bad):
        used = np.unique(faces)
        if len(np.unique(quantized[used], axis=0)) != len(used):
            raise ValueError("terrain diagonals cannot repair merged Float32 vertices")
    return quantized, bad


def repair_diagonals(xy, faces, classes, *, max_swaps=64, max_tests=192):
    quantized, bad = checked_coordinates(xy, faces, classes, max_swaps, max_tests)
    if not len(bad):
        return faces, {}
    result, rational = faces.copy(), {}
    work = {
        "swaps": 0,
        "candidate_tests": 0,
        "neighbour_face_tests": 0,
        "edge_face_tests": 0,
    }

    def positive(coords, frame, triangles):
        for triangle in triangles:
            points = []
            for k in triangle:
                key = frame, int(k)
                if key not in rational:
                    rational[key] = tuple(Fraction(float(v)) for v in coords[k])
                points.append(rational[key])
            if side(*points) <= 0:
                return False
        return bool(np.all(areas(coords, triangles) > 0))

    for face_id in bad:
        if areas(quantized, result[[face_id]])[0] > 0:
            continue
        if work["swaps"] >= max_swaps:
            raise ValueError("terrain diagonal swap budget exhausted")
        work["neighbour_face_tests"] += len(result)
        neighbours = np.flatnonzero(np.isin(result, result[face_id]).sum(1) == 2)
        repaired = False
        for other in neighbours:
            work["candidate_tests"] += 1
            if work["candidate_tests"] > max_tests:
                raise ValueError("terrain diagonal candidate budget exhausted")
            if classes[face_id] != classes[other] or classes[face_id] == 2:
                continue
            pair = alternative(result[face_id], result[other])
            if pair is None or not positive(xy, 0, result[[face_id, other]]):
                continue
            if not positive(xy, 0, pair) or not positive(quantized, 1, pair):
                continue
            u, v = int(pair[0, 0]), int(pair[0, 2])
            work["edge_face_tests"] += len(result)
            if np.any(np.any(result == u, axis=1) & np.any(result == v, axis=1)):
                continue
            result[[face_id, other]] = pair
            work["swaps"] += 1
            repaired = True
            break
        if not repaired:
            raise ValueError("no D/Q-positive terrain diagonal within its class")
    if np.any(areas(quantized, result) <= 0):
        raise ValueError("terrain diagonal choices leave an invalid Float32 facet")
    return result, {
        **work,
        "rational_vertices": len(rational),
        "quantized_xy_bytes": quantized.nbytes,
        "output_index_bytes": result.nbytes,
    }
