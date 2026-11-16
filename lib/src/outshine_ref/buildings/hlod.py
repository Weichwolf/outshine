"""Camera-independent building tile recipes for the #1959 comparison.

Coordinates are metres, XY horizontal and Z up. Heights include the roof.
The caller supplies terrain seats and the fine roof recipe; no city rules live here.
"""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from shapely.geometry import LineString, Polygon, box
from shapely.ops import split


@dataclass(frozen=True)
class Building:
    footprint: Polygon
    base: float
    height: float
    roof_rise: float
    colour: tuple[float, float, float]
    roof_frame: tuple | None = None


@dataclass(frozen=True)
class Mesh:
    vertices: np.ndarray
    triangles: np.ndarray
    colours: np.ndarray
    buildings: np.ndarray


def mesh(triangles, colours, buildings=None):
    points = np.asarray(triangles, dtype=np.float32).reshape(-1, 3)
    vertices, indices = np.unique(points, axis=0, return_inverse=True)
    return Mesh(vertices, indices.reshape(-1, 3).astype(np.uint32),
                np.asarray(colours, dtype=np.float32).reshape(-1, 3),
                np.ones(len(colours), np.uint8) if buildings is None
                else np.asarray(buildings, dtype=np.uint8))


def combine(meshes):
    meshes = list(meshes)
    if not meshes:
        return mesh([], [])
    return mesh(np.concatenate([m.vertices[m.triangles] for m in meshes]),
                np.concatenate([m.colours for m in meshes]),
                np.concatenate([m.buildings for m in meshes]))


def polygons(geometry):
    if isinstance(geometry, Polygon):
        return [geometry] if geometry.area > 1e-8 else []
    return [p for g in getattr(geometry, "geoms", ()) for p in polygons(g)]


def roof_axis(polygon):
    corners = np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)[:4]
    edges = np.roll(corners, -1, axis=0) - corners
    short = edges[np.argmin(np.linalg.norm(edges, axis=1))]
    width = np.linalg.norm(short)
    return corners.mean(axis=0), short / width, width / 2


def roof_pieces(polygon, rise, frame=None):
    if rise <= 0:
        return [polygon], None
    centre, axis, half_width = roof_axis(polygon) if frame is None else frame
    along = np.array([-axis[1], axis[0]])
    reach = np.linalg.norm(np.ptp(np.asarray(polygon.exterior.coords), axis=0)) * 2
    ridge = LineString([centre - along * reach, centre + along * reach])
    return polygons(split(polygon, ridge)), (centre, axis, half_width)


def roof_height(building, xy, roofs=True):
    xy = np.asarray(xy)
    rise = min(building.roof_rise, building.height * .45) if roofs else 0.0
    if rise <= 0:
        return np.full(len(xy), building.base + building.height)
    centre, axis, half_width = building.roof_frame or roof_axis(building.footprint)
    across = np.abs((xy - centre) @ axis) / half_width
    return building.base + building.height - rise * np.minimum(across, 1)


def visible_wall(a, b, building, neighbours, roofs):
    """Subtract shared vertical intervals; preserve exposed steps and gables."""
    top = roof_height(building, np.array([a, b]), roofs)
    wall = Polygon([(0, building.base), (1, building.base), (1, top[1]), (0, top[0])])
    edge = LineString([a, b])
    normal = np.array([a[1] - b[1], b[0] - a[0]]) / edge.length
    if building.footprint.contains(shapely.Point((a + b) / 2 + normal * 1e-6)):
        normal = -normal
    for other in neighbours:
        shared = edge.intersection(other.footprint.boundary)
        lines = [shared] if isinstance(shared, LineString) else getattr(shared, "geoms", ())
        for line in lines:
            if not isinstance(line, LineString) or line.length < 1e-8:
                continue
            middle = np.asarray(line.interpolate(.5, normalized=True).coords)[0]
            if not other.footprint.contains(shapely.Point(middle + normal * 1e-6)):
                continue
            u0, u1 = sorted(edge.project(shapely.Point(p), normalized=True)
                            for p in (line.coords[0], line.coords[-1]))
            breaks = [u0, u1]
            centre, axis, _ = other.roof_frame or roof_axis(other.footprint)
            slope = float((b - a) @ axis)
            if slope != 0:
                ridge = float((centre - a) @ axis) / slope
                if u0 < ridge < u1:
                    breaks.insert(1, ridge)
            for left, right in zip(breaks[:-1], breaks[1:]):
                heights = roof_height(other, a + np.array([left, right])[:, None] * (b - a), roofs)
                hidden = Polygon([(left, other.base), (right, other.base),
                                  (right, heights[1]), (left, heights[0])])
                wall = wall.difference(hidden)
    triangles = []
    for part in polygons(wall):
        for face in shapely.constrained_delaunay_triangles(part).geoms:
            uz = np.asarray(face.exterior.coords)[:3]
            triangles.append(np.column_stack((a + uz[:, :1] * (b - a), uz[:, 1])))
    return triangles


