"""Project existing finite-actor route tubes into facade protection before extrusion."""

import math

import numpy as np
import shapely
from building_layout import rectangle
from building_plan import FLOOR_M


def routes(prepared, portals):
    mass = prepared.local
    entry = mass.entry
    centre, tangent = np.asarray(entry.centre), np.asarray(entry.tangent)
    outward = np.array([tangent[1], -tangent[0]])
    result = [
        (
            entry.identifier,
            shapely.LineString([centre + outward * 0.8, centre - outward * 1.5]),
            mass.bottom_m + FLOOR_M,
        ),
        (
            mass.identifier + "/approach",
            shapely.from_wkb(mass.core.approach_wkb),
            mass.bottom_m + FLOOR_M,
        ),
    ]
    for portal in portals:
        result.extend(
            (portal.identifier + "/" + name, shapely.from_wkb(wkb), portal.bottom_m)
            for name, wkb in (
                ("core", portal.core_route_wkb),
                ("deck", portal.path_wkb),
            )
        )
    return tuple(result)


def protection(paths, origin, tangent, region, outward_m):
    u0, z0, u1, z1 = region.bounds
    origin, tangent = np.asarray(origin), np.asarray(tangent)
    strip = rectangle(
        origin + tangent * ((u0 + u1) / 2), tangent, u1 - u0, outward_m, 0.005
    )
    parts = []
    factor = 1 / math.cos(math.pi / (4 * 32))
    for _identifier, path, floor in paths:
        if floor >= z1 or floor + 1.90 <= z0:
            continue
        for radius, low, high in (
            (0.10, floor, floor + 0.40),
            (0.30, floor + 0.40, floor + 1.90),
        ):
            overlap = path.buffer(radius * factor, quad_segs=32).intersection(strip)
            for part in shapely.get_parts(overlap):
                if part.is_empty or part.area <= 1e-12:
                    continue
                u = (shapely.get_coordinates(part) - origin) @ tangent
                parts.append(
                    shapely.box(
                        float(u.min()) - 0.02,
                        low - 0.02,
                        float(u.max()) + 0.02,
                        high + 0.02,
                    )
                )
    return shapely.union_all(parts)
