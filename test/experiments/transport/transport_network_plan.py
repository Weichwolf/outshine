"""A complete horizontal surface per physical tier, before height/recipe assignment.

Physical surface connectivity does not imply a traffic connection. Supplied tiers
and source identities remain separate; bridge/tunnel transitions need a 2.5D plan.
"""

from collections import defaultdict
import time

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from shapely.geometry import MultiPolygon

from geos_triangulation import triangles_of


def groups_of(roads):
    groups = defaultdict(list)
    for index, road in enumerate(roads):
        groups[road['tier']].append(index)
    return groups


def bands_of(roads, precision):
    lines = np.array([road['line'] for road in roads], dtype=object)
    width = np.array([road['width'] * .5 for road in roads])
    bands = shapely.buffer(lines, width, quad_segs=2, cap_style='round', join_style='bevel')
    bands = shapely.set_precision(shapely.set_precision(bands, precision), 0)
    if not np.all(shapely.is_valid(bands)) or np.any(shapely.is_empty(bands)):
        raise ValueError('invalid or empty source band')
    return bands


def global_plan(roads, bands):
    return {tier: shapely.union_all(bands[indices])
            for tier, indices in groups_of(roads).items()}


def clustered_plan(roads, bands):
    result, stats = {}, []
    for tier, indices in groups_of(roads).items():
        local = bands[indices]
        pairs = shapely.STRtree(local).query(local, predicate='intersects')
        pairs = pairs[:, pairs[0] < pairs[1]]
        graph = coo_matrix((np.ones(pairs.shape[1], dtype=np.uint8), pairs),
                           shape=(len(local), len(local))).tocsr()
        count, owners = connected_components(graph, directed=False)
        ordered = np.argsort(owners, kind='stable')
        cuts = np.r_[0, np.flatnonzero(np.diff(owners[ordered])) + 1, len(ordered)]
        merged = [shapely.union_all(local[ordered[first:last]])
                  for first, last in zip(cuts[:-1], cuts[1:])]
        polygons = [p for shape in merged for p in shapely.get_parts(shape)]
        result[tier] = MultiPolygon(polygons)
        stats.append(dict(tier=list(tier), bands=len(local), pairs=pairs.shape[1],
                          components=int(count), largest_component=int(np.bincount(owners).max())))
    return result, stats


def audit_plan(bands, plan, reference):
    errors = [plan[tier].symmetric_difference(reference[tier]).area for tier in plan]
    if not all(shape.is_valid for shape in plan.values()) or max(errors, default=0) > 1e-7:
        raise ValueError('clustered plan differs from the complete global reference')
    area = sum(shape.area for shape in plan.values())
    parts = [p for shape in plan.values() for p in shapely.get_parts(shape)]
    return dict(source_area_m2=float(shapely.area(bands).sum()), plan_area_m2=area,
                duplicate_surface_area_removed_m2=float(shapely.area(bands).sum()) - area,
                maximum_reference_difference_m2=max(errors, default=0),
                polygons=len(parts), preserved_holes=sum(len(p.interiors) for p in parts))


def boundary_segments(shape):
    xy, ring = shapely.get_coordinates(shapely.get_parts(shape.boundary), return_index=True)
    selected = ring[:-1] == ring[1:]
    return shapely.linestrings(np.stack([xy[:-1][selected], xy[1:][selected]], axis=1))


def mesh_of(plan):
    meshes, stats = {}, []
    for tier, shape in plan.items():
        polygons = triangles_of(shape)
        xy = shapely.get_coordinates(polygons).reshape(-1, 4, 2)[:, :3]
        vertices, indices = np.unique(xy.reshape(-1, 2), axis=0, return_inverse=True)
        indices = indices.reshape(-1, 3)
        edges = np.sort(np.r_[indices[:, [0, 1]], indices[:, [1, 2]], indices[:, [2, 0]]], axis=1)
        edges, counts = np.unique(edges, axis=0, return_counts=True)
        if np.any(counts > 2):
            raise ValueError(f'{tier}: non-manifold surface')
        boundary = shapely.points(vertices[edges[counts == 1]].mean(axis=1))
        segments = boundary_segments(shape)
        nearest = shapely.STRtree(segments).nearest(boundary)
        if np.any(shapely.distance(boundary, segments[nearest]) > 1e-7):
            raise ValueError(f'{tier}: crack or T-junction')
        area_error = abs(float(shapely.area(polygons).sum()) - shape.area)
        if area_error > max(1e-7, shape.area * 1e-12):
            raise ValueError(f'{tier}: triangle coverage differs from its surface')
        meshes[tier] = dict(vertices=vertices, indices=indices)
        stats.append(dict(tier=list(tier), vertices=len(vertices), triangles=len(indices),
                          boundary_edges=int(np.count_nonzero(counts == 1)),
                          mesh_area_error_m2=area_error,
                          bytes=vertices.nbytes + indices.nbytes))
    return meshes, stats


def measure(function, repeats):
    result, durations = None, []
    for _ in range(repeats):
        start = time.perf_counter()
        result = function()
        durations.append((time.perf_counter() - start) * 1000)
    return result, dict(minimum_ms=min(durations), median_ms=float(np.median(durations)),
                        maximum_ms=max(durations), repeats=repeats)


def verify():
    from shapely.geometry import LineString
    roads = [dict(line=LineString(xy), width=4., tier=tier) for xy, tier in (
        ([(-10, 0), (10, 0)], (0, 'ground')),
        ([(0, 0), (0, 10)], (0, 'ground')),
        ([(.1, 0), (.1, -10)], (0, 'ground')),
        ([(-10, -10), (10, 10)], (1, 'bridge')),
        ([(-10, -10), (-10, 10), (10, 10), (10, -10), (-10, -10)], (0, 'ground')))]
    bands = bands_of(roads, .001)
    plan, _ = clustered_plan(roads, bands)
    audit = audit_plan(bands, plan, global_plan(roads, bands))
    meshes, _ = mesh_of(plan)
    assert len(meshes) == 2 and audit['duplicate_surface_area_removed_m2'] > 0
    assert audit['preserved_holes'] > 0
    assert plan[(0, 'ground')].intersection(plan[(1, 'bridge')]).area > 0