def extrusions(buildings, *, roofs=True, tolerance=0.0, remove_shared=False):
    """Preserve holes; simplify in metres; keep total top height for flat roofs."""
    buildings = [replace(b, footprint=b.footprint.simplify(tolerance, preserve_topology=True),
                         roof_frame=b.roof_frame or roof_axis(b.footprint)) for b in buildings]
    tree = shapely.STRtree([b.footprint for b in buildings]) if remove_shared else None
    triangles, colours = [], []
    for index, building in enumerate(buildings):
        p = building.footprint
        rise = min(building.roof_rise, building.height * .45) if roofs else 0.0
        parts, axes = roof_pieces(p, rise, building.roof_frame)
        boundary = p.boundary.buffer(1e-7)

        for part in parts:
            faces = shapely.constrained_delaunay_triangles(part)
            for face in faces.geoms:
                xy = np.asarray(face.exterior.coords)[:3]
                triangles.append(np.column_stack((xy, roof_height(building, xy, roofs))))
                colours.append(np.asarray(building.colour) * .72)
            for ring in [part.exterior, *part.interiors]:
                xy = np.asarray(ring.coords)
                for a, b in zip(xy[:-1], xy[1:]):
                    edge = LineString([a, b])
                    if not boundary.covers(edge):
                        continue
                    neighbours = [] if tree is None else [buildings[int(i)] for i in
                        tree.query(edge, predicate="intersects") if int(i) != index]
                    faces = visible_wall(a, b, building, neighbours, roofs)
                    triangles.extend(faces)
                    colours.extend([building.colour] * len(faces))
    return mesh(triangles, colours)


def partition(buildings, bounds, divisions):
    """Clip, rather than assign centroids: a tile never loses a crossing building."""
    x0, y0, x1, y1 = bounds
    step = np.array([x1 - x0, y1 - y0]) / divisions
    result = []
    tree = shapely.STRtree([b.footprint for b in buildings])
    for y in range(divisions):
        for x in range(divisions):
            lo = np.array([x0, y0]) + step * (x, y)
            hi = lo + step
            boundary = box(*lo, *hi)
            members = [buildings[int(i)] for i in tree.query(boundary, predicate="intersects")]
            clipped = [replace(b, footprint=p,
                               roof_frame=b.roof_frame or roof_axis(b.footprint)) for b in members
                       for p in polygons(b.footprint.intersection(boundary))]
            result.append(((*lo, *hi), clipped))
    return result


