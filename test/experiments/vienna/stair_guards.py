"""Stone coping, square piers and open iron guards on exposed stair sides."""

from itertools import pairwise

import numpy as np
import shapely
from mesh_members import rectangular_member


def exposed_runs(side, exterior, stations):
    runs = []
    for i, (a, b) in enumerate(pairwise(side)):
        line = shapely.LineString([a, b])
        if stations[i + 1] - stations[i] < 1e-6:
            continue
        intervals = []
        for edge in shapely.get_parts(line.intersection(exterior)):
            if edge.geom_type == "LineString" and edge.length > 0.02:
                ends = shapely.get_point(edge, [0, -1])
                distances = shapely.line_locate_point(line, ends)
                intervals.append(tuple(stations[i] + np.sort(distances)))
        for lo, hi in sorted(intervals):
            if runs and lo - runs[-1][-1] < 1e-5:
                runs[-1].append(hi)
            else:
                runs.append([lo, hi])
    return runs


def draw_guard_run(meshes, knots, position, transverse, stations):
    start, end = knots[0], knots[-1]
    length = end - start
    if length < 0.42:
        return
    normal = lambda at: np.r_[
        [np.interp(at, stations, weights) for weights in transverse.T], 0.0
    ]
    at = lambda station: np.array(
        [np.interp(station, stations, coord) for coord in position.T]
    )
    for a, b in pairwise(knots):
        transverse_at = normal((a + b) / 2)
        for z, width, depth, material in (
            (0.045, 0.16, 0.29, "stair limestone"),
            (0.37, 0.031, 0.028, "painted iron"),
            (0.96, 0.064, 0.039, "painted iron"),
        ):
            rise = np.array([0.0, 0.0, z])
            rectangular_member(
                meshes,
                at(a) + rise,
                at(b) + rise,
                transverse_at,
                width,
                depth,
                material,
            )
    end_inset = 0.13
    posts = np.linspace(
        start + end_inset, end - end_inset, max(2, int(np.ceil(length / 1.7)) + 1)
    )
    for station in posts:
        p, transverse_at = at(station), normal(station)
        rectangular_member(
            meshes,
            p - [0, 0, 0.09],
            p + [0, 0, 1.02],
            transverse_at,
            0.20,
            0.22,
            "stair limestone",
        )
        rectangular_member(
            meshes,
            p + [0, 0, 1.01],
            p + [0, 0, 1.08],
            transverse_at,
            0.24,
            0.25,
            "stair limestone",
        )
    for lo, hi in pairwise(posts):
        balusters = np.linspace(lo, hi, max(2, int(np.ceil((hi - lo) / 0.17)) + 1))[
            1:-1
        ]
        for station in balusters:
            p = at(station)
            rectangular_member(
                meshes,
                p + [0, 0, 0.37],
                p + [0, 0, 0.96],
                normal(station),
                0.019,
                0.019,
                "painted iron",
            )


def draw_stair_guards(meshes, stairs, landings):
    if not stairs:
        return
    exterior = shapely.union_all([p.footprint for p in [*stairs, *landings]])
    exterior = exterior.boundary.buffer(2e-6, quad_segs=1)
    for stair in stairs:
        if abs(stair.heights[-1] - stair.heights[0]) < 0.50:
            continue
        z = np.r_[
            stair.heights[0],
            (stair.heights[:-1] + stair.heights[1:]) / 2,
            stair.heights[-1],
        ]
        for side_index in range(2):
            side = stair.sections[:, side_index]
            opposite = stair.sections[:, 1 - side_index]
            inward = opposite - side
            inward /= np.linalg.norm(inward, axis=1, keepdims=True)
            stations = np.r_[
                0, np.cumsum(np.linalg.norm(np.diff(side, axis=0), axis=1))
            ]
            position = np.c_[side + 0.14 * inward, z]
            for knots in exposed_runs(side, exterior, stations):
                draw_guard_run(meshes, knots, position, inward, stations)
