"""Bounded exact planar family areas; facet means are not a pixel-footprint filter."""

import time

import numpy as np
import shapely


def cells(domains, *, method="cells", max_cells=8192):
    if method not in ("cells", "whole") or not 1 <= len(domains) <= 16:
        raise ValueError("bounded known family decomposition required")
    output, labels, splits = [], [], 0
    queue = [
        (p, family, 0)
        for family, d in enumerate(domains)
        for p in shapely.get_parts(d)
        if not p.is_empty
    ]
    while queue:
        polygon, family, depth = queue.pop()
        if polygon.geom_type != "Polygon" or not polygon.is_valid:
            raise ValueError("valid polygonal family domain required")
        if len(output) + len(queue) >= max_cells:
            raise ValueError("surface family cell budget")
        if (
            method == "cells"
            and shapely.get_num_coordinates(polygon) > 256
            and depth < 20
        ):
            x0, y0, x1, y1 = polygon.bounds
            if x1 - x0 >= y1 - y0:
                mid = (x0 + x1) / 2
                boxes = [(x0, y0, mid, y1), (mid, y0, x1, y1)]
            else:
                mid = (y0 + y1) / 2
                boxes = [(x0, y0, x1, mid), (x0, mid, x1, y1)]
            for box in boxes:
                clipped = shapely.clip_by_rect(polygon, *box)
                queue.extend(
                    (p, family, depth + 1)
                    for p in shapely.get_parts(clipped)
                    if not p.is_empty
                )
            splits += 1
        else:
            output.append(polygon)
            labels.append(family)
    return (
        np.array(output, object),
        np.asarray(labels, int),
        {"cells": len(output), "splits": splits},
    )


def family_areas(triangles, domains, *, method="cells", seconds=30, max_pairs=1048576):
    if not 0 < seconds <= 120 or not 1 <= len(triangles) <= 262144:
        raise ValueError("bounded finite family area query required")
    began = time.perf_counter()
    polygons = shapely.polygons(triangles)
    area = shapely.area(polygons)
    if not np.isfinite(area).all() or np.any(area <= 0):
        raise ValueError("positive finite surface triangles required")
    shapes, labels, work = cells(domains, method=method)
    tree = shapely.STRtree(shapes)
    result = np.zeros((len(polygons), len(domains)), float)
    reason = np.zeros(len(polygons), np.uint8)
    pairs = 0
    for start in range(0, len(polygons), 256):
        if time.perf_counter() - began > seconds:
            reason[start:] = 1
            break
        query = polygons[start : start + 256]
        hit = tree.query(query, predicate="intersects")
        pairs += hit.shape[1]
        if pairs > max_pairs:
            reason[start:] = 2
            break
        intersections = shapely.intersection(query[hit[0]], shapes[hit[1]])
        amounts = shapely.area(intersections)
        np.add.at(result, (start + hit[0], labels[hit[1]]), amounts)
    total = result.sum(axis=1)
    bad = (area < 1e-9) | (total > area + np.maximum(1e-10, area * 1e-9))
    reason[(reason == 0) & bad] = 3
    result[reason != 0] = 0
    # Only roundoff excess is normalised; unresolved facets keep explicit unknown.
    excess = result.sum(axis=1) > area
    result[excess] *= (area[excess] / result[excess].sum(axis=1))[:, None]
    result[:, 0] = np.maximum(0, area - result.sum(axis=1))
    return (
        result,
        reason,
        dict(
            work,
            pairs=pairs,
            seconds=time.perf_counter() - began,
            unknown_area_m2=float(result[:, 0].sum()),
            unresolved_faces=int(np.count_nonzero(reason)),
            reason_counts={
                str(i): int(np.count_nonzero(reason == i)) for i in range(4)
            },
        ),
    )


def centroid_areas(triangles, domains):
    polygons = shapely.polygons(triangles)
    area = shapely.area(polygons)
    result = np.zeros((len(polygons), len(domains)), float)
    points = shapely.points(np.mean(triangles, axis=1))
    for family, domain in enumerate(domains):
        if family:
            result[shapely.covers(domain, points), family] = area[
                shapely.covers(domain, points)
            ]
    count = np.count_nonzero(result, axis=1)
    result[count != 1] = 0
    result[:, 0] = area - result.sum(axis=1)
    return result
