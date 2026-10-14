"""Split triangle edges at existing interface vertices; preserve coordinates and faces."""

from itertools import pairwise

import numpy as np
import shapely


def _interface_candidates(tree, vertices, tolerance, maximum):
    if maximum is None:
        return tree.query(
            shapely.points(vertices), predicate="dwithin", distance=tolerance
        )
    if type(maximum) is not int or maximum < 0:
        raise ValueError("nonnegative interface candidate cap required")
    parts, counted, start = [], 0, 0
    edges = len(tree.geometries)
    while start < len(vertices):
        batch = min(256, len(vertices) - start, (maximum - counted) // max(1, edges))
        if batch == 0:
            raise ValueError("interface candidate budget exhausted")
        pairs = tree.query(
            shapely.points(vertices[start : start + batch]),
            predicate="dwithin",
            distance=tolerance,
        )
        pairs[0] += start
        parts.append(pairs)
        counted += pairs.shape[1]
        start += batch
    return np.concatenate(parts, axis=1) if parts else np.empty((2, 0), dtype=np.intp)


def conform_triangles(
    vertices: np.ndarray,
    indices: np.ndarray,
    *,
    max_candidates: int | None = None,
    max_faces: int | None = None,
) -> tuple[np.ndarray, dict[str, int]]:
    vertices, indices = np.asarray(vertices, dtype=float), np.asarray(indices)
    if vertices.ndim != 2 or vertices.shape[1] != 2 or not np.isfinite(vertices).all():
        raise ValueError("finite Nx2 interface vertices required")
    if indices.ndim != 2 or indices.shape[1] != 3 or indices.dtype.kind not in "iu":
        raise ValueError("Mx3 integer triangle indices required")
    if len(indices) and (indices.min() < 0 or indices.max() >= len(vertices)):
        raise ValueError("triangle index outside the vertex array")
    if max_faces is not None and (
        type(max_faces) is not int or max_faces < len(indices)
    ):
        raise ValueError("interface face budget exhausted")
    edges = np.concatenate([indices[:, [0, 1]], indices[:, [1, 2]], indices[:, [2, 0]]])
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    tolerance = (
        64 * np.finfo(float).eps * max(1.0, float(np.abs(vertices).max(initial=0)))
    )
    tree = shapely.STRtree(shapely.linestrings(vertices[edges]))
    pairs = _interface_candidates(tree, vertices, tolerance, max_candidates)
    splits = {}
    for point, edge in pairs.T:
        a, b = edges[edge]
        if point in (a, b):
            continue
        delta = vertices[b] - vertices[a]
        length = delta @ delta
        if length <= 0:
            continue
        station = float((vertices[point] - vertices[a]) @ delta / length)
        if 0 < station < 1:
            splits.setdefault((int(a), int(b)), []).append((station, int(point)))
    if not splits:
        return indices, {"split_edges": 0, "added_triangles": 0}
    splits = {e: [p for _, p in sorted(set(values))] for e, values in splits.items()}
    output = []
    for original in indices:
        faces = [tuple(map(int, original))]
        for a, b in zip(original, np.roll(original, -1)):
            key = tuple(sorted((int(a), int(b))))
            if key not in splits:
                continue
            points = splits[key] if a < b else splits[key][::-1]
            owner = next(i for i, f in enumerate(faces) if a in f and b in f)
            opposite = next(p for p in faces[owner] if p not in (a, b))
            chain = [int(a), *points, int(b)]
            faces[owner : owner + 1] = [(u, v, opposite) for u, v in pairwise(chain)]
        if max_faces is not None and len(output) + len(faces) > max_faces:
            raise ValueError("interface face budget exhausted")
        output.extend(faces)
    result = np.array(output, dtype=indices.dtype)
    return result, {
        "split_edges": len(splits),
        "added_triangles": len(result) - len(indices),
    }
