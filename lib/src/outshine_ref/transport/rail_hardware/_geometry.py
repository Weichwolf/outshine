"""Own simplified rail section and shared cuboid; dimensions are recipe metres."""

import numpy as np
import shapely

from outshine_ref.contracts import Mesh

SECTION = np.array(
    [
        [-0.075, 0],
        [0.075, 0],
        [0.075, 0.014],
        [0.012, 0.035],
        [0.00825, 0.121],
        [0.036, 0.136],
        [0.036, 0.172],
        [-0.036, 0.172],
        [-0.036, 0.136],
        [-0.00825, 0.121],
        [-0.012, 0.035],
        [-0.075, 0.014],
    ]
)
SECTION.setflags(write=False)
CAP = np.array(
    [
        [10, 11, 0],
        [7, 8, 9],
        [5, 6, 4],
        [1, 2, 3],
        [10, 0, 1],
        [7, 9, 6],
        [10, 1, 3],
        [9, 4, 6],
        [10, 3, 4],
        [4, 9, 10],
    ],
    dtype=np.uint32,
)
CAP.setflags(write=False)


def cuboid():
    vertices = np.array(
        [
            [-0.5, -0.5, 0],
            [0.5, -0.5, 0],
            [0.5, 0.5, 0],
            [-0.5, 0.5, 0],
            [-0.5, -0.5, 1],
            [0.5, -0.5, 1],
            [0.5, 0.5, 1],
            [-0.5, 0.5, 1],
        ]
    )
    triangles = np.array(
        [
            [0, 2, 1],
            [0, 3, 2],
            [4, 5, 6],
            [4, 6, 7],
            [0, 1, 5],
            [0, 5, 4],
            [1, 2, 6],
            [1, 6, 5],
            [2, 3, 7],
            [2, 7, 6],
            [3, 0, 4],
            [3, 4, 7],
        ],
        dtype=np.uint32,
    )
    return Mesh(vertices, triangles)


SLEEPER = cuboid()


def sweep_rails(xy, miter, datum, gauge, sleeper_height):
    count, section = len(xy), len(SECTION)
    # Gauge is the inner head-face distance, not rail-centre distance.
    offsets = np.array([-1.0, 1.0])[:, None, None] * (gauge + 0.072) / 2
    transverse = offsets + SECTION[None, None, :, 0]
    vertices = np.empty((2, count, section, 3))
    vertices[..., :2] = (
        xy[None, :, None, :] + transverse[..., None] * miter[None, :, None, :]
    )
    vertices[..., 2] = datum + sleeper_height + SECTION[:, 1]
    ring = np.arange((count - 1) * section, dtype=np.uint32).reshape(-1, section)
    following = ring[:, (np.arange(section) + 1) % section]
    sides = np.stack(
        (
            np.stack((ring, following, following + section), axis=-1),
            np.stack((ring, following + section, ring + section), axis=-1),
        ),
        axis=2,
    ).reshape(-1, 3)
    one = np.vstack((sides, CAP[:, ::-1], CAP + (count - 1) * section))
    faces = np.vstack((one, one + count * section))
    return Mesh(vertices.reshape(-1, 3), faces)


def sleeper_instances(xy, lengths, spacing, size, datum):
    stations = np.r_[0.0, np.cumsum(lengths)]
    total = stations[-1]
    count = int(np.floor((total - size[1]) / spacing)) + 1
    wanted = (total - (count - 1) * spacing) / 2 + spacing * np.arange(count)
    segment = np.searchsorted(stations, wanted, side="right") - 1
    segment = np.minimum(segment, len(lengths) - 1)
    tangent = np.diff(xy, axis=0)[segment] / lengths[segment, None]
    points = xy[segment] + tangent * (wanted - stations[segment])[:, None]
    transforms = np.zeros((count, 3, 4))
    transforms[:, :2, 0] = np.c_[-tangent[:, 1], tangent[:, 0]] * size[0]
    transforms[:, :2, 1] = -tangent * size[1]
    transforms[:, 2, 2] = size[2]
    transforms[:, :2, 3] = points
    transforms[:, 2, 3] = datum
    return transforms


def bake_rail_sleepers(transforms):
    transforms = np.asarray(transforms, dtype=float)
    if (
        transforms.ndim != 3
        or transforms.shape[1:] != (3, 4)
        or len(transforms) > 262144
        or not np.isfinite(transforms).all()
    ):
        raise ValueError("bounded finite Nx3x4 sleeper transforms required")
    if np.any(np.linalg.det(transforms[:, :, :3]) <= 1e-14):
        raise ValueError("positive nondegenerate sleeper transforms required")
    vertices = np.einsum("nij,vj->nvi", transforms[:, :, :3], SLEEPER.vertices)
    vertices += transforms[:, None, :, 3]
    faces = (
        SLEEPER.triangles[None]
        + 8 * np.arange(len(transforms), dtype=np.uint32)[:, None, None]
    )
    return Mesh(vertices.reshape(-1, 3), faces.reshape(-1, 3))


def footprints(xy, miter, gauge, transforms):
    centre = (gauge + 0.072) / 2
    polygons = []
    for offset in (-centre, centre):
        first = xy + (offset - 0.075) * miter
        second = xy + (offset + 0.075) * miter
        polygons.append(shapely.Polygon(np.vstack((first, second[::-1]))))
    bottom = np.einsum("nij,vj->nvi", transforms[:, :2, :3], SLEEPER.vertices[:4])
    bottom += transforms[:, None, :2, 3]
    sleepers = shapely.polygons(bottom)
    if not shapely.is_valid(polygons).all() or not shapely.is_valid(sleepers).all():
        return None, None, {"reason": "rail_footprint_self_intersection"}
    if polygons[0].intersection(polygons[1]).area > 1e-9:
        return None, None, {"reason": "rail_feet_intersect"}
    tree = shapely.STRtree(sleepers)
    candidates = 0
    keep_sleepers = np.ones(len(sleepers), dtype=bool)
    for begin in range(0, len(sleepers), 256):
        pairs = tree.query(sleepers[begin : begin + 256], predicate="intersects")
        candidates += pairs.shape[1]
        if candidates > 100000:
            return None, None, {"reason": "rail_sleeper_pair_budget"}
        first, second = begin + pairs[0], pairs[1]
        keep = first < second
        overlap = shapely.intersection(sleepers[first[keep]], sleepers[second[keep]])
        bad = shapely.area(overlap) > 1e-9
        for a, b in zip(first[keep][bad], second[keep][bad], strict=True):
            if b - a != 1:
                return None, None, {"reason": "rail_sleepers_intersect"}
            if keep_sleepers[a] and keep_sleepers[b]:
                keep_sleepers[b] = False
    gaps = np.diff(np.flatnonzero(keep_sleepers))
    if gaps.max(initial=0) > 2:
        return None, None, {"reason": "rail_sleeper_gap"}
    return (
        np.r_[np.array(polygons, object), sleepers[keep_sleepers]],
        keep_sleepers,
        {
            "sleeper_pair_candidates": candidates,
            "omitted_corner_sleepers": int(np.count_nonzero(~keep_sleepers)),
        },
    )
