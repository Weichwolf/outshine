"""#1959 final round: the corrected recipes selected by Technical Lead on 16 Nov.

Run with PYTHONPATH=lib/src .venv/bin/python test/experiments/hlod_levels/compare.py
--cache ~/.local/share/outshine/outshine/sources --output /tmp/outshine-research-...
Needs the lib places extra and numba. No source fetches or native frame-time claims.
"""

import argparse
from dataclasses import replace
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
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
    Building, Bake, bake, block_hulls, combine, extrusions, geometric_error, partition,
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
    ground = ground_nodes(bounds, terrain, math.ceil((bounds[2] - bounds[0]) / 8))
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


def build_geometry(level, members):
    if level == "coarse":
        return block_hulls(members, shapely.total_bounds([b.footprint for b in members]))
    return extrusions(members, roofs=True, tolerance=.5 if level == "middle" else 0,
                      remove_shared=True)


def adaptive_tiles(level, buildings, bounds, output):
    pending = [(bounds, buildings, "")]
    meshes, stats = [], []
    while pending:
        tile_bounds, members, key = pending.pop()
        if not members:
            continue
        started = time.perf_counter()
        value = build_geometry(level, members)
        if len(value.triangles) > 20000:
            pending.extend((child_bounds, child_members, key + str(index))
                           for index, (child_bounds, child_members) in enumerate(
                               partition(members, tile_bounds, 2)))
            continue
        reference = value if level == "fine" else build_geometry("fine", members)
        epsilon = geometric_error(reference, value)
        size = save_mesh(output / f"{level}-{key or 'root'}.npz", value)
        stats.append({"quad_suffix": key, "bounds": tile_bounds,
                      "buildings": len(members), "triangles": len(value.triangles),
                      "bytes": size, "build_s": time.perf_counter() - started, **epsilon})
        meshes.append(value)
    return combine(meshes), {"level": level, "tiles": stats}


def build_candidates(buildings, bounds, terrain, output):
    candidates, stats = {}, []
    for level in ("fine", "middle", "coarse"):
        value, cost = adaptive_tiles(level, buildings, bounds, output)
        candidates[level] = value
        stats.append(cost)
        print(f"{level}: {len(cost['tiles'])} leaves, max {max(t['triangles'] for t in cost['tiles'])} triangles", flush=True)
    started = time.perf_counter()
    cells = math.ceil((bounds[2] - bounds[0]) / 8)
    ground = ground_nodes(bounds, terrain, cells)
    field = bake(buildings, bounds, ground, cells)
    size = save_bake(output / "far-root.npz", field, True)
    field = replace(field, colour=np.round(field.colour * 255) / 255)
    value = remesh(field)
    reference = combine([terrain_mesh(bounds, terrain), candidates['fine']])
    epsilon = geometric_error(reference, value)
    stats.append({"level": "far", "tiles": [{"quad_suffix": "", "bounds": bounds,
                 "buildings": len(buildings), "triangles": 0,
                 "shared_terrain_triangles": len(value.triangles), "bytes": size,
                 "build_s": time.perf_counter() - started, **epsilon}]})
    candidates['far'] = value
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


def camera(reference_box, distance, azimuth):
    low, high = np.asarray(reference_box)
    centre = (low + high) / 2
    a, elevation = math.radians(azimuth), math.radians(15)
    direction = np.array([math.cos(a) * math.cos(elevation),
                          math.sin(a) * math.cos(elevation), math.sin(elevation)])
    left, right = 0.0, np.linalg.norm(high - low) + distance
    for _ in range(48):
        step = (left + right) / 2
        eye = centre + direction * step
        actual = np.linalg.norm(np.maximum(0, np.maximum(low - eye, eye - high)))
        if actual < distance:
            left = step
        else:
            right = step
    return centre + direction * right, -direction


def render(value, reference_box, distance, azimuth):
    eye, forward = camera(reference_box, distance, azimuth)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    xyz = (value.vertices - eye) @ np.array([right, up, forward]).T
    focal = 360 / math.tan(math.radians(60) / 2)
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


def transition(epsilon, height):
    return epsilon * height / (4 * math.tan(math.radians(60) / 2))