def block_hulls(buildings, bounds):
    """Top-ASL groups span at most 4 m; towers retain their original body/roof.

    Tower rule matches the native form classifier: height >19 m and >2*span.
    Groups close with a 5 m mitre buffer; each connected block gets one convex
    hull, DP 4 m, area-weighted base/top/colour. Error must be measured afterwards.
    """
    result, groups = [], []
    for b in sorted(buildings, key=lambda b: b.base + b.height):
        _, axis, _ = b.roof_frame or roof_axis(b.footprint)
        along = np.array([-axis[1], axis[0]])
        span = np.ptp(np.asarray(b.footprint.exterior.coords) @ along)
        if b.height > 19 and b.height > 2 * span:
            result.append(b)
        elif not groups or b.base + b.height - (groups[-1][0].base + groups[-1][0].height) > 4:
            groups.append([b])
        else:
            groups[-1].append(b)
    for group in groups:
        footprints = [b.footprint for b in group]
        tree = shapely.STRtree(footprints)
        closed = shapely.union_all(footprints).buffer(5, join_style=2).buffer(-5, join_style=2)
        for component in polygons(closed.intersection(box(*bounds))):
            members = [group[int(i)] for i in tree.query(component, predicate="intersects")]
            if not members:
                continue
            weights = [b.footprint.area for b in members]
            base = float(np.average([b.base for b in members], weights=weights))
            top = float(np.average([b.base + b.height for b in members], weights=weights))
            colour = np.average([b.colour for b in members], axis=0, weights=weights)
            result.append(Building(component.convex_hull.simplify(4, preserve_topology=True),
                                   base, top - base, 0.0, tuple(colour)))
    return extrusions(result, remove_shared=True)


def triangle_keys(value):
    faces = value.vertices[value.triangles]
    order = np.lexsort((faces[:, :, 2], faces[:, :, 1], faces[:, :, 0]), axis=1)
    canonical = np.take_along_axis(faces, order[:, :, None], axis=1)
    return np.ascontiguousarray(canonical).reshape(-1, 9)


def directed_error(source, target, tree, shared, tolerance):
    """Adaptive centroid/radius bounds; only potentially maximal faces subdivide."""
    faces = source.vertices[source.triangles[~shared]].astype(np.float64)
    if not len(faces):
        return 0.0, 0.0
    vertices = target.vertices.astype(np.float64)
    indices = target.triangles.astype(np.int64)
    lower = float(np.sqrt(tree.squared_distance(vertices, indices, faces.reshape(-1, 3))[0]).max())
    upper = 0.0
    while len(faces):
        centres = faces.mean(axis=1)
        distances = np.sqrt(tree.squared_distance(vertices, indices, centres)[0])
        radii = np.linalg.norm(faces - centres[:, None], axis=2).max(axis=1)
        lower = max(lower, float(distances.max()))
        bounds = distances + radii
        active = bounds > lower + tolerance
        if (~active).any():
            upper = max(upper, float(bounds[~active].max()))
        faces = faces[active]
        if not len(faces):
            break
        edges = np.roll(faces, -1, axis=1) - faces
        longest = np.einsum("ijk,ijk->ij", edges, edges).argmax(axis=1)
        rows = np.arange(len(faces))
        a, b, c = (faces[rows, (longest + i) % 3] for i in range(3))
        midpoint = (a + b) / 2
        faces = np.concatenate((np.stack((a, midpoint, c), axis=1),
                                np.stack((midpoint, b, c), axis=1)))
    return lower, upper


def geometric_error(reference, variant, tolerance=.5):
    """Bound symmetric mesh Hausdorff distance to within 0.5 m using libigl AABBs.

    Exact common triangles need no query. Centroid-to-triangle distance plus the
    triangle covering radius bounds its entire surface; refine until the global
    upper/lower interval is <= tolerance. Only the upper endpoint is a LOD error.
    """
    import igl

    ka, kb = triangle_keys(reference), triangle_keys(variant)
    _, inverse = np.unique(np.concatenate((ka, kb)), axis=0, return_inverse=True)
    ia, ib = inverse[:len(ka)], inverse[len(ka):]
    a_shared, b_shared = np.isin(ia, ib), np.isin(ib, ia)
    if a_shared.all() and b_shared.all():
        return {"epsilon_m": 0.0, "sampled_max_m": 0.0, "uncertainty_m": 0.0}
    if not len(ia) or not len(ib):
        raise ValueError("both surfaces must contain geometry")
    tree_a, tree_b = igl.AABB(), igl.AABB()
    tree_a.init(reference.vertices.astype(np.float64), reference.triangles.astype(np.int64))
    tree_b.init(variant.vertices.astype(np.float64), variant.triangles.astype(np.int64))
    ab = directed_error(reference, variant, tree_b, a_shared, tolerance)
    ba = directed_error(variant, reference, tree_a, b_shared, tolerance)
    lower, upper = max(ab[0], ba[0]), max(ab[1], ba[1])
    return {"epsilon_m": upper + 1e-6, "sampled_max_m": lower,
            "uncertainty_m": upper - lower + 1e-6}


