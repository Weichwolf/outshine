"""Intentional retaining seams and a separate rejected excavation candidate."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from surface_solid_checks import audit_closed


def edges_of(faces):
    edges = {}
    for k, face in enumerate(faces):
        for i, j in zip(range(3), (1, 2, 0), strict=True):
            edges.setdefault(tuple(sorted((int(face[i]), int(face[j])))), []).append(
                (k, i, j)
            )
    if any(len(records) > 2 for records in edges.values()):
        raise ValueError("nonmanifold terrain partition")
    return edges


def split_roots(layout):
    xy, faces, kinds, targets, bed = (
        layout[k] for k in ("xy", "faces", "ground", "targets", "bed")
    )
    points, truth, contact, roots = xy.tolist(), targets.tolist(), bed.tolist(), {}
    for (i, j), records in edges_of(faces).items():
        if len(records) != 2 or kinds[records[0][0]] == kinds[records[1][0]]:
            continue
        a, b = bed[[i, j]] - targets[[i, j]]
        if a * b < 0:
            t = a / (a - b)
            roots[i, j] = len(points)
            points.append((xy[i] + t * (xy[j] - xy[i])).tolist())
            z = float(targets[i] + t * (targets[j] - targets[i]))
            truth.append(z)
            contact.append(z)
    result, owners = [], []
    for face, kind in zip(faces, kinds, strict=True):
        children = [tuple(map(int, face))]
        for a, b in zip(face, np.roll(face, -1), strict=True):
            key = tuple(sorted((int(a), int(b))))
            if key not in roots:
                continue
            index = next(i for i, f in enumerate(children) if a in f and b in f)
            opposite = next(v for v in children[index] if v not in (a, b))
            root = roots[key]
            children[index : index + 1] = [(a, root, opposite), (root, b, opposite)]
        result.extend(children)
        owners.extend([kind] * len(children))
    if len(result) > 32768 or len(points) > 16384:
        raise ValueError("root-split terrain geometry cap exhausted")
    return {
        "xy": np.array(points),
        "faces": np.array(result),
        "ground": np.array(owners),
        "targets": np.array(truth),
        "bed": np.array(contact),
        "root_count": np.array(len(roots)),
    }


def retaining_body(layout):
    xy, faces, kinds, targets, bed = (
        layout[k] for k in ("xy", "faces", "ground", "targets", "bed")
    )
    vertices, lookup = [], {}

    def node(i, z):
        key = (int(i), float(z))
        if key not in lookup:
            lookup[key] = len(vertices)
            vertices.append([*xy[i], z])
        return lookup[key]

    base = float(targets.min() - 5)
    top, bottom = [], []
    for face, kind in zip(faces, kinds, strict=True):
        heights = bed if kind else targets
        top.append([node(i, heights[i]) for i in face])
        bottom.append([node(i, base) for i in face])
    walls, internal = [], []
    for records in edges_of(faces).values():
        k, i, j = records[0]
        if len(records) == 1:
            rim = [top[k][j], top[k][i], bottom[k][i], bottom[k][j]]
            inside = False
        else:
            other, a, b = records[1]
            if top[k][i] == top[other][b] and top[k][j] == top[other][a]:
                continue
            delta = [
                vertices[top[k][u]][2] - vertices[top[other][v]][2]
                for u, v in ((i, b), (j, a))
            ]
            if min(delta) < -1e-8 and max(delta) > 1e-8:
                raise ValueError(
                    "crossing retaining seam requires its actual zero root"
                )
            first = sum(vertices[top[k][p]][2] for p in (i, j))
            second = sum(vertices[top[other][p]][2] for p in (a, b))
            if first < second:
                k, i, j, other, a, b = other, a, b, k, i, j
            rim = [top[k][j], top[k][i], top[other][b], top[other][a]]
            inside = True
        added = wall_triangles(
            vertices, lookup, rim, int(faces[k, i]), int(faces[k, j])
        )
        walls.extend(added)
        internal.extend([inside] * len(added))
    body = Mesh(np.array(vertices), np.vstack([top, np.array(bottom)[:, ::-1], walls]))
    if audit_closed(body) <= 0:
        raise ValueError("nonpositive terrain volume")
    return body, {
        "top_faces": np.array(top),
        "wall_faces": np.array(walls),
        "retaining_wall": np.array(internal),
        "ground": kinds,
    }


def wall_triangles(vertices, lookup, rim, first, second):
    a, b, c, d = rim

    def between(base, low, high):
        return sorted(
            (z, v) for (i, z), v in lookup.items() if i == base and low < z < high
        )

    left = between(first, vertices[c][2], vertices[b][2])
    right = between(second, vertices[d][2], vertices[a][2])
    ring = [a, b, *[v for _z, v in reversed(left)], c, d, *[v for _z, v in right]]
    ring = [n for p, n in enumerate(ring) if n != ring[p - 1]]
    if len(ring) == 3:
        return [ring]
    if len(ring) == 4:
        return [ring[:3], [ring[0], ring[2], ring[3]]]
    if len(ring) < 3:
        raise ValueError("degenerate retaining seam")
    centre = len(vertices)
    vertices.append(np.array([vertices[v] for v in ring]).mean(axis=0).tolist())
    return [[centre, a, b] for a, b in zip(ring, ring[1:] + ring[:1], strict=True)]


def manifold(mesh):
    import manifold3d as m

    result = m.Manifold(
        m.Mesh64(
            np.array(mesh.vertices, order="C", copy=True),
            np.array(mesh.triangles, dtype=np.uint64, order="C", copy=True),
        )
    )
    if result.status() != m.Error.NoError or result.is_empty():
        raise ValueError("unusable input solid: " + str(result.status()))
    return result


def protected_excavation(terrain, ground, upper):
    import manifold3d as m
    from foot_ramp_geometry import close_caps

    if len(upper.triangles) > 8192:
        raise ValueError("excavation cutter cap exhausted")
    indices, faces = np.unique(ground.triangles, return_inverse=True)
    faces = faces.reshape(-1, 3)
    top = ground.vertices[indices]
    bottom = top.copy()
    bottom[:, 2] = terrain.vertices[:, 2].min() - 1
    protected_mesh = close_caps(Mesh(top, faces), Mesh(bottom, faces))
    protected = manifold(protected_mesh)
    copied = protected_mesh.vertices.nbytes + protected_mesh.triangles.nbytes
    cutters = []
    for face in upper.triangles:
        triangle = upper.vertices[face]
        polygon = shapely.Polygon(triangle[:, :2]).buffer(0.02, join_style="mitre")
        xy = np.asarray(polygon.exterior.coords[:-1])
        if not polygon.exterior.is_ccw:
            xy = xy[::-1]
        gradient = np.linalg.solve(
            triangle[1:, :2] - triangle[:1, :2], triangle[1:, 2] - triangle[0, 2]
        )
        z = triangle[0, 2] + (xy - triangle[0, :2]) @ gradient - 0.1
        lower = np.c_[xy, z]
        count, triangles = len(xy), []
        for i in range(1, count - 1):
            triangles.extend([[count, count + i, count + i + 1], [0, i + 1, i]])
        for a in range(count):
            b = (a + 1) % count
            triangles.extend([[a, b, b + count], [a, b + count, a + count]])
        cutter = Mesh(np.vstack([lower, lower + [0, 0, 64]]), triangles)
        copied += cutter.vertices.nbytes + cutter.triangles.nbytes
        cutters.append(manifold(cutter))
    original = manifold(terrain)
    copied += terrain.vertices.nbytes + terrain.triangles.nbytes
    blade = m.Manifold.batch_boolean(cutters, m.OpType.Add) - protected
    result = (original - blade).simplify(0.00005)
    if result.status() != m.Error.NoError:
        raise ValueError("excavation failed: " + str(result.status()))
    double = result.to_mesh64()
    rebuilt = m.Manifold(result.to_mesh())
    if rebuilt.status() != m.Error.NoError:
        raise ValueError("Float32 topology reconstruction failed")
    exported = rebuilt.to_mesh64()
    return (
        Mesh(double.vert_properties[:, :3], double.tri_verts),
        Mesh(exported.vert_properties[:, :3], exported.tri_verts),
        {
            "cutters": len(cutters),
            "lateral_expansion_m": 0.02,
            "underside_offset_m": 0.1,
            "extrusion_m": 64,
            "simplification_m": 0.00005,
            "removed_volume_m3": original.volume() - result.volume(),
            "explicit_manifold_input_copy_bytes": copied,
            "status": "diagnostic_only; contact_and_visual_approval_missing",
        },
    )
