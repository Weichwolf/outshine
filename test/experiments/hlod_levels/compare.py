"""#1959: exactly two candidates per level, using existing native source cache.

Run with PYTHONPATH=lib/src .venv/bin/python test/experiments/building_hlod/compare.py
--cache ~/.local/share/outshine/outshine/sources --output /tmp/outshine-research-...
Needs the lib places extra and numba. No source fetches or native frame-time claims.
"""

import argparse
from dataclasses import replace
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import time

import mapbox_vector_tile
from numba import njit
import numpy as np
from PIL import Image, ImageColor, ImageDraw
from scipy.ndimage import binary_erosion, distance_transform_edt, map_coordinates
import shapely
from shapely import affinity
from shapely.geometry import box, shape

from outshine_ref.buildings.hlod import (
    Building, Bake, bake, block_hulls, combine, extrusions, mesh, partition,
    polygons, remesh, roof_axis,
)
from outshine_ref.places.height import decode_terrarium


REVISION = "20261025_080012_pt"
CASES = {"Tokyo": (14, 14549, 6452), "Wien": (14, 8938, 5679),
         "Feldkirch": (14, 8628, 5747)}
GROUND_COLOUR = (.12, .16, .09)


def source(cache, tile, vector):
    def text(value):
        data = value.encode()
        return struct.pack("<I", len(data)) + data

    dataset = "openfreemap.openmaptiles" if vector else "mapterhorn.terrarium"
    revision = REVISION if vector else ""
    endpoint = (f"https://tiles.openfreemap.org/planet/{revision}/{{z}}/{{x}}/{{y}}.pbf"
                if vector else "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp")
    data = text("raw-source-content-2") + text(dataset) + struct.pack("<I", 2 if vector else 1)
    data += b"".join(text(v) for v in (revision, "vector" if vector else "elevation",
                                    "/".join(map(str, tile)), endpoint, ""))
    path = cache / hashlib.sha256(data).hexdigest()
    payload = path.read_bytes()
    return payload, {"tile": tile, "cache_key": path.name, "bytes": len(payload),
                     "sha256": hashlib.sha256(payload).hexdigest(), "endpoint": endpoint}


def linear_colour(value):
    try:
        rgb = np.array(ImageColor.getrgb(value)[:3]) / 255
    except (ValueError, TypeError, AttributeError):
        rgb = np.array([.70, .68, .64])
    return tuple(np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4))


def inputs(cache, tile):
    vector, vector_receipt = source(cache, tile, True)
    elevation, dem_receipt = source(cache, tile, False)
    dem = decode_terrarium(elevation)
    z, _, y = tile
    latitude = math.atan(math.sinh(math.pi * (1 - 2 * (y + .5) / 2**z)))
    side = 2 * math.pi * 6378137 * math.cos(latitude) / 2**z
    layer = mapbox_vector_tile.decode(vector, default_options={"y_coord_down": True})["building"]
    extent = layer["extent"]
    buildings = []

    def terrain(xy):
        xy = np.asarray(xy)
        u = xy[:, 0] / side * dem.shape[1] - .5
        v = (1 - xy[:, 1] / side) * dem.shape[0] - .5
        return map_coordinates(dem, [v, u], order=1, mode="nearest")

    for feature in layer["features"]:
        props = feature["properties"]
        if props.get("hide_3d"):
            continue
        height = float(props.get("render_height", 0))
        minimum = max(0.0, float(props.get("render_min_height", 0)))
        if height <= minimum:
            continue
        geometry = affinity.affine_transform(shape(feature["geometry"]),
                                             [side / extent, 0, 0, -side / extent, 0, side])
        for p in polygons(shapely.make_valid(geometry).intersection(box(0, 0, side, side))):
            if p.area < 1:
                continue
            base = float(np.median(terrain(np.asarray(p.exterior.coords)))) + minimum
            frame = roof_axis(p)
            rise = min(3.0, frame[2] * .5, (height - minimum) * .3)
            if height >= 30 or p.area >= 900:
                rise = 0.0
            buildings.append(Building(p, base, height - minimum, rise,
                                      linear_colour(props.get("colour")), frame))
    return buildings, (0, 0, side, side), terrain, [vector_receipt, dem_receipt]


def ground_nodes(bounds, terrain, cells=64):
    x0, y0, x1, y1 = bounds
    xx, yy = np.meshgrid(np.linspace(x0, x1, cells + 1), np.linspace(y0, y1, cells + 1))
    return terrain(np.column_stack((xx.ravel(), yy.ravel()))).reshape(cells + 1, cells + 1)


def terrain_mesh(bounds, terrain):
    ground = ground_nodes(bounds, terrain)
    cells = ground.shape[0] - 1
    colours = np.tile(GROUND_COLOUR, (cells, cells, 1)).astype(np.float32)
    return remesh(Bake(tuple(bounds), ground, colours, np.zeros((cells, cells))))


