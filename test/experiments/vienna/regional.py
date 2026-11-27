"""Georeferenced medium-distance relief built from pinned vector tiles and DEM."""

from concurrent.futures import ThreadPoolExecutor

import numpy as np
import shapely
from bridges import build_water_bridges
from ground_routes import cut_ground_routes, route_footprints
from outshine_ref.buildings.structures import suppressed
from outshine_ref.places.height import Elevation
from outshine_ref.places.sources import (
    acquire,
    covering_tiles,
    decode_vector,
    tile_geography,
)
from PIL import Image, ImageDraw
from regional_detail import building_shells, true_shore
from scene_mesh import Meshes
from shapely.geometry import Polygon, box, shape


def tile_rect(source, tile):
    lat, lon = tile_geography([[0, 0], [1, 1]], 1, tile)
    return (np.column_stack((lon, lat)) - [source.lon, source.lat]) * source.metres


def regional_sources(source, place, snapshot, cache, fov):
    yaw = np.deg2rad(place["heading_deg"])
    focal = 360 / np.tan(np.deg2rad(fov) / 2)
    reach = np.clip(12 * focal, 6000, 15000)
    half = np.arctan(640 / focal) + np.deg2rad(5)
    angles = np.linspace(yaw - half, yaw + half, 17)
    sector = Polygon(
        np.vstack(
            ([0, 0], np.column_stack((np.sin(angles), np.cos(angles))) * reach, [0, 0])
        )
    )
    tiles = []
    for t in covering_tiles(source.lat, source.lon, reach, 14):
        bounds = tile_rect(source, t)
        if sector.intersects(box(*bounds.min(axis=0), *bounds.max(axis=0))):
            tiles.append(t)
    sources = [
        (
            "vector",
            t,
            f"https://tiles.openfreemap.org/planet/{snapshot}/"
            + "/".join(map(str, t))
            + ".pbf",
        )
        for t in tiles
    ]
    for t in sorted(
        {
            (12, (x + i) >> 2, (y + j) >> 2)
            for z, x, y in tiles
            for i in (-1, 0, 1)
            for j in (-1, 0, 1)
        }
    ):
        sources.append(
            (
                "elevation",
                t,
                "https://tiles.mapterhorn.com/" + "/".join(map(str, t)) + ".webp",
            )
        )
    with ThreadPoolExecutor(max_workers=6) as workers:
        return list(workers.map(lambda s: acquire(s, cache, fetch=True), sources))


def paint(image, feature, extent, value):
    geom = feature["geometry"]
    polygons = geom["coordinates"]
    if geom["type"] == "Polygon":
        polygons = [polygons]
    elif geom["type"] != "MultiPolygon":
        return
    draw = ImageDraw.Draw(image)
    factor = image.width / extent
    for poly in polygons:
        for i, ring in enumerate(poly):
            xy = (np.asarray(ring) * factor).astype(int)
            if len(xy) >= 3:
                draw.polygon([tuple(p) for p in xy], fill=value if i == 0 else 0)


def class_fields(data, size, elevated, detail):
    land, structures = Image.new("L", (size, size)), Image.new("F", (size, size))
    for layer in ("landcover", "water", "transportation", "building"):
        if layer not in data or (detail and layer == "building"):
            continue
        group = data[layer]
        features = group["features"]
        if layer == "building":
            features = sorted(
                features, key=lambda f: f["properties"].get("render_height", 0)
            )
        for f in features:
            props = f["properties"]
            if layer == "building":
                h = props.get("render_height", 0)
                if h > 0 and not suppressed(props):
                    paint(structures, f, group["extent"], float(h))
            else:
                value = {
                    "grass": 1,
                    "farmland": 1,
                    "wood": 2,
                    "wetland": 1,
                    "bridge": 4,
                }.get(props.get("class"), 0)
                if layer == "water":
                    value = 3
                if value == 4 and id(f) in elevated:
                    continue
                if value:
                    paint(land, f, group["extent"], value)
    return np.asarray(land), np.asarray(structures)


