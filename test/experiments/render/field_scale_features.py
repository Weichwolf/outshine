"""Actual source extents and nearest footprint witnesses in a specified view."""

import numpy as np
import shapely
from field_scale_projection import project, quantiles
from shapely.affinity import affine_transform


def source_features(support, view):
    bounds, screen, footprints, floors = [], [], [], []
    for source in range(len(support.source_ids)):
        frame = np.linalg.inv(support.inverses[source])
        origin = support.origins[source]
        begin, end = support.offsets[source : source + 2]
        points, polygons, floor_points = [], [], []
        for domain_id in range(int(begin), int(end)):
            domain = support.domains[domain_id]
            plane, bottom = support.planes[domain_id], support.bottoms[domain_id]
            xy = shapely.get_coordinates(domain)
            floor = np.column_stack((xy, np.full(len(xy), bottom))) @ frame + origin
            roof = np.column_stack((xy, xy @ plane[:2] + plane[2])) @ frame + origin
            points.extend((floor, roof))
            floor_points.append(floor)
            shift = origin[:2] + bottom * frame[2, :2]
            polygons.append(
                affine_transform(
                    domain,
                    [frame[0, 0], frame[1, 0], frame[0, 1], frame[1, 1], *shift],
                )
            )
        world = np.concatenate(points)
        image = project(world, view)
        bounds.append(np.stack((world.min(0), world.max(0))))
        screen.append(np.stack((image.min(0), image.max(0))))
        footprints.append(shapely.union_all(polygons))
        floors.append(float(np.median(np.concatenate(floor_points)[:, 2])))
    gaps, pairs, metres, pixels = [], [], [], []
    nearest = shapely.STRtree(footprints).query_nearest(
        footprints, exclusive=True, all_matches=False
    )
    seen = set()
    zero = 0
    for a, b in zip(*nearest, strict=True):
        pair = tuple(sorted((int(a), int(b))))
        if pair in seen:
            continue
        seen.add(pair)
        line = shapely.shortest_line(footprints[a], footprints[b])
        if line.length <= 0:
            zero += 1
            continue
        xy = shapely.get_coordinates(line)
        points = np.column_stack((xy, np.full(2, max(floors[a], floors[b]) + 1)))
        gaps.append(points)
        pairs.append(support.source_ids[[a, b]])
        metres.append(line.length)
        pixels.append(float(np.linalg.norm(np.diff(project(points, view), axis=0))))
    return {
        "source_ids": support.source_ids,
        "source_world_bounds": np.asarray(bounds),
        "source_screen_bounds": np.asarray(screen),
        "gap_source_ids": np.asarray(pairs, np.uint32).reshape(-1, 2),
        "gap_witness_segments": np.asarray(gaps).reshape(-1, 2, 3),
        "gap_width_m": np.asarray(metres),
        "gap_width_px": np.asarray(pixels),
    }, {"nearest_unique_pairs": len(seen), "touching_or_overlapping_pairs": zero}


def feature_summary(features):
    spans = np.diff(features["source_screen_bounds"], axis=1)[:, 0]
    gap = features["gap_width_px"]
    largest = spans.max(1)
    return {
        "quantile_order": [0, 0.25, 0.5, 0.75, 0.9, 1],
        "source_count": len(spans),
        "source_x_span_px": quantiles(spans[:, 0]),
        "source_y_span_px": quantiles(spans[:, 1]),
        "source_max_axis_span_px": quantiles(largest),
        "source_max_axis_below_half_pixel": int((largest < 0.5).sum()),
        "source_max_axis_below_one_pixel": int((largest < 1).sum()),
        "positive_nearest_gap_count": len(gap),
        "nearest_gap_width_m": quantiles(features["gap_width_m"]),
        "nearest_gap_endpoint_separation_px": quantiles(gap),
        "nearest_gap_below_half_pixel": int((gap < 0.5).sum()),
        "nearest_gap_at_least_one_pixel": int((gap >= 1).sum()),
        "scope": "Source spans project actual coupled floor/roof vertices, before occlusion. Gap connectors are a subset of nearest source floor-footprint pairs, at the unchanged builder's common witness elevation max(median source floors)+1m. Their endpoint separation is not a street aperture/visible width, all gaps, a courtyard diameter, coverage or shadow proof.",
    }