def save_mesh(path, value):
    np.savez(path, vertices=value.vertices.astype("<f4"), triangles=value.triangles.astype("<u4"),
             linear_rgb=value.colours.astype("<f4"), building=value.buildings)
    return path.stat().st_size


def save_bake(path, value, include_height):
    rgb = np.clip(np.round(value.colour * 255), 0, 255).astype(np.uint8)
    roughness = np.full(rgb.shape[:2], 217, np.uint8)
    arrays = {"linear_rgb8": rgb, "roughness8": roughness}
    if include_height:
        arrays["height_f32"] = value.height.astype("<f4")
        dy, dx = np.gradient(value.height,
                             (value.bounds[3] - value.bounds[1]) / (len(value.height) - 1),
                             (value.bounds[2] - value.bounds[0]) / (len(value.height) - 1))
        normals = np.stack((-dx, -dy, np.ones_like(dx)), axis=-1)
        normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
        arrays["normal_snorm8"] = np.round(normals * 127).astype(np.int8)
    np.savez(path, **arrays)
    return path.stat().st_size


def build_candidates(buildings, bounds, terrain, output):
    groups = {"fine": partition(buildings, bounds, 4),
              "middle": partition(buildings, bounds, 4),
              "coarse": partition(buildings, bounds, 2),
              "far": [(bounds, buildings)]}
    candidates, stats = {}, []
    for level, tiles in groups.items():
        names = {"fine": ["reference"], "middle": ["flat", "roof"],
                 "coarse": ["hulls", "heightfield"], "far": ["height_albedo", "albedo"]}[level]
        for name in names:
            meshes, tile_stats = [], []
            for index, (tile_bounds, members) in enumerate(tiles):
                started = time.perf_counter()
                path = output / f"{level}-{name}-{index}.npz"
                if level in ("fine", "middle"):
                    value = extrusions(members, roofs=name != "flat",
                                       tolerance=0.0 if level == "fine" else .5)
                    size = save_mesh(path, value)
                elif name == "hulls":
                    value = block_hulls(members, tile_bounds)
                    size = save_mesh(path, value)
                else:
                    ground = ground_nodes(tile_bounds, terrain)
                    field = bake(members, tile_bounds, ground)
                    if level == "far":
                        size = save_bake(path, field, name == "height_albedo")
                        value = remesh(field, heights=ground if name == "albedo" else None)
                    else:
                        value = remesh(field)
                        size = save_mesh(path, value)
                elapsed = time.perf_counter() - started
                meshes.append(value)
                tile_stats.append({"tile": index, "bounds": tile_bounds, "buildings": len(members),
                                   "triangles": 0 if level == "far" else len(value.triangles),
                                   "shared_terrain_triangles": len(value.triangles) if level == "far" else 0,
                                   "bytes": size, "build_s": elapsed})
            candidates[f"{level}/{name}"] = combine(meshes)
            stats.append({"level": level, "candidate": name, "tiles": tile_stats})
    return candidates, stats


@njit
def raster(triangles, colours, flags, depth, rgb, mask):
    for at in range(len(triangles)):
        t = triangles[at]
        if min(t[0, 2], t[1, 2], t[2, 2]) <= .1:
            continue
        x0, y0 = t[0, 0], t[0, 1]
        x1, y1 = t[1, 0], t[1, 1]
        x2, y2 = t[2, 0], t[2, 1]
        denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denominator) < 1e-10:
            continue
        left = max(0, int(math.floor(min(x0, x1, x2))))
        right = min(1279, int(math.ceil(max(x0, x1, x2))))
        top = max(0, int(math.floor(min(y0, y1, y2))))
        bottom = min(719, int(math.ceil(max(y0, y1, y2))))
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                u = ((y1 - y2) * (x + .5 - x2) + (x2 - x1) * (y + .5 - y2)) / denominator
                v = ((y2 - y0) * (x + .5 - x2) + (x0 - x2) * (y + .5 - y2)) / denominator
                w = 1 - u - v
                if min(u, v, w) < -1e-8:
                    continue
                distance = 1 / (u / t[0, 2] + v / t[1, 2] + w / t[2, 2])
                if distance < depth[y, x]:
                    depth[y, x] = distance
                    rgb[y, x] = colours[at]
                    mask[y, x] = flags[at]


def render(value, bounds, terrain, distance, azimuth):
    x0, y0, x1, y1 = bounds
    centre = np.array([(x0 + x1) / 2, (y0 + y1) / 2, 0.0])
    centre[2] = terrain(centre[None, :2])[0] + 25
    radius = math.hypot(x1 - x0, y1 - y0) / 2 + distance
    a, elevation = math.radians(azimuth), math.radians(15)
    eye = centre + radius * np.array([math.cos(a), math.sin(a), math.tan(elevation)])
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    xyz = (value.vertices - eye) @ np.array([right, up, forward]).T
    focal = 360 / math.tan(math.radians(55) / 2)
    projected = np.column_stack((640 + focal * xyz[:, 0] / xyz[:, 2],
                                 360 - focal * xyz[:, 1] / xyz[:, 2], xyz[:, 2]))
    depth = np.full((720, 1280), np.inf)
    rgb = np.tile([.22, .32, .46], (720, 1280, 1)).astype(np.float32)
    mask = np.zeros((720, 1280), np.uint8)
    raster(projected[value.triangles], value.colours, value.buildings, depth, rgb, mask)
    return rgb, mask.astype(bool)


