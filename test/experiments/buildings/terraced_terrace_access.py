"""Actual continuous support/body tubes from the retained stair front to each deck."""

import math
from itertools import pairwise

import numpy as np
import shapely
from entrance_clearance import section
from structural_shell_geometry import native
from terraced_facade_prepare import positions
from terraced_terrace_plan import GUARD_M


def world_path(plan, wkb):
    xy = np.asarray(shapely.from_wkb(wkb).coords)
    return shapely.LineString(
        positions(np.c_[xy, np.zeros(len(xy))], plan.prepared.mass.core)[:, :2]
    )


def selected_parts(owner, path, floor):
    asset = owner.facade.physical()
    parts = [
        dict(p.mesh, vertex_roots=p.vertex_roots)
        for p in (asset.floors.shell, asset.floors.stairs, asset.floors.landing)
    ]
    parts.extend(c.mesh for c in owner.components)
    mesh = asset.frames
    xyz = mesh["vertices"][mesh["faces"]]
    lo, hi = xyz.min(axis=1), xyz.max(axis=1)
    x0, y0, x1, y1 = path.buffer(0.31).bounds
    hit = (lo[:, 0] <= x1) & (hi[:, 0] >= x0) & (lo[:, 1] <= y1) & (hi[:, 1] >= y0)
    hit &= (lo[:, 2] <= floor + 1.90) & (hi[:, 2] >= floor - 1e-7)
    for part in np.unique(mesh["parts"][hit]):
        rows = mesh["parts"] == part
        parts.append({"vertices": mesh["vertices"], "faces": mesh["faces"][rows]})
    return [p for p in parts if overlaps(p, path, floor)]


def overlaps(mesh, path, floor):
    xyz = mesh["vertices"][mesh["faces"]].reshape(-1, 3)
    lo, hi = xyz.min(axis=0), xyz.max(axis=0)
    x0, y0, x1, y1 = path.buffer(0.31).bounds
    return (
        hi[2] >= floor - 1e-7
        and lo[2] <= floor + 1.90
        and not (hi[0] < x0 or lo[0] > x1 or hi[1] < y0 or lo[1] > y1)
    )


def continuous(parts, path, floor):
    factor = 1 / math.cos(math.pi / (4 * 32))
    torso, feet = (path.buffer(r * factor, quad_segs=32) for r in (0.30, 0.10))
    solids, heights = [], []
    for mesh in parts:
        xyz = mesh["vertices"][mesh["faces"]]
        cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        normals = cross / np.linalg.norm(cross, axis=1, keepdims=True)
        lo, hi = xyz.min(axis=1), xyz.max(axis=1)
        x0, y0, x1, y1 = torso.bounds
        near = (lo[:, 0] <= x1) & (hi[:, 0] >= x0) & (lo[:, 1] <= y1) & (hi[:, 1] >= y0)
        near &= (hi[:, 2] > floor + 1e-7) & (lo[:, 2] < floor + 1.90)
        for i in np.flatnonzero(near):
            if not shapely.MultiPoint(xyz[i, :, :2]).convex_hull.intersects(torso):
                continue
            if 1e-8 < abs(normals[i, 2]) < 1 - 1e-8:
                raise ValueError(
                    "Continuous terrace route has a nonprismatic nearby boundary"
                )
            heights.extend(xyz[i, :, 2])
        solids.append(native(mesh, mesh.get("vertex_roots")).scale((1000.0,) * 3))
    support = shapely.union_all([section(s, floor - 1e-7) for s in solids])
    if feet.difference(support).area > 1e-8:
        raise ValueError("Actual terrace route has missing continuous foot support")
    count = 0
    for low, high in pairwise(
        np.unique([*heights, floor + 1e-7, floor + 0.40, floor + 1.90])
    ):
        lo, hi = max(low, floor + 1e-7), min(high, floor + 1.90)
        if lo >= hi:
            continue
        region = feet if hi <= floor + 0.40 else torso
        occupied = shapely.union_all([section(s, (lo + hi) / 2) for s in solids])
        if region.intersection(occupied).area > 1e-8:
            raise ValueError("Actual terrace route obstructs the continuous actor tube")
        count += 1
    return {
        "length_m": path.length,
        "critical_slabs": count,
        "closed_components": len(parts),
    }


def check(owner):
    check_guards(owner)
    rows = []
    for p in owner.plan.portals:
        paths = [world_path(owner.plan, wkb) for wkb in (p.core_route_wkb, p.path_wkb)]
        rows.append(
            {
                "portal_id": p.identifier,
                "storey": p.storey,
                "routes": [
                    continuous(
                        selected_parts(owner, path, p.bottom_m), path, p.bottom_m
                    )
                    for path in paths
                ],
                "scope": "Continuous finite actor from the previously verified stair front, through the actual open portal onto the level deck; no native controller, full terrace roaming or highest-roof access proof.",
            }
        )
    return rows


def check_guards(owner):
    mass = owner.plan.prepared.mass
    for t in owner.plan.terraces:
        terrace = shapely.from_wkb(t.deck_wkb)
        lower = shapely.from_wkb(mass.storeys[t.storey - 1].footprint_wkb)
        expected = terrace.difference(lower.buffer(-GUARD_M, join_style="mitre"))
        guards = [
            c
            for c in owner.components
            if c.source.identifier == t.identifier + "/guard"
        ]
        actual = shapely.union_all(
            [
                section(native(c.mesh).scale((1000.0,) * 3), t.floor_m + 0.55)
                for c in guards
            ]
        )
        if actual.symmetric_difference(expected).area > 1e-7:
            raise ValueError(
                "Actual terrace edge protection differs from its original drop edges"
            )
