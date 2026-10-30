"""Bounded topological binary construction model, separate from original sources."""

import numpy as np
import shapely
from stair_seed_precision import GRID_M

BAND_M = 2 * GRID_M


def holes(shape):
    return sum(
        len(p.interiors) for p in shapely.get_parts(shape) if p.geom_type == "Polygon"
    )


def construct(requested):
    xy = shapely.get_coordinates(requested)
    if (
        not requested.is_valid
        or requested.geom_type not in ("Polygon", "MultiPolygon")
        or len(xy) > 8192
        or np.max(abs(xy), initial=0) >= 8190
    ):
        raise ValueError("Bounded valid local building construction domain required")
    actual = shapely.set_precision(requested, GRID_M)
    # Do not propagate GEOS's precision model to later physical predicates.
    actual = shapely.set_precision(actual, 0)
    if requested.is_empty != actual.is_empty:
        raise ValueError("Whole requested building collapses at construction precision")
    if (
        not actual.is_valid
        or actual.geom_type not in ("Polygon", "MultiPolygon")
        or len(shapely.get_parts(actual)) > 128
        or holes(actual) < holes(requested)
    ):
        raise ValueError(
            "Building construction changes a courtyard or bounded topology"
        )
    original_holes = [
        shapely.Polygon(ring)
        for part in shapely.get_parts(requested)
        for ring in part.interiors
    ]
    actual_holes = [
        shapely.Polygon(ring)
        for part in shapely.get_parts(actual)
        for ring in part.interiors
    ]
    matched = set()
    for hole in original_holes:
        matches = [
            i
            for i, candidate in enumerate(actual_holes)
            if candidate.covers(hole.representative_point())
        ]
        if len(matches) != 1 or matches[0] in matched:
            raise ValueError(
                "Building construction replaces or merges an original courtyard"
            )
        matched.add(matches[0])
    core = requested.buffer(-BAND_M, join_style="mitre")
    envelope = requested.buffer(BAND_M, join_style="mitre")
    if not core.difference(actual).is_empty or not actual.difference(envelope).is_empty:
        raise ValueError("Actual construction escapes its declared whole boundary band")
    points = shapely.get_coordinates(actual)
    if len(points) > 8192 or not np.array_equal(
        points, points.astype("f4").astype(float)
    ):
        raise ValueError("Binary construction vertices must be exactly Float32")
    return actual, {
        "model": "topological-binary-boundary/1",
        "grid_m": GRID_M,
        "boundary_band_m": BAND_M,
        "requested_parts": len(shapely.get_parts(requested)),
        "constructed_parts": len(shapely.get_parts(actual)),
        "requested_holes": holes(requested),
        "constructed_holes": holes(actual),
        "constructed_vertices": len(points),
        "omitted_requested_parts": [
            {
                "input_part": i,
                "reason": "below_binary_construction_precision",
                "footprint_wkb": part.wkb_hex,
            }
            for i, part in enumerate(shapely.get_parts(requested))
            if not shapely.dwithin(part, actual, BAND_M)
        ],
    }