def tile_surface(source, receipt, dem, focal, radius, data, elevated):
    nw, se = tile_rect(source, receipt["tile"])
    tile = box(nw[0], se[1], se[0], nw[1])
    d = tile.distance(shapely.Point(0, 0))
    size = int(np.clip((se[0] - nw[0]) * focal / (max(d, 500) * 2), 72, 256))
    detail = d < 2.5 * focal
    land, height = class_fields(data, size, elevated, detail)
    x = np.linspace(nw[0], se[0], size + 1)
    y = np.linspace(nw[1], se[1], size + 1)
    # Insert the four near-patch edges so the two source domains meet exactly.
    xx = np.unique(np.r_[x, [v for v in (-radius, radius) if x[0] < v < x[-1]]])
    yy = np.unique(np.r_[y, [v for v in (-radius, radius) if y[-1] < v < y[0]]])[::-1]
    xc = (xx[1:] + xx[:-1]) / 2
    yc = (yy[1:] + yy[:-1]) / 2
    ix = np.clip(np.searchsorted(x, xc) - 1, 0, size - 1)
    iy = np.clip(np.searchsorted(-y, -yc) - 1, 0, size - 1)
    land = land[np.ix_(iy, ix)]
    height = height[np.ix_(iy, ix)]
    gx, gy = np.meshgrid(xx, yy)
    xy = np.stack((gx, gy), axis=-1)
    if detail:
        true_shore(xy, land, data, nw, se, radius)
    lon, lat = (xy / source.metres + [source.lon, source.lat]).T
    # Shared DEM vertices preserve tile-edge elevations and account for Earth curvature.
    z = (
        dem.sample(lat.T.clip(-85, 85), lon.T)
        - source.datum
        - (gx * gx + gy * gy) / (2 * 6371000.0)
    )
    ground = np.dstack((xy, z)).reshape(-1, 3)
    side = len(xx)
    a = np.arange(len(ground)).reshape(gx.shape)[:-1, :-1]
    faces = np.stack((a, a + side, a + side + 1, a + 1), axis=-1)
    outside = (abs(xc)[None, :] >= radius) | (abs(yc)[:, None] >= radius)
    products = (
        building_shells(data, nw, se, source, dem, radius, focal) if detail else []
    )
    routes = (
        route_footprints(data.get("transportation", {}), nw, se, tile, focal)
        if detail
        else {}
    )
    for i, name in enumerate(
        (
            "regional soil",
            "regional grass",
            "regional wood",
            "regional water",
            "regional deck",
        )
    ):
        mask = (land == i) & outside
        if routes and i < 3:
            products.extend(cut_ground_routes(ground, faces[mask], name, routes))
        elif mask.any():
            products.append((name, ground, faces[mask]))
    mask = (height > 0) & outside
    if mask.any():
        q = ground[faces[mask]].copy()
        top = q.copy()
        top[:, :, 2] = q[:, :, 2].mean(axis=1)[:, None] + height[mask, None]
        vertices = top.reshape(-1, 3)
        idx = np.arange(len(vertices)).reshape(-1, 4)
        products.append(("regional roofs", vertices, idx))
        for dy, dx, i, j in ((0, -1, 0, 1), (1, 0, 1, 2), (0, 1, 2, 3), (-1, 0, 3, 0)):
            other = np.roll(height, (-dy, -dx), axis=(0, 1))
            if dx == -1:
                other[:, 0] = 0
            if dx == 1:
                other[:, -1] = 0
            if dy == -1:
                other[0, :] = 0
            if dy == 1:
                other[-1, :] = 0
            use = height[mask] > other[mask]
            if not use.any():
                continue
            lo = q[use][:, [i, j]].copy()
            lo[:, :, 2] += other[mask][use, None]
            v = np.concatenate((top[use][:, [i, j]], lo[:, ::-1]), axis=1).reshape(
                -1, 3
            )
            products.append(("regional walls", v, np.arange(len(v)).reshape(-1, 4)))
    return products, tile


def regional_meshes(source, receipts, focal, radius):
    dem = Elevation(receipts)
    features, water, originals = [], [], []
    nearby = {}
    reach = 2.5 * focal
    for row in receipts:
        if row["kind"] != "vector":
            continue
        nw, se = tile_rect(source, row["tile"])
        if (
            box(*np.minimum(nw, se), *np.maximum(nw, se)).distance(shapely.Point(0, 0))
            > reach
        ):
            continue
        data = nearby[row["url"]] = decode_vector(row)
        for name in ("water", "transportation"):
            group = data.get(name)
            if not group:
                continue
            for f in group["features"]:
                props = f["properties"]
                if name == "transportation" and props.get("brunnel") != "bridge":
                    continue
                geom = shapely.transform(
                    shape(f["geometry"]),
                    lambda xy, scale=group["extent"], origin=nw, delta=se - nw: (
                        origin + xy / scale * delta
                    ),
                )
                if name == "water":
                    water.append(shapely.make_valid(geom))
                else:
                    features.append((name, geom, props))
                    if props.get("class") == "bridge":
                        originals.append((id(f), geom, props.get("layer", 1)))
    bridges = Meshes()

    def terrain(xy):
        lon, lat = (xy / source.metres + [source.lon, source.lat]).T
        return (
            dem.sample(lat, lon)
            - source.datum
            - np.sum(xy * xy, axis=1) / (2 * 6371000.0)
        )

    solved = build_water_bridges(
        bridges,
        features,
        box(-reach, -reach, reach, reach),
        shapely.union_all(water),
        terrain,
        exclude=box(-radius, -radius, radius, radius),
    )
    elevated = {
        k
        for k, g, l in originals
        if l in solved and solved[l].covers(g.representative_point())
    }
    for receipt in receipts:
        if receipt["kind"] == "vector":
            data = nearby.pop(receipt["url"], None)
            yield tile_surface(
                source,
                receipt,
                dem,
                focal,
                radius,
                data if data is not None else decode_vector(receipt),
                elevated if data is not None else (),
            )
    yield [(n, *mesh) for n, mesh in bridges.items()], shapely.Polygon()


def beyond_region(vertices, faces, tiles):
    domain = shapely.union_all(tiles)
    footprint = shapely.polygons(vertices[faces, :2])
    touched = shapely.intersects(footprint, domain)
    border = np.flatnonzero(touched & ~shapely.covered_by(footprint, domain))
    extra = []
    for i in border:
        clipped = footprint[i].difference(domain)
        triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(clipped))
        if not len(triangles):
            continue
        xy = np.asarray(
            [g.exterior.coords[:3] for g in shapely.orient_polygons(triangles)]
        ).reshape(-1, 2)
        xyz = vertices[faces[i]]
        normal = np.cross(xyz[1] - xyz[0], xyz[2] - xyz[0])
        if abs(normal[2]) < 1e-12:
            continue
        extra.append(np.c_[xy, xyz[0, 2] - (xy - xyz[0, :2]) @ normal[:2] / normal[2]])
    if not extra:
        return vertices, faces[~touched]
    extra = np.concatenate(extra)
    new = np.arange(len(vertices), len(vertices) + len(extra)).reshape(-1, 3)
    return np.vstack((vertices, extra)), np.vstack((faces[~touched], new))
