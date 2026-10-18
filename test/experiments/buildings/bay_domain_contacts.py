"""Conservative exterior boxes before any facade layer extrusion."""

from itertools import pairwise

import numpy as np
import shapely
from building_layout import rectangle


def filter_traces(traces, mass, outward_m, corner_margin_m):
    accepted, rejected, boxes = [], [], []
    polygons = [shapely.from_wkb(s.footprint_wkb) for s in mass.storeys]
    for trace in traces:
        key, storey, a, tangent, length = trace
        centre, width = a + tangent * (length / 2), length - 2 * corner_margin_m
        box = rectangle(centre, tangent, width, outward_m, 0.005)
        attachment = rectangle(centre, tangent, width, 0, 0.005)
        conflict = not polygons[key[0]].buffer(1e-8).covers(attachment)
        for index, other in enumerate(mass.storeys):
            if other.top_m <= storey.bottom_m or other.bottom_m >= storey.top_m:
                continue
            allowed = attachment if index == key[0] else shapely.Polygon()
            if box.difference(allowed).intersection(polygons[index]).area > 1e-8:
                conflict = True
                break
        if conflict:
            rejected.append((key, "exterior layer bounds intersect a source storey"))
        else:
            accepted.append(trace)
            boxes.append(box)
    overlaps = set()
    if boxes:
        pairs = shapely.STRtree(boxes).query(boxes, predicate="intersects").T
        for a, b in pairs:
            first, second = accepted[a][1], accepted[b][1]
            if a >= b or min(first.top_m, second.top_m) <= max(
                first.bottom_m, second.bottom_m
            ):
                continue
            if boxes[a].intersection(boxes[b]).area > 1e-8:
                overlaps.update((int(a), int(b)))
    result = []
    for index, trace in enumerate(accepted):
        if index in overlaps:
            rejected.append(
                (trace[0], "overlapping exterior layer bounds at a wall corner")
            )
        else:
            result.append(trace)
    return result, rejected


def entry_on_boundary(mass, tolerance):
    entry = mass.entry
    point, tangent = np.asarray(entry.centre), np.asarray(entry.tangent)
    polygon = shapely.orient_polygons(shapely.from_wkb(mass.storeys[0].footprint_wkb))
    for ring in (polygon.exterior, *polygon.interiors):
        xy = np.asarray(ring.coords)
        for a, b in pairwise(xy):
            length = np.linalg.norm(b - a)
            along = (b - a) / length
            relative = point - a
            normal = np.array([-along[1], along[0]])
            u = relative @ along
            if (
                abs(relative @ normal) <= tolerance
                and np.linalg.norm(tangent - along) <= 1e-8
                and u - 0.55 >= -tolerance
                and u + 0.55 <= length + tolerance
            ):
                return
    raise ValueError("Existing entry does not belong to an actual ground wall edge")
