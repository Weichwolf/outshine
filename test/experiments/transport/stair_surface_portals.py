"""Full-width old/new traces with actual positive area on opposite sides."""

from itertools import pairwise

import numpy as np
import shapely
from stair_module_trace import compare


def side(vertices, faces, line, sign, width):
    axis = (line[-1] - line[0]) / np.linalg.norm(line[-1] - line[0])
    normal = np.array([-axis[1], axis[0]])
    length = np.linalg.norm(line[-1] - line[0])
    corners = np.array(
        [[0, 0], [length, 0], [length, sign * width / 2], [0, sign * width / 2]]
    )
    half = shapely.Polygon(line[0] + corners @ np.array([axis, normal]))
    cuts = shapely.intersection(shapely.polygons(vertices[faces, :2]), half)
    xy, parents = shapely.get_coordinates(cuts, return_index=True)
    extent = np.zeros(len(faces))
    np.maximum.at(extent, parents, sign * ((xy - line[0]) @ normal))
    used = (shapely.area(cuts) > 0) & (extent > 1e-7)
    return [(vertices, faces[used], half)] if used.any() else []


def candidates(old, new, width):
    lines, segments = [], 0
    for ring in shapely.get_parts(old.boundary.simplify(1e-7, preserve_topology=False)):
        for a, b in pairwise(ring.coords):
            segments += 1
            if segments > 4096:
                raise ValueError("portal boundary segment budget exhausted")
            cut = shapely.LineString([a, b]).intersection(new.buffer(1e-7))
            for part in shapely.get_parts(cut):
                if part.geom_type == "LineString" and part.length >= width - 2e-7:
                    lines.append(np.array([part.coords[0], part.coords[-1]]))
                    if len(lines) > 128:
                        raise ValueError("portal candidate budget exhausted")
    lines.sort(key=lambda p: -np.linalg.norm(p[1] - p[0]))
    return lines, segments


def portal_checks(oldv, oldf, newv, newf, line, sign, width):
    checks = []
    for cast in (False, True):
        actual_line = line.astype("f4").astype(float) if cast else line
        groups = []
        for vertices, faces, direction in ((oldv, oldf, sign), (newv, newf, -sign)):
            actual = vertices.astype("f4").astype(float) if cast else vertices
            parts = side(actual, faces, actual_line, direction, width)
            if not parts:
                raise ValueError("actual portal side has no positive-area facets")
            groups.append([(vertices, used, mask) for _v, used, mask in parts])
        # Keep original coordinates here so trace_bounds pays their actual rounding.
        checks.append(compare(groups, line, cast))
    return checks


def portal(oldv, oldf, newv, newf, width):
    if (
        not np.isfinite(width)
        or not 0 < width <= 16
        or not 0 < len(oldf) <= 32768
        or not 0 < len(newf) <= 8192
    ):
        raise ValueError("bounded finite whole-width portal inputs required")
    old = shapely.union_all(shapely.polygons(oldv[oldf, :2]))
    new = shapely.union_all(shapely.polygons(newv[newf, :2]))
    lines, segments = candidates(old, new, width)
    tests, attempts = 0, 0
    for line in lines[:8]:
        for sign in (-1, 1):
            tests += 2 * (len(oldf) + len(newf))
            attempts += 1
            if tests > 65536:
                raise ValueError("portal facet intersection budget exhausted")
            try:
                checks = portal_checks(oldv, oldf, newv, newf, line, sign, width)
            except ValueError:
                continue
            return {
                "line_xy_m": line.tolist(),
                "length_m": float(np.linalg.norm(line[1] - line[0])),
                "old_side": sign,
                "checks": checks,
                "candidate_count": len(lines),
                "boundary_segments": segments,
                "attempts": attempts,
                "facet_intersections": tests,
                "reference": "different_old_and_new_owner_facets_on_opposite_sides",
            }
    raise ValueError("no complete opposite-side whole-width portal")


def inspect_contacts(data, rows, contacts):
    result = []
    for contact in contacts:
        owner, cap, mask = contact["owner"], contact["cap"], contact["mask"]
        faces = data["surface_faces"][data["owners"][:, owner]]
        result.append(
            {
                "source_id": contact["source_id"],
                "cached_source_id": contact["cached_source_id"],
                **contact["binding"],
                "mask_wkt": shapely.to_wkt(mask, rounding_precision=-1),
                "overlap_area_m2": float(mask.area),
                "portal": portal(
                    cap.vertices,
                    cap.triangles,
                    data["surface_vertices"],
                    faces,
                    rows[owner]["width_m"],
                ),
            }
        )
    return result
