"""QSplat-inspired geometric preflight, not an attributed or native far product."""

from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree


def incident_radii(points, triangles):
    """Vertex balls cover each incident triangle; no new surface samples are made."""
    p = points[triangles]
    edges = np.roll(p, -1, axis=1) - p
    squared = np.sum(edges * edges, axis=2)
    longest = squared.max(1)
    cross = np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1)
    radius = np.sqrt(longest) / 2
    acute = (2 * longest < squared.sum(1)) & (cross > 0)
    radius[acute] = np.sqrt(np.prod(squared[acute], axis=1)) / (2 * cross[acute])
    # Zero-area source faces contribute no visible surface. Keep their addresses.
    radius = np.nextafter(radius * (1 + 16 * np.finfo(float).eps), np.inf)
    radius[cross == 0] = 0
    result = np.zeros(len(points))
    np.maximum.at(result, triangles.ravel(), np.repeat(radius, 3))
    return result, np.flatnonzero(cross == 0)


def merge_ball(a, ar, b, br):
    distance = float(np.linalg.norm(b - a))
    if ar >= distance + br:
        centre = a.copy()
    elif br >= distance + ar:
        centre = b.copy()
    else:
        radius = (distance + ar + br) / 2
        centre = a + (radius - ar) / distance * (b - a)
    radius = max(np.linalg.norm(centre - a) + ar, np.linalg.norm(centre - b) + br)
    # Conservative arithmetic padding, not a surface/weld tolerance.
    padding = 64 * np.finfo(float).eps * max(1.0, float(abs(centre).max()))
    return centre, float(np.nextafter(radius + padding, np.inf))


@dataclass(frozen=True)
class PointHierarchy:
    centres: np.ndarray
    radii: np.ndarray
    children: np.ndarray
    first: np.ndarray
    count: np.ndarray
    order: np.ndarray

    def arrays(self):
        return {key: getattr(self, key) for key in self.__dataclass_fields__}


def build(points, triangles):
    points, triangles = np.asarray(points, float), np.asarray(triangles)
    if (
        points.ndim != 2
        or points.shape[1] != 3
        or not len(points)
        or len(points) > 100000
        or len(triangles) > 200000
        or not np.isfinite(points).all()
        or triangles.ndim != 2
        or triangles.shape[1] != 3
        or triangles.dtype.kind not in "ui"
        or np.any(triangles < 0)
        or np.any(triangles >= len(points))
    ):
        raise ValueError(
            "bounded finite source points and valid original triangles required"
        )
    radii, degenerate = incident_radii(points, triangles)
    tree = cKDTree(points, leafsize=1, balanced_tree=True, compact_nodes=True)
    objects, children, first, counts = [], [], [], []
    pending = [(tree.tree, -1, 0)]
    while pending:
        node, parent, side = pending.pop()
        index = len(objects)
        objects.append(node)
        children.append([-1, -1])
        first.append(node.start_idx)
        counts.append(node.end_idx - node.start_idx)
        if parent >= 0:
            children[parent][side] = index
        if node.split_dim >= 0:
            pending.extend(((node.greater, index, 1), (node.lesser, index, 0)))
    centres, size = np.zeros((len(objects), 3)), np.zeros(len(objects))
    for i in range(len(objects) - 1, -1, -1):
        a, b = children[i]
        if a < 0:
            ids = tree.indices[first[i] : first[i] + counts[i]]
            if not np.all(points[ids] == points[ids[0]]):
                raise ValueError(
                    "leaf must contain only geometrically coincident points"
                )
            centres[i], size[i] = points[ids[0]], radii[ids].max()
        else:
            centres[i], size[i] = merge_ball(centres[a], size[a], centres[b], size[b])
    result = PointHierarchy(
        centres,
        size,
        np.asarray(children, np.int32),
        np.asarray(first, np.uint32),
        np.asarray(counts, np.uint32),
        tree.indices.astype(np.uint32),
    )
    leaves = result.children[:, 0] < 0
    return result, {
        "input_vertices": len(points),
        "input_triangles": len(triangles),
        "degenerate_original_faces": degenerate.tolist(),
        "nodes": len(size),
        "geometric_leaves": int(leaves.sum()),
        "coincident_extra_vertex_addresses": int(
            result.count[leaves].sum() - leaves.sum()
        ),
        "owned_geometry_array_bytes": sum(a.nbytes for a in result.arrays().values()),
    }