@dataclass(frozen=True)
class Bake:
    bounds: tuple[float, float, float, float]
    height: np.ndarray
    colour: np.ndarray
    coverage: np.ndarray


def bake(buildings, bounds, ground, cells=64):
    """Exact footprint/cell area, conservative cell top, linear area-mixed RGB.

    Ground is sampled at grid nodes. Building contributions use ground seats
    supplied by the caller. Overlaps resolve top-down, with coverage capped at one.
    """
    x0, y0, x1, y1 = bounds
    step = np.array([x1 - x0, y1 - y0]) / cells
    height = np.full((cells, cells), -np.inf, np.float64)
    coverage = np.zeros_like(height)
    colour = np.zeros((cells, cells, 3), np.float64)
    for b in sorted(buildings, key=lambda b: b.base + b.height, reverse=True):
        lo = np.maximum(0, np.floor((np.array(b.footprint.bounds[:2]) - (x0, y0)) / step).astype(int))
        hi = np.minimum(cells - 1, np.floor((np.array(b.footprint.bounds[2:]) - (x0, y0)) / step).astype(int))
        if np.any(hi < lo):
            continue
        xx, yy = np.meshgrid(np.arange(lo[0], hi[0] + 1), np.arange(lo[1], hi[1] + 1))
        left, bottom = x0 + xx * step[0], y0 + yy * step[1]
        areas = shapely.area(shapely.intersection(
            b.footprint, shapely.box(left, bottom, left + step[0], bottom + step[1])))
        area = np.clip(areas / np.prod(step), 0, 1)
        free = 1 - coverage[yy, xx]
        contribution = np.minimum(area, free)
        colour[yy, xx] += contribution[..., None] * np.array(b.colour) * .72
        coverage[yy, xx] += contribution
        height[yy, xx] = np.maximum(height[yy, xx], np.where(area > 0, b.base + b.height, -np.inf))
    colour += (1 - coverage[..., None]) * np.array([.12, .16, .09])
    nodes = np.asarray(ground, dtype=np.float64).copy()
    if nodes.shape != (cells + 1, cells + 1):
        raise ValueError("ground must cover every grid node")
    for dy, dx in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        target = nodes[dy:dy + cells, dx:dx + cells]
        np.maximum(target, height, out=target)
    return Bake(tuple(bounds), nodes.astype(np.float32), colour.astype(np.float32),
                coverage.astype(np.float32))


def remesh(baked, *, heights=None):
    """A fixed regular terrain lattice; no per-building geometry at far level."""
    cells = baked.colour.shape[0]
    x0, y0, x1, y1 = baked.bounds
    xx, yy = np.meshgrid(np.linspace(x0, x1, cells + 1), np.linspace(y0, y1, cells + 1))
    zz = baked.height if heights is None else heights
    vertices = np.column_stack((xx.ravel(), yy.ravel(), np.asarray(zz).ravel()))
    indices = np.arange((cells + 1) ** 2).reshape(cells + 1, cells + 1)
    a, b, c, d = (indices[:-1, :-1], indices[:-1, 1:],
                  indices[1:, 1:], indices[1:, :-1])
    triangles = np.stack((a, b, c, a, c, d), axis=-1).reshape(-1, 3)
    colours = np.repeat(baked.colour.reshape(-1, 3), 2, axis=0)
    flags = np.repeat(baked.coverage.ravel() > .25, 2).astype(np.uint8)
    return Mesh(vertices.astype(np.float32), triangles.astype(np.uint32), colours, flags)
