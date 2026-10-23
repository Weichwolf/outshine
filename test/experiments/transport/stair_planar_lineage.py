"""Exact prepared planar correspondence, independent of source height variables."""

import numpy as np


def prepared_faces(plan):
    planar, coordinates = [], {}
    for i, point in enumerate(plan["xy"]):
        planar.append(coordinates.setdefault(tuple(point), i))
    canonical, reference = {}, {}
    for owner, (keys, faces) in enumerate(
        zip(plan["part_global_faces"], plan["parts"], strict=True)
    ):
        for parent, face in zip(keys, faces, strict=True):
            geometry = tuple(planar[int(v)] for v in face)
            key = int(parent), tuple(sorted(geometry))
            order = reference.setdefault(key, geometry)
            by_corner = dict(zip(geometry, map(int, face), strict=True))
            aligned = [by_corner[v] for v in order]
            height_key = tuple(sorted(aligned))
            old = canonical.get(height_key, (aligned, 0, int(parent)))
            if old[2] != int(parent):
                raise ValueError("shared height face has conflicting planar parents")
            canonical[height_key] = old[0], old[1] | (1 << owner), old[2]
    faces, masks, parents = map(list, zip(*canonical.values(), strict=True))
    return faces, masks, parents, planar


def refined_face_ids(faces, parents, planar):
    known, result = {}, []
    for face, parent in zip(faces, parents, strict=True):
        key = parent, tuple(sorted(planar[int(v)] for v in face))
        result.append(known.setdefault(key, len(known)))
    return np.array(result, dtype=int)


def overlap_faces(plan):
    result = []
    for keys, faces in zip(plan["part_global_faces"], plan["parts"], strict=True):
        mapped = {}
        for parent, face in zip(keys, faces, strict=True):
            points = plan["xy"][face]
            order = np.lexsort((points[:, 1], points[:, 0]))
            key = int(parent), tuple(map(tuple, points[order]))
            if key in mapped:
                raise ValueError("duplicate prepared planar subface within one source")
            mapped[key] = face[order]
        result.append(mapped)
    return result