def cut(tree, view, *, diameter_px=1.0, node_budget=200000):
    if (
        not np.isfinite(diameter_px)
        or diameter_px <= 0
        or type(node_budget) is not int
        or not 0 <= node_budget <= 200000
    ):
        raise ValueError("positive support diameter and bounded node work required")
    eye, basis, focal, width, height = view
    eye, basis = np.asarray(eye), np.asarray(basis)
    if (
        eye.shape != (3,)
        or basis.shape != (3, 3)
        or not np.isfinite(eye).all()
        or not np.isfinite(basis).all()
        or not np.allclose(basis @ basis.T, np.eye(3), rtol=0, atol=1e-12)
        or not np.isfinite([focal, width, height]).all()
        or min(focal, width, height) <= 0
    ):
        raise ValueError("finite orthonormal camera and positive profile required")
    planes = np.array(
        [
            [0, 0, 1],
            [1, 0, width / (2 * focal)],
            [-1, 0, width / (2 * focal)],
            [0, 1, height / (2 * focal)],
            [0, -1, height / (2 * focal)],
        ]
    )
    lengths = np.linalg.norm(planes, axis=1)
    pending, selected, culled, unresolved, screens, lower = [0], [], [], [], [], []
    work = {"node_tests": 0, "frustum_plane_tests": 0, "projected_radius_tests": 0}
    while pending:
        node = pending.pop()
        if work["node_tests"] == node_budget:
            unresolved.append(node)
            continue
        work["node_tests"] += 1
        p = (tree.centres[node] - eye) @ basis.T
        r = tree.radii[node]
        distances = planes @ p
        distances[0] -= 0.1
        work["frustum_plane_tests"] += 5
        if np.any(distances < -r * lengths):
            culled.append(node)
            continue
        if p[2] > r + 0.1:
            radius = focal * r * np.sqrt(1 + np.sum((p[:2] / p[2]) ** 2)) / (p[2] - r)
        else:
            radius = np.inf
        work["projected_radius_tests"] += 1
        a, b = tree.children[node]
        if 2 * radius <= diameter_px or a < 0:
            selected.append(node)
            lower.append(
                2 * focal * r / np.sqrt(p[2] ** 2 - r**2) if p[2] > r else np.inf
            )
            xy = (
                np.array([width / 2, height / 2]) + focal * p[:2] / p[2] * [1, -1]
                if np.isfinite(radius)
                else np.full(2, np.nan)
            )
            screens.append([*xy, radius])
        else:
            pending.extend((int(b), int(a)))
    ids = np.asarray(selected, np.uint32)
    screen = np.asarray(screens, float).reshape(-1, 3)
    return {
        "selected": ids,
        "culled": np.asarray(culled, np.uint32),
        "unresolved": np.asarray(unresolved, np.uint32),
        "screen": screen,
        "minimum_sphere_diameter": np.asarray(lower),
    }, dict(
        work,
        splats=len(ids),
        collapsed_nodes=int(np.count_nonzero(tree.children[ids, 0] >= 0)),
        oversized_leaves=int(np.count_nonzero(2 * screen[:, 2] > diameter_px)),
        necessarily_oversized_leaves=int(
            np.count_nonzero(np.asarray(lower) > diameter_px)
        ),
        unprojectable_leaves=int(np.count_nonzero(~np.isfinite(screen).all(1))),
        unresolved_nodes=len(unresolved),
    )


def load(path):
    with np.load(path, allow_pickle=False) as data:
        return PointHierarchy(
            **{key: data[key] for key in PointHierarchy.__dataclass_fields__}
        )


def verify_partition(tree, rows):
    ids = np.concatenate([rows[k] for k in ("selected", "culled", "unresolved")])
    first, count = tree.first[ids], tree.count[ids]
    order = np.argsort(first)
    expected = np.r_[0, np.cumsum(count[order], dtype=np.uint64)]
    if not np.array_equal(first[order], expected[:-1]) or expected[-1] != len(
        tree.order
    ):
        raise ValueError("cut must cover every original vertex address exactly once")
    return int(expected[-1])
