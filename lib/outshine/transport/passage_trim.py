"""Place moulded stone on the actual wall around an existing passage aperture."""

import numpy as np

from outshine.buildings.building_passages import (
    JOIN_TOLERANCE_M,
    PORTAL_OVERLAP_M,
    PORTAL_WIDTH_M,
    passage_axis,
    vault_profile,
)

TRIM_WIDTH_M = 0.22
TRIM_DEPTH_M = 0.105
PLINTH_WIDTH_M = 0.32
PLINTH_HEIGHT_M = 0.30
PLINTH_CAP_M = 0.07
PLINTH_PROJECTION_M = 0.055
BEVEL_M = 0.027
FOOT_EMBED_M = 0.15


def facade_edge(polygon, endpoint, outward, across):
    reach = PORTAL_WIDTH_M / 2 + PLINTH_WIDTH_M
    segments = [
        np.stack((xy[:-1], xy[1:]), axis=1)
        for ring in (polygon.exterior, *polygon.interiors)
        if len(xy := np.asarray(ring.coords)) > 1
    ]
    starts, ends = np.concatenate(segments).transpose(1, 0, 2)
    spans = ends - starts
    origins = starts - endpoint
    u = origins @ across
    width = spans @ across
    facing = spans[:, 1] * outward[0] - spans[:, 0] * outward[1]
    first = np.minimum(u, u + width)
    last = np.maximum(u, u + width)
    valid = (
        (first <= -reach)
        & (last >= reach)
        & (facing > 0.7 * np.linalg.norm(spans, axis=1))
    )
    candidates = np.flatnonzero(valid)
    offsets = -u[candidates] / width[candidates]
    mids = starts[candidates] + offsets[:, None] * spans[candidates]
    distance = np.linalg.norm(mids - endpoint, axis=1)
    if not len(distance):
        return
    closest = np.argmin(distance)
    if distance[closest] <= PORTAL_OVERLAP_M + JOIN_TOLERANCE_M:
        edge = candidates[closest]
        return mids[closest], spans[edge] / width[edge]


def stone_surround(meshes, mid, horizontal, outward, crown, height):
    inner, radial = vault_profile(crown)
    outer = inner + radial * TRIM_WIDTH_M
    spring = inner[[0, -1]]
    foot_edges = spring[:, 0] + np.array([-PLINTH_WIDTH_M, PLINTH_WIDTH_M])
    sides = np.stack((spring[:, 0], foot_edges), axis=1)
    grades = height((mid + sides[..., None] * horizontal).reshape(-1, 2)).reshape(2, 2)
    rims = np.minimum(grades.max(axis=1) + PLINTH_HEIGHT_M, spring[:, 1] - 0.35)
    feet = np.minimum(grades.min(axis=1) - FOOT_EMBED_M, rims - PLINTH_HEIGHT_M)
    levels = np.column_stack((feet, rims, rims + PLINTH_CAP_M))
    inner = np.vstack(
        (
            np.column_stack((np.repeat(inner[0, 0], 3), levels[0])),
            inner,
            np.column_stack((np.repeat(inner[-1, 0], 3), levels[1, ::-1])),
        )
    )
    outer = np.vstack(
        (
            np.column_stack(([foot_edges[0], foot_edges[0], outer[0, 0]], levels[0])),
            outer,
            np.column_stack(
                ([outer[-1, 0], foot_edges[1], foot_edges[1]], levels[1, ::-1])
            ),
        )
    )
    bevel = (outer - inner) * (BEVEL_M / TRIM_WIDTH_M)
    profile = np.stack((inner, inner + bevel, outer - bevel, outer), axis=1)
    depth = np.broadcast_to(
        [-0.01, TRIM_DEPTH_M, TRIM_DEPTH_M, -0.01], profile.shape[:2]
    ).copy()
    depth[np.ix_([0, 1, -2, -1], [1, 2])] += PLINTH_PROJECTION_M
    xy = mid + profile[..., :1] * horizontal + depth[..., None] * outward
    vertices = np.concatenate((xy, profile[..., 1:]), axis=2).reshape(-1, 3)

    strip = np.arange(len(inner) - 1)[:, None] * 4
    edge = np.arange(4)
    next_edge = (edge + 1) % 4
    quads = strip[..., None] + np.stack(
        (edge, edge + 4, next_edge + 4, next_edge), axis=1
    )
    end = len(vertices) - 4
    quads = np.vstack(
        (quads.reshape(-1, 4), [0, 1, 2, 3], end + np.array([3, 2, 1, 0]))
    )
    faces = quads[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    front = vertices[quads[1, :3]]
    if np.cross(front[1] - front[0], front[2] - front[0])[:2] @ outward < 0:
        faces = faces[:, ::-1]
    meshes.add("portal stone", vertices, faces)


def draw_passage_trim(meshes, polygon, passage, height):
    frame = passage_axis(passage)
    if frame is None:
        return
    origin, end, along, across = frame
    for point, outward in ((origin, -along), (end, along)):
        if np.linalg.norm(point) > 85:
            continue
        wall = facade_edge(polygon, point, outward, across)
        if wall is not None:
            mid, horizontal = wall
            stone_surround(meshes, mid, horizontal, outward, passage.crown_m, height)
