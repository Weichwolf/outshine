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


def extrusions(buildings, *, roofs=True, tolerance=0.0):
    """Preserve holes; simplify in metres; keep total top height for flat roofs."""
    triangles, colours = [], []
    for building in buildings:
        p = building.footprint.simplify(tolerance, preserve_topology=True)
        rise = min(building.roof_rise, building.height * .45) if roofs else 0.0
        parts, axes = roof_pieces(p, rise, building.roof_frame)
        boundary = p.boundary.buffer(1e-7)

        def top(xy):
            if axes is None:
                return np.full(len(xy), building.base + building.height)
            centre, axis, half_width = axes
            across = np.abs((xy - centre) @ axis) / half_width
            return building.base + building.height - rise * np.minimum(across, 1)

        for part in parts:
            faces = shapely.constrained_delaunay_triangles(part)
            for face in faces.geoms:
                xy = np.asarray(face.exterior.coords)[:3]
                triangles.append(np.column_stack((xy, top(xy))))
                colours.append(np.asarray(building.colour) * .72)
            for ring in [part.exterior, *part.interiors]:
                xy = np.asarray(ring.coords)
                for a, b in zip(xy[:-1], xy[1:]):
                    edge = LineString([a, b])
                    if not boundary.covers(edge):
                        continue
                    low = np.array([[*a, building.base], [*b, building.base]])
                    high = np.column_stack((np.array([a, b]), top(np.array([a, b]))))
                    triangles.extend([[low[0], low[1], high[1]],
                                      [low[0], high[1], high[0]]])
                    colours.extend([building.colour, building.colour])
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
    """Close with a 5 m radius, convex hull per connected block, 4 m DP.

    Area-weighted top and base follow #1958. This intentionally exposes tower loss:
    a 4 m footprint tolerance is not a bound on the resulting 3D height error.
    """
    if not buildings:
        return mesh([], [])
    footprints = [b.footprint for b in buildings]
    tree = shapely.STRtree(footprints)
    closed = shapely.union_all(footprints).buffer(5, join_style=2).buffer(-5, join_style=2)
    result = []
    for component in polygons(closed.intersection(box(*bounds))):
        indices = tree.query(component, predicate="intersects")
        members = [buildings[int(i)] for i in indices]
        if not members:
            continue
        footprint = component.convex_hull.simplify(4, preserve_topology=True)
        weights = [b.footprint.area for b in members]
        base = np.average([b.base for b in members], weights=weights)
        top = np.average([b.base + b.height for b in members], weights=weights)
        colour = np.average([b.colour for b in members], axis=0, weights=weights)
        result.append(Building(footprint, base, top - base, 0.0, tuple(colour)))
    return extrusions(result, roofs=False)


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
    height = np.zeros((cells, cells), np.float64)
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
        height[yy, xx] = np.maximum(height[yy, xx], np.where(area > 0, b.base + b.height, 0))
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
