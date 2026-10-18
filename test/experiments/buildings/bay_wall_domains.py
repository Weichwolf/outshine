"""Actual coplanar stepped wall domains and protected source opening footprints."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from bay_domain_contacts import entry_on_boundary, filter_traces
from bay_route_reserves import protection, routes
from building_plan import FLOOR_M

PLANE_TOL_M = 1e-8
CORNER_MARGIN_M = 0.26
OPENING_MARGIN_M = 0.12


@dataclass(frozen=True)
class WallDomain:
    identifier: str
    origin_xy: tuple
    tangent: tuple
    region_wkb: bytes
    protected_wkb: bytes
    windows: tuple
    wall_ids: tuple


def on_edge(opening, a, tangent, length):
    delta = np.asarray(opening.centre) - a
    normal = np.array([-tangent[1], tangent[0]])
    u = float(delta @ tangent)
    if (
        abs(float(delta @ normal)) > PLANE_TOL_M
        or np.linalg.norm(np.asarray(opening.tangent) - tangent) > 1e-8
        or u - opening.width_m / 2 < -PLANE_TOL_M
        or u + opening.width_m / 2 > length + PLANE_TOL_M
    ):
        raise ValueError("Existing opening does not belong to its actual wall edge")


def traces(prepared, portals):
    openings = {}
    for opening in (*prepared.facade.windows, *portals):
        openings.setdefault((opening.storey, *opening.edge), []).append(opening)
    result = []
    for index, storey in enumerate(prepared.local.storeys):
        polygon = shapely.orient_polygons(shapely.from_wkb(storey.footprint_wkb))
        for ring_index, ring in enumerate((polygon.exterior, *polygon.interiors)):
            for edge_index, (a, b) in enumerate(pairwise(np.asarray(ring.coords))):
                key = (index, ring_index, edge_index)
                if key not in openings:
                    continue
                length = float(np.linalg.norm(b - a))
                tangent = (b - a) / length
                for opening in openings.pop(key):
                    on_edge(opening, a, tangent, length)
                if length <= 2 * CORNER_MARGIN_M:
                    raise ValueError("Opening wall has no safe exterior corner margin")
                result.append((key, storey, a, tangent, length))
    if openings:
        raise ValueError("Opening references a missing source wall edge")
    if len(result) > 768:
        raise ValueError("Wall domain budget exceeded before coplanar grouping")
    return result


def protected(prepared, portals, origin, tangent):
    normal = np.array([-tangent[1], tangent[0]])
    parts = []
    for opening in (*prepared.facade.windows, *portals):
        relative = np.asarray(opening.centre) - origin
        if (
            abs(relative @ normal) > PLANE_TOL_M
            or np.linalg.norm(np.asarray(opening.tangent) - tangent) > 1e-8
        ):
            continue
        u = float(relative @ tangent)
        pad = OPENING_MARGIN_M
        parts.append(
            shapely.box(
                u - opening.width_m / 2 - pad,
                opening.bottom_m - pad,
                u + opening.width_m / 2 + pad,
                opening.bottom_m + opening.height_m + pad,
            )
        )
    entry = prepared.local.entry
    relative = np.asarray(entry.centre) - origin
    if (
        abs(relative @ normal) <= PLANE_TOL_M
        and np.linalg.norm(np.asarray(entry.tangent) - tangent) <= 1e-8
    ):
        u = float(relative @ tangent)
        low = prepared.local.bottom_m + FLOOR_M
        parts.append(
            shapely.box(
                u - 0.55 - OPENING_MARGIN_M,
                low - OPENING_MARGIN_M,
                u + 0.55 + OPENING_MARGIN_M,
                low + entry.height_m + OPENING_MARGIN_M,
            )
        )
    return shapely.union_all(parts)


def domains(prepared, *, portals=(), outward_m=0.20):
    entry_on_boundary(prepared.local, PLANE_TOL_M)
    actual, rejected = filter_traces(
        traces(prepared, portals), prepared.local, outward_m, CORNER_MARGIN_M
    )
    groups = []
    for trace in actual:
        _key, _storey, a, tangent, _length = trace
        normal = np.array([-tangent[1], tangent[0]])
        match = next(
            (
                g
                for g in groups
                if np.linalg.norm(g[0][3] - tangent) <= 1e-8
                and abs((a - g[0][2]) @ normal) <= PLANE_TOL_M
            ),
            None,
        )
        if match is None:
            groups.append([trace])
        else:
            match.append(trace)
    result = []
    paths = routes(prepared, portals)
    windows = prepared.facade.windows
    for group in groups:
        key, _storey, origin, tangent, _length = group[0]
        parts, keys = [], {item[0] for item in group}
        for _k, storey, a, _t, length in group:
            u = float((a - origin) @ tangent)
            parts.append(
                shapely.box(
                    u + CORNER_MARGIN_M,
                    storey.bottom_m,
                    u + length - CORNER_MARGIN_M,
                    storey.top_m,
                )
            )
        identifier = prepared.mass.identifier + "/wall-plane/" + "/".join(map(str, key))
        selected = tuple(
            sorted(
                (w for w in windows if (w.storey, *w.edge) in keys),
                key=lambda w: w.identifier,
            )
        )
        region = shapely.union_all(parts)
        reserved = protected(prepared, portals, origin, tangent).union(
            protection(paths, origin, tangent, region, outward_m)
        )
        result.append(
            WallDomain(
                identifier,
                tuple(origin),
                tuple(tangent),
                region.wkb,
                reserved.wkb,
                selected,
                tuple(f"{k[0]}/{k[1]}/{k[2]}" for k in sorted(keys)),
            )
        )
    return tuple(result), tuple(
        (prepared.mass.identifier + "/wall/" + "/".join(map(str, key)), reason)
        for key, reason in rejected
    )
