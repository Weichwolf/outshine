"""Bounded consecutive-edge roof repair using the SDK's closest-line criterion.

The criterion can move skew 3D edges. It is not a generic exact intersection
predicate and is kept experimental until roof emission contracts are verified.
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Repair:
    points: object
    faces: tuple
    changes: tuple
    checked_pairs: int


def crossing(a, b, c, d):
    first, second = b-a, d-c
    length, other = np.linalg.norm(first), np.linalg.norm(second)
    if length <= 1e-25 or other <= 1e-25:
        return None
    first, second = first/length, second/other
    normal = np.cross(first, second)
    denominator = normal@normal
    if denominator < 1e-8:
        return None
    station = np.cross(c-a, second)@normal/denominator
    if not 0. <= station < length:
        return None
    point = a+station*first
    if (point-c)@(point-c) >= other*other or (point-d)@(point-d) >= other*other:
        return None
    return point


def repair(points, faces, axis=2, max_nodes=4096):
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
        raise ValueError('Roof repair needs finite XYZ nodes')
    if type(axis) is not int or not 0 <= axis < 3 or type(max_nodes) is not int or max_nodes < len(points):
        raise ValueError('Invalid roof repair axis or node capacity')
    coordinates = [p.copy() for p in points]
    rings, changes, checked = [], [], 0
    for face_number, original in enumerate(faces):
        ring = list(original)
        if len(ring) < 3 or any(not isinstance(i, (int, np.integer)) or not 0 <= i < len(points) for i in ring):
            raise ValueError('Invalid roof polygon indices')
        peak = int(np.argmax(points[ring, axis]))
        step = 0
        while len(ring) > 3 and step < len(ring):
            first = (peak+step) % len(ring)
            indices = [(first+i) % len(ring) for i in range(4)]
            ids = [ring[i] for i in indices]
            checked += 1
            point = crossing(*(coordinates[i] for i in ids))
            if point is None:
                step += 1
                continue
            if len(coordinates) == max_nodes:
                raise ValueError('Roof contour repair node capacity exceeded')
            node = len(coordinates)
            coordinates.append(point)
            # Across the cyclic boundary, replacing the second middle vertex
            # retains the same start vertex as SDK Polygon::eraseIndex.
            replace_at, remove_at = (indices[1], indices[2]) if indices[2] else (indices[2], indices[1])
            ring[replace_at] = node
            del ring[remove_at]
            peak -= int(remove_at < first)
            changes.append((face_number, ids[1], ids[2], node))
        rings.append(tuple(ring))
    data = np.asarray(coordinates, dtype=float)
    immutable = np.frombuffer(data.tobytes(), dtype=float).reshape(-1, 3)
    return Repair(immutable, tuple(rings), tuple(changes), checked)


def used_points(points, faces):
    return np.asarray(points)[np.unique(np.concatenate([np.asarray(f, dtype=int) for f in faces]))]
