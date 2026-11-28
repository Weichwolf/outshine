"""Retain source vertices and clip route fragments to Web Mercator tile cores."""

from dataclasses import dataclass
from math import asinh, atan, degrees, pi, sinh, tan

from shapely import LineString, box, intersection

WORLD_CELLS = 1 << 46


@dataclass(frozen=True, slots=True)
class RouteVertex:
    point: tuple[int, int]
    observed: bool


@dataclass(frozen=True, slots=True)
class RouteFragment:
    tile: tuple[int, int, int]
    feature: int
    cell: float
    tags: tuple
    vertices: tuple[RouteVertex, ...]


def geography(point):
    x, y = point
    return (
        (x % WORLD_CELLS) / WORLD_CELLS * 360 - 180,
        degrees(atan(sinh(pi * (1 - 2 * y / WORLD_CELLS)))),
    )


def grid_point(longitude, latitude):
    return (
        round((longitude + 180) / 360 * WORLD_CELLS) % WORLD_CELLS,
        round((1 - asinh(tan(latitude * pi / 180)) / pi) / 2 * WORLD_CELLS),
    )


def _clip_lines(coordinates, extent, core):
    points = tuple(
        point
        for index, point in enumerate(coordinates)
        if not index or point != coordinates[index - 1]
    )
    if len(points) < 2:
        return
    inside = [0 <= x <= extent and 0 <= y <= extent for x, y in points]
    if all(inside):
        yield tuple((point, True) for point in points)
        return
    fragment = []
    for a, b, inside_a, inside_b in zip(
        points[:-1], points[1:], inside[:-1], inside[1:]
    ):
        if inside_a and inside_b:
            start, end = a, b
        else:
            clipped = intersection(LineString((a, b)), core)
            if clipped.geom_type != "LineString" or clipped.is_empty:
                if fragment:
                    yield tuple(fragment)
                    fragment = []
                continue
            start, end = clipped.coords
        if fragment and start != fragment[-1][0]:
            yield tuple(fragment)
            fragment = []
        if not fragment:
            fragment.append((start, start == a))
        fragment.append((end, end == b))
        if not inside_b:
            yield tuple(fragment)
            fragment = []
    if fragment:
        yield tuple(fragment)


def _source_lines(geometry):
    kind, coordinates = geometry["type"], geometry["coordinates"]
    if kind == "LineString":
        yield coordinates
    elif kind == "MultiLineString":
        yield from coordinates


def _tile_bounds(tile, stride, cell, bounds):
    _, x, y = tile
    west, north, east, south = bounds
    if east < west:
        if x * stride < WORLD_CELLS // 2:
            west -= WORLD_CELLS
        else:
            east += WORLD_CELLS
    return (
        (west - x * stride) / cell,
        (north - y * stride) / cell,
        (east - x * stride) / cell,
        (south - y * stride) / cell,
    )


def _overlaps(line, bounds):
    west, north, east, south = bounds
    xs, ys = zip(*line, strict=True)
    return max(xs) >= west and min(xs) <= east and max(ys) >= north and min(ys) <= south


def tile_fragments(tile, layer, bounds=None):
    """Decode line geometry, leaving transportation polygons to area recipes."""
    zoom, x, y = tile
    extent = layer["extent"]
    stride = WORLD_CELLS >> zoom
    cell = stride / extent
    core = box(0, 0, extent, extent)
    local = _tile_bounds(tile, stride, cell, bounds) if bounds is not None else None
    for feature in layer["features"]:
        tags = tuple(sorted(feature["properties"].items()))
        for line in _source_lines(feature["geometry"]):
            if len(line) < 2 or (local is not None and not _overlaps(line, local)):
                continue
            for segment in _clip_lines(tuple(map(tuple, line)), extent, core):
                vertices = tuple(
                    RouteVertex(
                        (x * stride + round(u * cell), y * stride + round(v * cell)),
                        observed,
                    )
                    for (u, v), observed in segment
                )
                if len({v.point for v in vertices}) < 2:
                    continue
                yield RouteFragment(tile, feature.get("id") or 0, cell, tags, vertices)
