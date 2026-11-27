"""Mapped waterfronts and building shells where the relief grid is visible at eye resolution."""

import numpy as np
import shapely
from outshine_ref.buildings.structures import suppressed
from shapely.geometry import box, shape


def metric_polygons(group, nw, se, key):
    shapes, values = [], []
    for f in group.get("features", []):
        prop = f["properties"]
        value = key(prop)
        if value is not None and f["geometry"]["type"] in ("Polygon", "MultiPolygon"):
            shapes.append(shape(f["geometry"]))
            values.append(value)
    if not shapes:
        return np.empty(0, dtype=object), np.empty(0)
    geoms = shapely.transform(
        np.asarray(shapes, dtype=object),
        lambda xy: nw + xy / group["extent"] * (se - nw),
    )
    return geoms, np.asarray(values)


def true_shore(xy, land, data, nw, se, radius):
    wet = land == 3
    if not wet.any() or wet.all():
        return
    polys, _ = metric_polygons(data.get("water", {}), nw, se, lambda p: 1)
    if not len(polys):
        return
    line = shapely.union_all(shapely.make_valid(polys)).boundary
    internal = xy[1:-1, 1:-1]
    count = wet[:-1, :-1].astype(np.int8) + wet[1:, :-1] + wet[1:, 1:] + wet[:-1, 1:]
    use = (
        (count > 0)
        & (count < 4)
        & np.all(np.abs(np.abs(internal) - radius) > 0.01, axis=2)
    )
    pts = internal[use]
    if not len(pts):
        return
    snap = shapely.shortest_line(shapely.points(pts), line)
    nearest = shapely.get_coordinates(shapely.get_point(snap, -1))
    step = abs(xy[0, -1, 0] - xy[0, 0, 0]) / land.shape[1]
    within = np.linalg.norm(nearest - pts, axis=1) < 1.6 * step
    pts[within] = nearest[within]
    internal[use] = pts


def building_shells(data, nw, se, source, dem, radius, focal):
    group = data.get("building", {})

    def interval(p):
        high = p.get("render_height", 0)
        low = p.get("render_min_height", 0)
        return (low, high) if high > low and not suppressed(p) else None

    geoms, heights = metric_polygons(group, nw, se, interval)
    if not len(geoms):
        return []
    anchors = shapely.point_on_surface(geoms)
    xy = shapely.get_coordinates(anchors)
    lon, lat = (xy / source.metres + [source.lon, source.lat]).T
    base = (
        dem.sample(lat, lon) - source.datum - np.sum(xy * xy, axis=1) / (2 * 6371000.0)
    )
    # Tile buffers contain duplicate source footprints. Each tile owns its interior.
    domain = box(nw[0], se[1], se[0], nw[1]).difference(
        box(-radius, -radius, radius, radius)
    )
    geoms = shapely.make_valid(geoms)
    partial = ~shapely.covered_by(geoms, domain)
    geoms[partial] = shapely.intersection(geoms[partial], domain)
    polys, index = shapely.get_parts(geoms, return_index=True)
    valid = (shapely.get_type_id(polys) == 3) & ~shapely.is_empty(polys)
    polys = shapely.orient_polygons(polys[valid])
    index = index[valid]
    if not len(polys):
        return []
    low = base[index] + heights[index, 0]
    high = base[index] + heights[index, 1]
    # Batches preserve courtyards, recessed corners and actual roof silhouettes.
    tris, parent = shapely.get_parts(
        shapely.constrained_delaunay_triangles(polys), return_index=True
    )
    coords = shapely.get_coordinates(shapely.orient_polygons(tris)).reshape(-1, 4, 2)[
        :, :3
    ]
    roofs = np.dstack(
        (coords, np.broadcast_to(high[parent, None], coords.shape[:2]))
    ).reshape(-1, 3)
    rings, parent = shapely.get_rings(polys, return_index=True)
    pts, owner = shapely.get_coordinates(rings, return_index=True)
    edge = np.flatnonzero(owner[:-1] == owner[1:])
    a, b = pts[edge], pts[edge + 1]
    parent = parent[owner[edge]]
    walls = np.stack(
        (
            np.c_[a, low[parent]],
            np.c_[b, low[parent]],
            np.c_[b, high[parent]],
            np.c_[a, high[parent]],
        ),
        axis=1,
    ).reshape(-1, 3)
    products = [
        ("regional roofs", roofs, np.arange(len(roofs)).reshape(-1, 3)),
        ("regional walls", walls, np.arange(len(walls)).reshape(-1, 4)),
    ]
    windows = visible_windows(
        a,
        b,
        low[parent],
        high[parent],
        shapely.area(polys)[parent],
        (high - low)[parent],
        focal,
    )
    if windows is not None:
        products.append(windows)
    return products


def visible_windows(a, b, bottom, top, area, footprint_height, focal):
    delta = b - a
    length = np.linalg.norm(delta, axis=1)
    outward = np.column_stack((delta[:, 1], -delta[:, 0])) / np.maximum(
        length[:, None], 1e-6
    )
    middle = (a + b) * 0.5
    distance = np.maximum(np.linalg.norm(middle, axis=1), 1.0)
    facing = -np.sum(outward * middle, axis=1) / distance
    h = top - bottom
    use = (
        (length > 2.5)
        & (h > 2.8)
        & (1.6 * focal * facing / distance > 1.15)
        & (area > 45)
        & (footprint_height < 4.5 * np.sqrt(area))
    )
    if not use.any():
        return None
    a, delta, outward, h, bottom = a[use], delta[use], outward[use], h[use], bottom[use]
    cols = np.maximum(1, np.rint(length[use] / 3.4).astype(int))
    floors = np.maximum(1, np.rint(h / 3.2).astype(int))
    counts = cols * floors
    owner = np.repeat(np.arange(len(counts)), counts)
    i = np.arange(len(owner)) - np.repeat(np.cumsum(counts) - counts, counts)
    u = (i % cols[owner] + 0.5) / cols[owner]
    start = a[owner] + delta[owner] * u[:, None] + outward[owner] * 0.035
    half = delta[owner] * (0.235 / cols[owner, None])
    mid = bottom[owner] + (i // cols[owner] + 0.5) * h[owner] / floors[owner]
    dz = 0.285 * h[owner] / floors[owner]
    verts = np.stack(
        (
            np.c_[start - half, mid - dz],
            np.c_[start + half, mid - dz],
            np.c_[start + half, mid + dz],
            np.c_[start - half, mid + dz],
        ),
        axis=1,
    ).reshape(-1, 3)
    return "glass", verts, np.arange(len(verts)).reshape(-1, 4)
