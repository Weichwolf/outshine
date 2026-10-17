"""One indexed body including flat treads, vertical risers and continuous undersides."""

import numpy as np
from outshine_ref.contracts import Mesh
from stair_junction_treads import partition
from surface_solid_checks import audit_closed


def wall(vertices, lookup, a, b, c, d, first, second):
    def between(base, low, high):
        return sorted(
            (z, v) for (i, z), v in lookup.items() if i == base and low < z < high
        )

    at_first = between(first, vertices[c][2], vertices[b][2])
    at_second = between(second, vertices[d][2], vertices[a][2])
    rim = [
        a,
        b,
        *[v for _z, v in reversed(at_first)],
        c,
        d,
        *[v for _z, v in at_second],
    ]
    if len(rim) == 4:
        return [[a, b, c], [a, c, d]]
    centre = len(vertices)
    vertices.append(np.array([vertices[v] for v in rim]).mean(axis=0).tolist())
    return [[centre, u, v] for u, v in zip(rim, rim[1:] + rim[:1], strict=True)]


def close(points, faces, step, levels):
    vertices, lookup, top, bottom = [], {}, [], []

    def node(i, z):
        key = (int(i), float(z))
        if key not in lookup:
            lookup[key] = len(vertices)
            vertices.append([*points[i, :2], z])
        return lookup[key]

    for face, k in zip(faces, step, strict=True):
        top.append([node(i, points[i, 2] if k < 0 else levels[k]) for i in face])
        bottom.append([node(i, points[i, 2] - 0.3) for i in face])
    edges = {}
    for f, face in enumerate(faces):
        for i, j in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((int(face[i]), int(face[j]))))
            edges.setdefault(key, []).append((f, i, j))
    walls, families = [], []
    for records in edges.values():
        if len(records) == 1:
            f, i, j = records[0]
            a, b, c, d = top[f][j], top[f][i], bottom[f][i], bottom[f][j]
            triangles = wall(
                vertices, lookup, a, b, c, d, int(faces[f, i]), int(faces[f, j])
            )
            walls.extend(triangles)
            families.extend([0] * len(triangles))
        elif len(records) == 2:
            f, i, j = records[0]
            other, x, y = records[1]
            if faces[f, i] != faces[other, y] or faces[f, j] != faces[other, x]:
                raise ValueError("stair cap edge has inconsistent orientation")
            if top[f][i] == top[other][y] and top[f][j] == top[other][x]:
                continue
            if vertices[top[f][i]][2] < vertices[top[other][y]][2]:
                f, i, j, other, x, y = other, x, y, f, i, j
            a, b, c, d = top[f][j], top[f][i], top[other][y], top[other][x]
            triangles = wall(
                vertices, lookup, a, b, c, d, int(faces[f, i]), int(faces[f, j])
            )
            walls.extend(triangles)
            families.extend([1] * len(triangles))
        else:
            raise ValueError("stair cap has non-manifold edge ownership")
    triangles = np.vstack([top, np.array(bottom)[:, ::-1], walls])
    body = Mesh(np.array(vertices), triangles)
    volume = audit_closed(body)
    if volume <= 0:
        raise ValueError("stair body has nonpositive closed volume")
    return (
        body,
        np.array(top),
        np.array(bottom),
        np.array(walls),
        np.array(families),
        volume,
    )


def build(plan, heights):
    points, faces, owners, steps, levels, note = partition(plan, heights)
    body, top, bottom, walls, families, volume = close(points, faces, steps, levels)
    extra = {}
    if "stairs" in note:
        extra["stair_owners"] = np.array([r["owner"] for r in note["stairs"]])
        extra["stair_level_offsets"] = np.array(
            [r["level_offset"] for r in note["stairs"]] + [len(levels)]
        )
    return (
        body,
        {
            "body_vertices": body.vertices,
            "body_faces": body.triangles,
            "top_faces": top,
            "bottom_faces": bottom,
            "wall_faces": walls,
            "wall_riser": families,
            "face_sources": owners,
            "face_step": steps,
            "levels_m": levels,
            "reference_vertices": points,
            "reference_faces": faces,
            **extra,
        },
        dict(
            note,
            volume_m3=volume,
            vertices=len(body.vertices),
            faces=len(body.triangles),
            top_faces=len(top),
            riser_faces=int(families.sum()),
        ),
    )
