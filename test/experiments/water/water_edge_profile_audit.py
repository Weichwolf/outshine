"""Independent complete-pair restriction check, without producer cuts or trees."""

import math

import numpy as np
import shapely
from water_field_difference import height, surface


def audit_profile(mesh, polylines, profile, *, pair_limit=1048576):
    if type(pair_limit) is not int or not 1 <= pair_limit <= 1048576:
        raise ValueError("bounded complete-pair profile audit required")
    triangles, gradient, polygons, _ = surface(mesh)
    addresses = profile.report["source_segment_addresses"]
    if len(addresses) != len(profile.segments):
        raise ValueError("profile source addresses incomplete")
    source_segments = []
    source_distance = roundoff_guard = 0.0
    for polyline_index, xy in enumerate(polylines):
        for local_index in range(len(xy) - 1):
            a, b = np.asarray(xy[local_index]), np.asarray(xy[local_index + 1])
            selected = [
                i
                for i, address in enumerate(addresses)
                if tuple(address) == (polyline_index, local_index)
            ]
            if not selected:
                raise ValueError("original border interval missing")
            edges = profile.points[profile.segments[selected], :2]
            distance = float(
                shapely.distance(
                    shapely.points(edges.reshape(-1, 2)), shapely.LineString([a, b])
                ).max()
            )
            guard = 64 * np.finfo(float).eps * max(1.0, float(abs(np.r_[a, b]).max()))
            source_distance, roundoff_guard = (
                max(source_distance, distance),
                max(roundoff_guard, guard),
            )
            if distance > guard:
                raise ValueError("profile moved away from its original border")
            direction = b - a
            parameter = ((edges[:, 0] - a) @ direction) / (direction @ direction)
            ordered = edges[np.argsort(parameter)]
            if not np.array_equal(ordered[0, 0], a) or not np.array_equal(
                ordered[-1, 1], b
            ):
                raise ValueError("profile does not cover original border endpoints")
            if not np.array_equal(ordered[:-1, 1], ordered[1:, 0]):
                raise ValueError("profile has an uncovered source interval")
            source_segments.extend(selected)
    if sorted(source_segments) != list(range(len(profile.segments))):
        raise ValueError("profile contains unowned intervals")
    pairs = len(profile.segments) * len(polygons)
    if pairs > pair_limit:
        raise ValueError("complete-pair profile audit budget exhausted")
    maximum = 0.0
    positive = 0
    for edge in profile.segments:
        points = profile.points[edge]
        a, b = points[:, :2]
        direction = b - a
        denominator = direction @ direction
        line = shapely.LineString([a, b])
        cuts = shapely.intersection(polygons, line)
        lengths = []
        for parent, cut in enumerate(cuts):
            if cut.is_empty or cut.length <= 0:
                continue
            if cut.geom_type != "LineString":
                raise ValueError("triangle-line audit is not one affine interval")
            xy = np.asarray(cut.coords)
            t = ((xy - a) @ direction) / denominator
            expected = points[0, 2] + t * (points[1, 2] - points[0, 2])
            actual = height(triangles, gradient, parent, xy)
            maximum = max(maximum, float(abs(expected - actual).max()))
            lengths.append(float(cut.length))
            positive += 1
        if not lengths or math.fsum(lengths) < line.length - 1e-9:
            raise ValueError("functional owner leaves a border interval uncovered")
    if maximum > 1e-9:
        raise ValueError("profile differs from the complete piecewise-affine owner")
    return {
        "all_triangle_segment_pairs": pairs,
        "positive_intervals": positive,
        "maximum_complete_edge_up_difference_m": maximum,
        "source_intervals_covered": len({tuple(a) for a in addresses}),
        "maximum_source_line_distance_m": source_distance,
        "coordinate_scale_roundoff_guard_m": roundoff_guard,
        "scope": "all root-triangle intersections plus original interval coverage in binary64; not a sample-only or Earth-source claim",
    }