def error(reference, candidate):
    rgb0, a = reference
    rgb1, b = candidate
    edge0 = a & ~binary_erosion(a)
    edge1 = b & ~binary_erosion(b)
    if not edge0.any() or not edge1.any():
        silhouette = math.hypot(1280, 720) if a.any() != b.any() else 0.0
        p95 = silhouette
    else:
        distances = np.concatenate((distance_transform_edt(~edge0)[edge1],
                                    distance_transform_edt(~edge1)[edge0]))
        silhouette, p95 = float(distances.max()), float(np.percentile(distances, 95))
    union = a | b
    colour = float(np.abs(rgb0[union] - rgb1[union]).mean()) if union.any() else 0.0
    return {"silhouette_max_px": silhouette, "silhouette_p95_px": p95,
            "mask_xor_px": int(np.count_nonzero(a ^ b)), "linear_rgb_mae": colour,
            "mean_colour_delta": float(np.abs(rgb0[a].mean(axis=0) - rgb1[b].mean(axis=0)).mean())
            if a.any() and b.any() else None}


def png(rgb):
    value = np.clip(rgb, 0, 1)
    value = np.where(value <= .0031308, value * 12.92, 1.055 * value ** (1 / 2.4) - .055)
    return Image.fromarray(np.round(value * 255).astype(np.uint8))


def comparisons(name, candidates, bounds, terrain, output, panels):
    ground = terrain_mesh(bounds, terrain)
    measured = []
    for level, distance, first, second in [
        ("middle", 500, "flat", "roof"), ("coarse", 3000, "hulls", "heightfield"),
        ("far", 15000, "height_albedo", "albedo")
    ]:
        for azimuth in (0, 120, 240):
            reference = render(combine([ground, candidates["fine/reference"]]),
                               bounds, terrain, distance, azimuth)
            rendered = [reference]
            for candidate in (first, second):
                value = candidates[f"{level}/{candidate}"]
                scene = value if level == "far" else combine([ground, value])
                result = render(scene, bounds, terrain, distance, azimuth)
                rendered.append(result)
                metrics = error(reference, result)
                measured.append({"city": name, "level": level, "candidate": candidate,
                                 "distance_to_bounding_sphere_m": distance,
                                 "azimuth_deg": azimuth, **metrics})
                png(result[0]).save(output / f"{level}-{candidate}-{azimuth}.png")
            png(reference[0]).save(output / f"{level}-fine-{azimuth}.png")
            row = Image.new("RGB", (1920, 380), "#18202b")
            labels = ["Fine recipe reference", first, second]
            draw = ImageDraw.Draw(row)
            for col, ((rgb, _), label) in enumerate(zip(rendered, labels)):
                row.paste(png(rgb).resize((640, 360)), (640 * col, 20))
                draw.text((640 * col + 8, 4), f"{name} {azimuth} deg | {label}", fill="white")
            panels[level].append(row)
    return measured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    started = time.perf_counter()
    args.output.mkdir(parents=True, exist_ok=True)
    report, panels = [], {level: [] for level in ("middle", "coarse", "far")}
    for name, tile in CASES.items():
        output = args.output / name
        output.mkdir(exist_ok=True)
        buildings, bounds, terrain, receipts = inputs(args.cache, tile)
        candidates, costs = build_candidates(buildings, bounds, terrain, output)
        measurements = comparisons(name, candidates, bounds, terrain, output, panels)
        report.append({"city": name, "bounds": bounds, "source_buildings": len(buildings),
                       "sources": receipts, "costs": costs, "images": measurements})
        print(f"{name}: {len(buildings)} buildings; {time.perf_counter() - started:.1f} s", flush=True)
    for level, rows in panels.items():
        image = Image.new("RGB", (1920, len(rows) * 380))
        for index, row in enumerate(rows):
            image.paste(row, (0, index * 380))
        image.save(args.output / f"{level}-comparison.png")
    result = {"raster": [1280, 720], "vertical_fov_deg": 55, "view_elevation_deg": 15,
              "roof_reference": "inferred gable: min(3 m, quarter OBB width, .3 height); flat >=30 m or >=900 m2",
              "limitations": "Unlit recipe reference, not native fine capture. Cache has no roof tags. One z14 tile per city; no frame-time acceptance.",
              "elapsed_s": time.perf_counter() - started, "cases": report}
    (args.output / "results.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(f"total {result['elapsed_s']:.1f} s", flush=True)


if __name__ == "__main__":
    main()
