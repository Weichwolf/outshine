"""Strict standard SFCGAL fallback without discarding original source-edge ownership."""

import numpy as np
import shapely


def partition(local, rings, node_limit):
    try:
        from pysfcgal.geometry import Geometry
    except ModuleNotFoundError as error:
        raise ModuleNotFoundError('Native skeleton fallback requires outshine-reference[roofs]') from error
    source = Geometry.from_wkt(shapely.to_wkt(local, rounding_precision=-1))
    output = source.straight_skeleton_partition()
    if output is None:
        raise ValueError('SFCGAL returned no skeleton partition')
    data = output.to_dict()
    if data['type'] != 'PolyhedralSurface':
        raise ValueError('SFCGAL returned a different partition geometry')
    original = np.concatenate(rings)
    tolerance = max(1., abs(original).max())*64*np.finfo(float).eps
    nodes, faces, lookup = [], [], {}
    for polygon in data['coordinates']:
        if len(polygon) != 1:
            raise ValueError('Native facet holes require an explicit topology contract')
        points = list(polygon[0])
        if points and points[0] == points[-1]:
            points = points[:-1]
        face = []
        for raw in points:
            point = np.asarray(raw, dtype=float)
            if point.shape != (2,) or not np.isfinite(point).all():
                raise ValueError('Native partition left the finite input plane')
            matched = np.flatnonzero(np.linalg.norm(original-point, axis=1) <= tolerance)
            if len(matched) > 1:
                raise ValueError('Native roundtrip cannot distinguish original vertices')
            key = tuple(original[matched[0]]) if len(matched) else tuple(point)
            if key not in lookup:
                if len(nodes) >= node_limit:
                    from .skeleton_plan import SkeletonBudgetExceeded
                    raise SkeletonBudgetExceeded('Native skeleton exceeds its node budget')
                lookup[key] = len(nodes); nodes.append(key)
            face.append(lookup[key])
        faces.append(tuple(face))
    return np.asarray(nodes), tuple(faces)