def comparisons(name, candidates, costs, bounds, terrain, output, panels):
    ground = terrain_mesh(bounds, terrain)
    original = combine([ground, candidates["fine"]])
    reference_box = np.array([original.vertices.min(axis=0), original.vertices.max(axis=0)])
    measured = []
    for level in ("middle", "coarse", "far"):
        tiles = next(c['tiles'] for c in costs if c['level'] == level)
        distance = max(.1, transition(max(t['epsilon_m'] for t in tiles), 720))
        for azimuth in (0, 120, 240):
            reference = render(original, reference_box, distance, azimuth)
            value = candidates[level]
            scene = value if level == "far" else combine([ground, value])
            result = render(scene, reference_box, distance, azimuth)
            eye, _ = camera(reference_box, distance, azimuth)
            measured.append({"city": name, "level": level, "distance_to_reference_aabb_m": distance,
                             "azimuth_deg": azimuth, "eye_xyz_m": eye.tolist(),
                             "reference_aabb": reference_box.tolist(), **error(reference, result)})
            png(result[0]).save(output / f"{level}-{azimuth}.png")
            png(reference[0]).save(output / f"{level}-fine-{azimuth}.png")
            difference = np.minimum(1, np.abs(reference[0] - result[0]) * 4)
            rendered = [reference[0], result[0], difference]
            masks = reference[1] | result[1]
            yy, xx = np.nonzero(masks)
            width = min(1280, max(int(xx.max() - xx.min()) + 33,
                                  (int(yy.max() - yy.min()) + 33) * 16 / 9))
            height = width * 9 / 16
            left = np.clip((xx.min() + xx.max() - width) / 2, 0, 1280 - width)
            top = np.clip((yy.min() + yy.max() - height) / 2, 0, 720 - height)
            crop = tuple(map(int, (left, top, left + width, top + height)))
            row = Image.new("RGB", (1920, 380), "#18202b")
            draw = ImageDraw.Draw(row)
            for col, (rgb, label) in enumerate(zip(rendered, ['Fine recipe', level, 'abs RGB x4'])):
                row.paste(png(rgb).crop(crop).resize((640, 360), Image.Resampling.NEAREST),
                          (640 * col, 20))
                draw.text((640 * col + 8, 4),
                          f"{name} {azimuth} deg | {label} | d={distance:.1f}m | crop {crop}", fill="white")
            panels[level].append(row)
    return measured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    started = time.perf_counter()
    reference_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    args.output.mkdir(parents=True, exist_ok=True)
    report, panels = [], {level: [] for level in ("middle", "coarse", "far")}
    for name, tile in CASES.items():
        output = args.output / name
        output.mkdir(exist_ok=True)
        buildings, bounds, terrain, receipts = inputs(args.cache, tile)
        candidates, costs = build_candidates(buildings, bounds, terrain, output)
        measurements = comparisons(name, candidates, costs, bounds, terrain, output, panels)
        report.append({"city": name, "bounds": bounds, "source_buildings": len(buildings),
                       "sources": receipts, "costs": costs, "images": measurements})
        (args.output / f"{name}-results.json").write_text(json.dumps(report[-1], indent=2, allow_nan=False) + "\n")
        print(f"{name}: {len(buildings)} buildings; {time.perf_counter() - started:.1f} s", flush=True)
    for level, rows in panels.items():
        image = Image.new("RGB", (1920, len(rows) * 380))
        for index, row in enumerate(rows):
            image.paste(row, (0, index * 380))
        image.save(args.output / f"{level}-comparison.png")
    result = {"raster": [1280, 720], "vertical_fov_deg": 60, "view_elevation_deg": 15,
              "reference_commit": reference_commit,
              "roof_reference": "inferred gable: min(3 m, quarter OBB width, .3 height); flat >=30 m or >=900 m2",
              "epsilon_method": "Bidirectional triangle-centroid distance plus source covering radius <=1m; conservative upper bound, not AABB lower bound. Zero for identical geometry.",
              "limitations": "Accepted unlit source/roof reference. One z14 source window per city, adaptive geometry leaves <=20000 triangles; far payload at 8m/texel on the same window. No native frame-time acceptance.",
              "elapsed_s": time.perf_counter() - started, "cases": report}
    (args.output / "results.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(f"total {result['elapsed_s']:.1f} s", flush=True)


if __name__ == "__main__":
    main()
