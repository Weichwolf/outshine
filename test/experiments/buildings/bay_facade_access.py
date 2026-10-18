"""Independent closed-mesh sections against existing continuous finite-actor routes."""

import math
from itertools import pairwise

import numpy as np
import shapely
from bay_route_reserves import routes
from clad_facade_mesh import surface
from entrance_clearance import section
from facade_frame_geometry import placed
from structural_shell_geometry import native
from terraced_facade_prepare import positions


def unobstructed(mesh, path, floor):
    xyz = mesh["vertices"][mesh["faces"]]
    lower, upper = xyz.min(axis=(0, 1)), xyz.max(axis=(0, 1))
    factor = 1 / math.cos(math.pi / (4 * 32))
    feet, torso = (path.buffer(r * factor, quad_segs=32) for r in (0.10, 0.30))
    if upper[2] <= floor or lower[2] >= floor + 1.90:
        return 0
    if not torso.intersects(shapely.box(lower[0], lower[1], upper[0], upper[1])):
        return 0
    solid = native(mesh).scale((1000.0,) * 3)
    heights = np.unique(
        [*xyz[:, :, 2].ravel(), floor + 1e-7, floor + 0.40, floor + 1.90]
    )
    count = 0
    for lo, hi in pairwise(heights):
        lo, hi = max(lo, floor + 1e-7), min(hi, floor + 1.90)
        if lo >= hi:
            continue
        actor = feet if hi <= floor + 0.40 else torso
        occupied = section(solid, (lo + hi) / 2)
        if occupied.intersection(actor).area > 1e-8:
            raise ValueError(
                "New facade layer obstructs an existing continuous actor route"
            )
        count += 1
    return count


def check_routes(planned, prepared, portals):
    paths = []
    core = prepared.mass.core
    for identifier, path, floor in routes(prepared, portals):
        xy = shapely.get_coordinates(path)
        world = positions(np.c_[xy, np.zeros(len(xy))], core)[:, :2]
        paths.append((identifier, shapely.LineString(world), floor))
    checked, slabs = 0, 0
    for cell in planned.cells:
        for layer in cell.layers:
            # Bounds precede the closed diagnostic expansion, independently of the planner.
            x0, z0, x1, z1 = layer.region.bounds
            tangent = np.asarray(layer.tangent)
            outward = np.array([tangent[1], -tangent[0]])
            corners = np.array(
                [
                    np.asarray(layer.origin_xy) + u * tangent + d * outward
                    for u in (x0, x1)
                    for d in (0, layer.depth_m)
                ]
            )
            bounds = shapely.box(*corners.min(axis=0), *corners.max(axis=0))
            selected = [
                (path, floor)
                for _id, path, floor in paths
                if floor < layer.base_m + z1
                and floor + 1.90 > layer.base_m + z0
                and path.buffer(0.301).intersects(bounds)
            ]
            if not selected:
                continue
            product = placed(surface(layer, "closed"), layer, layer.material)
            checked += 1
            for path, floor in selected:
                slabs += unobstructed(product, path, floor)
    return {
        "routes": len(paths),
        "closed_layers": checked,
        "critical_slabs": slabs,
        "scope": "Additional closed layers only; original support/body unchanged. Continuous existing route tubes, no arbitrary roaming/controller or Float32 proof.",
    }
