"""Independent normals-based crease measurements on the consumed reference mesh."""

from collections import defaultdict

import numpy as np


def path_creases(plan, heights):
    stairs = {s[0] for s in plan["stairs"]}
    result = []
    for owner, (row, faces) in enumerate(zip(plan["rows"], plan["parts"], strict=True)):
        if owner in stairs:
            continue
        points = np.c_[plan["xy"], heights][faces]
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        if np.any(normals[:, 2] <= 0) or not np.isfinite(normals).all():
            raise ValueError("finite positive projected path facets required")
        gradients = -normals[:, :2] / normals[:, 2:3]
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        edges = defaultdict(list)
        for index, face in enumerate(faces):
            for a, b in zip(face, np.roll(face, -1), strict=True):
                edges[tuple(sorted((int(a), int(b))))].append(index)
        jump, maximum, internal = 0.0, 0.0, 0
        for edge, incident in edges.items():
            if len(incident) > 2:
                raise ValueError("crease measurement needs manifold source caps")
            if len(incident) != 2:
                continue
            a, b = incident
            length = np.linalg.norm(plan["xy"][edge[1]] - plan["xy"][edge[0]])
            jump += length * np.linalg.norm(gradients[a] - gradients[b])
            maximum = max(
                maximum,
                float(np.rad2deg(np.arccos(np.clip(normals[a] @ normals[b], -1, 1)))),
            )
            internal += 1
        result.append(
            {
                "source_id": row["source_id"],
                "integrated_gradient_jump_m": float(jump),
                "maximum_dihedral_degrees": maximum,
                "internal_edges": internal,
            }
        )
    return result
