"""Georeferenced medium-distance relief built from pinned vector tiles and DEM."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

import numpy as np
import shapely
from bridge_approach import APPROACH_SEARCH, bridge_approaches, earthwork_meshes
from bridge_contacts import bank_cuts
from bridges import draw_water_crossings, plan_water_crossings
from ground_surface import (
    ROAD_WIDTH,
    ROUTE_MATERIALS,
    clip_route_areas,
    partition_ground,
    route_footprints,
)
from outshine_ref.buildings.structures import suppressed
from outshine_ref.places.height import Elevation
from outshine_ref.places.sources import (
    acquire,
    covering_tiles,
    decode_vector,
    tile_geography,
)
from PIL import Image, ImageDraw
from regional_detail import building_shells, metric_polygons
from scene_mesh import Meshes, shore_walls, terrain_grid, terrain_height
from shapely.geometry import Polygon, box, shape
from shore_grade import SHORE_REACH_M, shore_grade
from viaducts import elevated_outline
from water_surface import clipped_water


@dataclass
class RegionalTransport:
    receipts: list
    dem: Elevation
    nearby: dict
    approaches: list
    crossings: list
    elevated: set
    bridges: Meshes


def tile_rect(source, tile):
    lat, lon = tile_geography([[0, 0], [1, 1]], 1, tile)
    return (np.column_stack((lon, lat)) - [source.lon, source.lat]) * source.metres


def regional_sources(
    source, place, snapshot, cache, fov, *, transport_domain=None, pins=()
):
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
        tile = box(*bounds.min(axis=0), *bounds.max(axis=0))
        path = "/".join(map(str, t))
        url = f"https://tiles.openfreemap.org/planet/{snapshot}/{path}.pbf"
        if sector.intersects(tile) or (
            transport_domain is not None
            and url in pins
            and transport_domain.intersects(tile)
        ):
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


def bridgehead_sources(source, spans, receipts, snapshot, cache, pins):
    known = {row["url"] for row in receipts}
    needed = set()
    for _, span in spans:
        for position in (span.origin, span.origin + span.length * span.direction):
            lon, lat = position / source.metres + [source.lon, source.lat]
            needed.update(covering_tiles(lat, lon, APPROACH_SEARCH, 14))
    nearby = []
    for tile in sorted(needed):
        _, x, y = tile
        key = "/".join(map(str, tile))
        vector = f"https://tiles.openfreemap.org/planet/{snapshot}/{key}.pbf"
        dem = f"https://tiles.mapterhorn.com/12/{x >> 2}/{y >> 2}.webp"
        if vector in known or vector not in pins or dem not in pins:
            continue
        for kind, index, url in (
            ("elevation", (12, x >> 2, y >> 2), dem),
            ("vector", tile, vector),
        ):
            if url in known:
                continue
            receipt = acquire((kind, index, url), cache, fetch=True)
            if receipt["sha256"] != pins[url]:
                raise ValueError(f"Changed Vienna bridgehead source: {url}")
            nearby.append(receipt)
            known.add(url)
    return nearby


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
    for layer in ("landcover", "transportation", "building"):
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
                if value == 4 and id(f) in elevated:
                    continue
                if value:
                    paint(land, f, group["extent"], value)
    return np.asarray(land), np.asarray(structures)


def tile_surface(
    source,
    receipt,
    dem,
    focal,
    radius,
    data,
    elevated,
    approaches,
    seam=None,
    crossings=(),
    *,
    water_plan,
    local_shore=None,
):
    nw, se = tile_rect(source, receipt["tile"])
    tile = box(nw[0], se[1], se[0], nw[1])
    d = tile.distance(shapely.Point(0, 0))
    size = int(np.clip((se[0] - nw[0]) * focal / (max(d, 500) * 2), 72, 256))
    detail = d < 2.5 * focal
    land, height = class_fields(data, size, elevated, detail)
    x = np.linspace(nw[0], se[0], size + 1)
    y = np.linspace(nw[1], se[1], size + 1)
    # Insert the four near-patch edges so the two source domains meet exactly.
    local = box(-radius, -radius, radius, radius)
    use_seam = seam is not None and tile.intersects(local.boundary)
    inner_axis = np.unique(seam[:, 0]) if use_seam else np.array([-radius, radius])
    xx = np.unique(np.r_[x, inner_axis[(x[0] < inner_axis) & (inner_axis < x[-1])]])
    yy = np.unique(np.r_[y, inner_axis[(y[-1] < inner_axis) & (inner_axis < y[0])]])[
        ::-1
    ]
    xc = (xx[1:] + xx[:-1]) / 2
    yc = (yy[1:] + yy[:-1]) / 2
    ix = np.clip(np.searchsorted(x, xc) - 1, 0, size - 1)
    iy = np.clip(np.searchsorted(-y, -yc) - 1, 0, size - 1)
    land = land[np.ix_(iy, ix)]
    height = height[np.ix_(iy, ix)]
    gx, gy = np.meshgrid(xx, yy)
    xy = np.stack((gx, gy), axis=-1)
    lon, lat = (xy / source.metres + [source.lon, source.lat]).T
    # Shared DEM vertices preserve tile-edge elevations and account for Earth curvature.
    z = (
        dem.sample(lat.T.clip(-85, 85), lon.T)
        - source.datum
        - (gx * gx + gy * gy) / (2 * 6371000.0)
    )
    if use_seam:
        boundary = (np.abs(gx) <= radius) & (np.abs(gy) <= radius)
        boundary &= (np.abs(gx) == radius) | (np.abs(gy) == radius)
        z[boundary] = terrain_height(seam, xy[boundary])
    ground = np.dstack((xy, z)).reshape(-1, 3)
    side = len(xx)
    a = np.arange(len(ground)).reshape(gx.shape)[:-1, :-1]
    faces = np.stack((a, a + side, a + side + 1, a + 1), axis=-1)
    outside = (abs(xc)[None, :] >= radius) | (abs(yc)[:, None] >= radius)
    products = (
        building_shells(data, nw, se, source, dem, radius, focal) if detail else []
    )
    names = (
        "regional soil",
        "grass",
        "regional wood",
        "regional water",
        "regional deck",
        *ROUTE_MATERIALS,
    )
    tile_water = tuple(clipped_water(water_plan, tile.difference(local)))
    water = shapely.union_all([footprint for _, footprint in tile_water])
    footprints = {3: water}
    graded = {}
    protected = shapely.Polygon()
    if detail:
        neighborhood = tile.buffer(SHORE_REACH_M, quad_segs=2)
        routes = route_footprints(
            data.get("transportation", {}), nw, se, neighborhood, focal
        )
        adjoining = [a for a in approaches if tile.intersects(a.footprint)]
        earthworks = [(a, "road") for a in adjoining] + [
            (cut, "regional soil")
            for cut in bank_cuts(
                ground, faces[outside], crossings, tile, approaches=adjoining
            )
        ]
        graded = dict(enumerate(earthworks, start=len(names)))
        footprints.update(
            {i: work.footprint.intersection(tile) for i, (work, _) in graded.items()}
        )
        footprints.update(
            {
                i: clip_route_areas((routes[name],), tile)
                for i, name in enumerate(ROUTE_MATERIALS, 5)
            }
        )
        if any(body.footprint.intersects(neighborhood) for body in water_plan):
            buildings, _ = metric_polygons(
                data.get("building", {}), nw, se, lambda _: 1
            )
            protected = shapely.union_all(
                [
                    *routes.values(),
                    *(work.footprint for work, _ in earthworks),
                    *shapely.make_valid(buildings),
                ],
                grid_size=1e-6,
            )
    near_bank = (
        (local_shore, local.boundary) if use_seam and local_shore is not None else None
    )

    def retaining_height(points):
        lon, lat = (points / source.metres + [source.lon, source.lat]).T
        return dem.sample(lat, lon) - source.datum

    shore = shore_grade(
        water_plan,
        tile,
        protected,
        neighbor=near_bank,
        retaining_height=retaining_height,
    )
    if detail or tile_water:
        shore_parts = Meshes()
        seam_band = local.boundary.buffer(SHORE_REACH_M, quad_segs=2)
        seam_band = seam_band.intersection(tile.difference(local))
        parts = partition_ground(
            ground,
            faces[outside],
            land[outside],
            dict(enumerate(names)) | {i: i for i in graded},
            footprints,
            breaks=shore.breaks(water, seam_band)
            if detail and seam is not None
            else (),
        )
        for name, vertices, triangles in parts:
            if name in graded:
                work, material = graded[name]
                products.extend(earthwork_meshes(vertices, triangles, work, material))
            elif name == "regional water":
                edges = vertices[triangles[:, [0, 1, 1, 2, 2, 0]]].reshape(-1, 2, 3)
                shore_walls(shore_parts, "regional soil", source, shore, edges)
            elif shore.contacts and name in names[:3]:
                used, inverse = np.unique(triangles, return_inverse=True)
                products.append(
                    (name, shore.deform(source, vertices[used]), inverse.reshape(-1, 3))
                )
            else:
                products.append((name, vertices, triangles))
        products.extend((name, *mesh) for name, mesh in shore_parts.items())
    else:
        for i, name in enumerate(names[:5]):
            mask = (land == i) & outside
            if mask.any():
                products.append((name, ground, faces[mask]))
    products.extend(
        ("regional water", *body.mesh(source, part)) for body, part in tile_water
    )
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


def regional_transport(
    source,
    receipts,
    focal,
    radius,
    *,
    viaducts=(),
    local_approaches=(),
    connect_bridgeheads=None,
):
    dem = Elevation(receipts)
    features, water_shapes, originals = [], [], []
    nearby = {}
    reach = 2.5 * focal

    def collect(row):
        if row["kind"] != "vector":
            return
        nw, se = tile_rect(source, row["tile"])
        data = nearby[row["url"]] = decode_vector(row)
        for name in ("water", "transportation"):
            group = data.get(name)
            if not group:
                continue
            for f in group["features"]:
                props = f["properties"]
                if (
                    name == "transportation"
                    and props.get("brunnel") != "bridge"
                    and (
                        props.get("brunnel")
                        or props.get("class") not in ROAD_WIDTH
                        or props.get("class") == "path"
                        or props.get("layer", 0) != 0
                        or not f["geometry"]["type"].endswith("LineString")
                    )
                ):
                    continue
                geom = shapely.transform(
                    shape(f["geometry"]),
                    lambda xy, scale=group["extent"], origin=nw, delta=se - nw: (
                        origin + xy / scale * delta
                    ),
                )
                if name == "water":
                    water_shapes.append(shapely.make_valid(geom))
                else:
                    if props.get("class") == "bridge":
                        layer = props.get("layer", 1)
                        originals.append((id(f), geom, layer))
                        if elevated_outline(geom, layer, viaducts):
                            continue
                    features.append((name, geom, props))

    for row in receipts:
        if row["kind"] != "vector":
            continue
        nw, se = tile_rect(source, row["tile"])
        if (
            box(*np.minimum(nw, se), *np.maximum(nw, se)).distance(shapely.Point(0, 0))
            <= reach
        ):
            collect(row)
    bridges = Meshes()

    def terrain(xy):
        lon, lat = (xy / source.metres + [source.lon, source.lat]).T
        return (
            dem.sample(lat, lon)
            - source.datum
            - np.sum(xy * xy, axis=1) / (2 * 6371000.0)
        )

    water = shapely.union_all(water_shapes)
    crossings = list(
        plan_water_crossings(
            features,
            box(-reach, -reach, reach, reach),
            water,
            terrain,
            exclude=box(-radius, -radius, radius, radius),
        )
    )
    spans = [(crossing.footprint, crossing.span) for crossing in crossings]
    if connect_bridgeheads is not None:
        extra = connect_bridgeheads(spans, receipts)
        if extra:
            receipts = [*receipts, *extra]
            dem = Elevation(receipts)
            for row in extra:
                collect(row)
            water = shapely.union_all(water_shapes)
    approaches = [
        *local_approaches,
        *bridge_approaches(spans, features, terrain, water),
    ]
    solved = draw_water_crossings(bridges, crossings, water, terrain, approaches)
    elevated = {
        k
        for k, g, l in originals
        if (l in solved and solved[l].covers(g.representative_point()))
        or elevated_outline(g, l, viaducts)
    }
    return RegionalTransport(
        receipts, dem, nearby, approaches, crossings, elevated, bridges
    )


def regional_meshes(source, transport, focal, radius, water_plan, *, local_shore=None):
    boundary = box(-radius, -radius, radius, radius).boundary
    align_seam = any(
        a.footprint.intersects(boundary) for a in transport.approaches
    ) or any(body.footprint.distance(boundary) <= SHORE_REACH_M for body in water_plan)
    seam = terrain_grid(source, radius)[0] if align_seam else None
    for receipt in transport.receipts:
        if receipt["kind"] == "vector":
            data = transport.nearby.get(receipt["url"])
            yield tile_surface(
                source,
                receipt,
                transport.dem,
                focal,
                radius,
                data if data is not None else decode_vector(receipt),
                transport.elevated if data is not None else (),
                transport.approaches,
                seam,
                transport.crossings,
                water_plan=water_plan,
                local_shore=local_shore,
            )
    yield [(n, *mesh) for n, mesh in transport.bridges.items()], shapely.Polygon()


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
