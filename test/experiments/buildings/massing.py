"""Bounded nested floor bands, preserving the supplied base, height interval and courtyards."""

import math

import numpy as np
import shapely


def canonical_ring(coordinates, positive):
    points = np.asarray(coordinates, dtype=np.float64)
    if np.array_equal(points[0], points[-1]):
        points = points[:-1]
    area = np.sum(points[:, 0]*np.roll(points[:, 1], -1)-points[:, 1]*np.roll(points[:, 0], -1))
    if (area > 0) != positive:
        points = points[::-1]
    first = min(range(len(points)), key=lambda i: tuple(points[i]))
    return np.roll(points, -first, axis=0)


def polygons_of(footprint):
    polygons = []
    for polygon in shapely.get_parts(footprint):
        rings = [canonical_ring(polygon.exterior.coords, True)]
        rings += sorted((canonical_ring(r.coords, False) for r in polygon.interiors), key=lambda r: tuple(r.flat))
        polygons.append(rings)
    return sorted(polygons, key=lambda rings: tuple(rings[0].flat))


def footprint_of(polygons):
    parts = [shapely.Polygon(rings[0], rings[1:]) for rings in polygons]
    return parts[0] if len(parts) == 1 else shapely.MultiPolygon(parts)


def stack(polygon, bottom, top, rows, style, mode, blocked=None):
    if mode not in ('plain', 'styled'):
        raise ValueError('unknown massing mode')
    floor = (top-bottom)/rows
    requested = min(1 if style == 'bauhaus' else 2, rows-2)
    reason = ('plain comparison requested' if mode == 'plain' else 'low horizontal house' if style == 'midcentury' else blocked
              or ('too few usable floors' if rows < 4 or floor < 1.8 else None))
    footprints, offsets = [polygon], [0.]
    step = min(2., .11*math.sqrt(polygon.area)) if style == 'bauhaus' else min(1.1, .07*math.sqrt(polygon.area))
    if reason is None:
        for tier in range(1, requested+1):
            candidate = polygon.buffer(-tier*step, join_style='mitre')
            usable = (not candidate.is_empty and candidate.is_valid and candidate.geom_type in ('Polygon', 'MultiPolygon')
                      and footprints[-1].covers(candidate) and footprints[-1].boundary.distance(candidate.boundary) > 1e-6)
            if usable:
                usable = all(part.area >= 16 and not part.buffer(-1.2, join_style='mitre').is_empty
                             for part in shapely.get_parts(candidate))
            if not usable:
                reason = 'setback leaves no strictly nested usable upper floor'
                break
            footprints.append(candidate)
            offsets.append(tier*step)
    layers, low = [], bottom
    count = len(footprints)
    for index, footprint in enumerate(footprints):
        floors = rows-count+1 if index == 0 else 1
        high = top if index == count-1 else bottom+(rows-count+1+index)*floor
        polygons = polygons_of(footprint)
        layers.append(dict(bottom_m=low, top_m=high, floors=floors, polygons=polygons,
                           footprint=footprint_of(polygons)))
        low = high
    return layers, dict(mode=mode, recipe=style+'-setback' if count > 1 else 'prism',
                        offsets_m=offsets, floors=rows, floor_height_m=floor, fallback_reason=reason)
