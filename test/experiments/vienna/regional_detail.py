"""Mapped waterfronts and building shells where the relief grid is visible at eye resolution."""

import numpy as np
import shapely
from building_colours import facade_colours, wall_material
from building_footprints import building_footprints
from outshine_ref.buildings.structures import suppressed
from outshine_ref.places.camera import axes, ecef
from shapely.geometry import box, shape

SKYLINE_MIN_PIXEL_HEIGHT = 2.0


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


def building_shells(data, nw, se, source, dem, radius, focal, *, distant=False):
    group = data.get("building", {})
    tile = box(nw[0], se[1], se[0], nw[1])
    minimum_height = (
        SKYLINE_MIN_PIXEL_HEIGHT * tile.distance(shapely.Point(0, 0)) / focal
        if distant
        else 0
    )

    def interval(p):
        high = p.get("render_height", 0)
        low = p.get("render_min_height", 0)
        return (
            (low, high)
            if high > low and high >= minimum_height and not suppressed(p)
            else None
        )

    geoms, properties = metric_polygons(
        group,
        nw,
        se,
        lambda p: p if interval(p) or (not distant and p.get("colour")) else None,
    )
    geoms, owners = building_footprints(geoms)
    if not len(geoms):
        return []
    properties = properties[owners]
    colours = np.asarray(facade_colours(geoms, properties), dtype=object)
    heights = np.asarray([interval(p) or (0.0, 0.0) for p in properties])
    renderable = heights[:, 1] > heights[:, 0]
    if distant:
        distance = shapely.distance(geoms, shapely.Point(0, 0))
        renderable &= heights[:, 1] * focal >= SKYLINE_MIN_PIXEL_HEIGHT * distance
    geoms, heights, colours = (
        geoms[renderable],
        heights[renderable],
        colours[renderable],
    )
    if not len(geoms):
        return []
    # Tile buffers contain duplicate source footprints. Each tile owns its interior.
    domain = tile.difference(box(-radius, -radius, radius, radius))
    partial = ~shapely.covered_by(geoms, domain)
    clipped = geoms.copy()
    clipped[partial] = shapely.intersection(geoms[partial], domain)
    polys, index = shapely.get_parts(clipped, return_index=True)
    valid = (shapely.get_type_id(polys) == 3) & ~shapely.is_empty(polys)
    polys = shapely.orient_polygons(polys[valid])
    index = index[valid]
    if not len(polys):
        return []
    visible, first, remap = np.unique(index, return_index=True, return_inverse=True)
    anchors = shapely.point_on_surface(geoms[visible])
    xy = shapely.get_coordinates(anchors)
    lon, lat = (xy / source.metres + [source.lon, source.lat]).T
    base, _ = dem.sample_with_zoom(lat, lon, allow_missing=True)
    missing = ~np.isfinite(base)
    if missing.any():
        xy[missing] = shapely.get_coordinates(
            shapely.point_on_surface(polys[first[missing]])
        )
        fallback_lon, fallback_lat = (
            xy[missing] / source.metres + [source.lon, source.lat]
        ).T
        base[missing] = dem.sample(fallback_lat, fallback_lon)
    if distant:
        base -= source.datum
    else:
        base -= source.datum + np.sum(xy * xy, axis=1) / (2 * 6371000.0)
    low = base[remap] + heights[index, 0]
    high = base[remap] + heights[index, 1]
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
    )
    products = [("regional roofs", roofs, np.arange(len(roofs)).reshape(-1, 3))]
    names = np.asarray([wall_material("regional walls", c) for c in colours])
    edge_material = names[index[parent]]
    for name in np.unique(edge_material):
        vertices = walls[edge_material == name].reshape(-1, 3)
        products.append((name, vertices, np.arange(len(vertices)).reshape(-1, 4)))
    if distant:
        anchor = ecef(source.lat, source.lon, source.datum)
        enu = axes(source.lat, source.lon).T
        for i, (name, vertices, faces) in enumerate(products):
            geo = vertices[:, :2] / source.metres + [source.lon, source.lat]
            xyz = ecef(geo[:, 1], geo[:, 0], vertices[:, 2] + source.datum)
            products[i] = name, (xyz - anchor) @ enu, faces
        return products
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
