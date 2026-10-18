"""Partial Shell walls from original rings; actual native acceptance is separate."""

import math
from collections import Counter

import numpy as np
from building_positions import building_positions, millimetres


def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def hip_breaks(shape, p, q):
    """Literal HipBreaksAlong reference, retaining native tolerance semantics."""
    hip = shape["hip"]
    if not hip["available"]:
        raise ValueError("unavailable native hip break plan")
    origin = np.asarray(hip["origin"], dtype=np.float64)
    first, last = p - origin, q - origin
    ray = last - first
    length = math.hypot(*ray)
    if length == 0:
        return []
    tolerance = max(hip["precision_m"] / length, 64 * np.finfo(float).eps)
    cuts = []
    for facet in hip["facets"]:
        ring = np.asarray(facet["ring"], dtype=np.float64)
        for a, b in zip(ring, np.roll(ring, -1, axis=0), strict=True):
            direction = b - a
            divisor = cross(ray, direction)
            if divisor == 0:
                continue
            offset = a - first
            along = cross(offset, direction) / divisor
            across = cross(offset, ray) / divisor
            if (
                tolerance < along < 1 - tolerance
                and -tolerance <= across <= 1 + tolerance
                and math.isfinite(along)
            ):
                cuts.append(float(along))
    unique = []
    for value in sorted(cuts):
        if not unique or abs(value - unique[-1]) > tolerance:
            unique.append(value)
    return unique


def accepted_triangles(local):
    """Site::Tri position weld/cross filtering before Float32 placement."""
    snapped = millimetres(local)
    if not np.isfinite(snapped).all() or np.abs(snapped).max() > 1e7:
        raise ValueError("bounded finite wall coordinates required")
    retained = []
    for triangle in snapped:
        if len(set(map(tuple, triangle))) != 3:
            continue
        a, b = triangle[1] - triangle[0], triangle[2] - triangle[0]
        normal = (
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0],
        )
        if math.sqrt(sum(value * value for value in normal)) >= 1e-12:
            retained.append(triangle)
    return np.asarray(retained, dtype=np.float64).reshape(-1, 3, 3)


def shape_panels(shape):
    """Only Flat/Shed/Hip base walls; gables and other roofs remain excluded."""
    if shape["roof_kind"] not in (0, 2, 3):
        raise ValueError("unsupported native wall break recipe")
    low, top = shape["bottom_m"], shape["roof_deck_m"]
    if not math.isfinite(low + top) or low >= top:
        raise ValueError("positive finite wall height required")
    result, counts = [], Counter()
    for ring_index, original in enumerate([shape["ring"], *shape["holes"]]):
        ring = np.asarray(original, dtype=np.float64)
        if (
            ring.ndim != 2
            or ring.shape[1:] != (2,)
            or not 3 <= len(ring) <= 4096
            or not np.isfinite(ring).all()
        ):
            raise ValueError("bounded finite original wall ring required")
        for edge, (p, q) in enumerate(
            zip(ring, np.roll(ring, -1, axis=0), strict=True)
        ):
            if math.hypot(*(q - p)) < 0.05:
                counts["native_short_edges_skipped"] += 1
                continue
            breaks = hip_breaks(shape, p, q) if shape["roof_kind"] == 2 else []
            for piece, (start, end) in enumerate(
                zip([0.0, *breaks], [*breaks, 1.0], strict=True)
            ):
                a, b = p + (q - p) * start, p + (q - p) * end
                quad = np.array([[*a, low], [*b, low], [*b, top], [*a, top]])
                triangles = accepted_triangles(quad[[[0, 1, 2], [0, 2, 3]]])
                counts["candidate_triangles"] += 2
                counts["native_local_degenerate_triangles"] += 2 - len(triangles)
                if len(triangles):
                    result.append(
                        {
                            "ring": ring_index,
                            "edge": edge,
                            "piece": piece,
                            "local": triangles,
                        }
                    )
    return result, counts


def shell_wall_groups(source, call, *, retained=False):
    """Require a concrete successful selected Shell call, never Whole acceptance."""
    if (
        retained is not True
        or call.get("accepted") is not True
        or call.get("detail") != 1
        or call.get("openings") is not False
        or call.get("source") != source["index"]
    ):
        raise ValueError("accepted retained source-specific closed Shell required")
    surface = source["surface"]
    if surface is None:
        raise ValueError("unknown native source surface")
    result, counts = [], Counter()
    for shape_index, shape in enumerate(surface["shapes"]):
        try:
            panels, work = shape_panels(shape)
        except ValueError as error:
            counts[f"excluded_shape: {error}"] += 1
            continue
        counts.update(work)
        for panel in panels:
            local = panel.pop("local")
            result.append(
                panel
                | {
                    "shape": shape_index,
                    "triangles": building_positions(
                        local, surface, recipe="snapped-f32"
                    ),
                }
            )
    return result, counts
