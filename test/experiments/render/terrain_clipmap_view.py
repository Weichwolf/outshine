"""Render cached terrain clipmaps with independent fine normals; diagnostic, not PBR."""

import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image

from current_visibility import project, raster
from terrain_clipmap import layout, positions
from terrain_clipmap_cache import HeightClipmap
from terrain_clipmap_source import NativeTerrain
from terrain_geo import ecef


def camera(scenario, source, width, height, yaw):
    view = ET.parse(scenario).getroot().find("views/view")
    at = view.find("at")
    lon, lat, altitude = (float(at.get(k)) for k in ("lon", "lat", "heightM"))
    if at.get("samplesHeight") == "yes":
        altitude += source.heights_lon_lat([[lon, lat]])[0]
    eye = (ecef(lon, lat, altitude) - source.origin) @ source.axes.T
    if not np.all(np.isfinite(eye)):
        raise ValueError("the camera needs a known height")
    longitude, latitude = np.deg2rad([lon, lat])
    east = np.array([-np.sin(longitude), np.cos(longitude), 0]) @ source.axes.T
    north = np.array([-np.sin(latitude) * np.cos(longitude),
                      -np.sin(latitude) * np.sin(longitude), np.cos(latitude)]) @ source.axes.T
    up = np.cross(east, north)
    bearing, pitch = np.deg2rad([float(at.get("bearingDeg", 0)) + yaw,
                                float(at.get("pitchDeg", 0))])
    forward = np.cos(pitch) * (np.cos(bearing) * north + np.sin(bearing) * east) + np.sin(pitch) * up
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack([right, np.cross(right, forward), forward])
    focal = height / (2 * np.tan(np.deg2rad(float(view.get("fovDeg"))) / 2))
    return eye, basis, focal, width, height


def surface_points(depth, view):
    eye, basis, focal, width, height = view
    y, x = np.nonzero(depth > 0)
    rays = np.column_stack(((x + 0.5 - width / 2) / focal,
                            (height / 2 - y - 0.5) / focal, np.ones(len(x))))
    return (y, x), eye + (rays @ basis) * (0.1 / depth[y, x])[:, None]


def fine_normals(source, xy, spacing=1.0):
    result = np.empty((len(xy), 3))
    for first in range(0, len(xy), 65536):
        points = xy[first:first + 65536]
        gradients = []
        for axis in range(2):
            delta = np.zeros(2)
            delta[axis] = spacing
            gradients.append((source(points + delta) - source(points - delta)) / (2 * spacing))
        normals = np.column_stack((-gradients[0], -gradients[1], np.ones(len(points))))
        result[first:first + len(points)] = normals / np.linalg.norm(normals, axis=1)[:, None]
    return result


def render(vertices, triangles, view, source):
    projected, z = project(vertices, view)
    depth = raster(projected, z, triangles, view[3], view[4])
    pixels, points = surface_points(depth, view)
    normals = fine_normals(source, points[:, :2])
    light = np.array([-0.4, -0.5, 1.0])
    light /= np.linalg.norm(light)
    intensity = 0.22 + 0.78 * np.maximum(normals @ light, 0)
    image = np.full((view[4], view[3], 3), [157, 185, 205], dtype=np.uint8)
    unknown = ~np.all(np.isfinite(normals), axis=1)
    image[pixels] = np.rint(np.clip(np.nan_to_num(intensity[:, None]) * 210, 0, 255)).astype(np.uint8)
    image[pixels[0][unknown], pixels[1][unknown]] = [255, 0, 255]
    return image, depth, dict(covered_pixels=len(points), unknown_normal_pixels=int(unknown.sum()),
                             near_crossing_triangles=int(np.sum((z[triangles].min(axis=1) <= 0.1)
                                                               & (z[triangles].max(axis=1) > 0.1))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--input-byte-budget", type=int, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--half-cells", type=int, default=64)
    parser.add_argument("--levels", type=int, default=10)
    parser.add_argument("--cell-m", type=float, default=1)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--camera-offset", type=float, nargs=3, default=(0, 0, 0))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    began = time.perf_counter()
    source = NativeTerrain.from_cache(args.cache, args.key, args.input_byte_budget)
    loaded = time.perf_counter()
    view = camera(args.scenario, source, 1280, 720, args.yaw)
    view[0][:] += args.camera_offset
    mesh = layout(view[0][:2], args.levels, args.half_cells, args.cell_m)
    heights = HeightClipmap(source, args.levels, args.half_cells, args.cell_m)
    initial_samples = heights.move(view[0][:2])
    vertices = positions(mesh, heights)
    built = time.perf_counter()
    image, depth, counters = render(vertices, mesh.triangles, view, source)
    args.output.mkdir(parents=True, exist_ok=False)
    Image.fromarray(image).save(args.output / "image.png")
    np.save(args.output / "depth.npy", depth.astype(np.float32))
    report = dict(source=source.provenance, camera_eye_enu_m=view[0].tolist(), yaw=args.yaw,
                  command=sys.argv, scenario_sha256=hashlib.sha256(args.scenario.read_bytes()).hexdigest(),
                  levels=args.levels, half_cells=args.half_cells, cell_m=args.cell_m,
                  vertices=len(vertices), triangles=len(mesh.triangles),
                  height_cache_samples=initial_samples,
                  height_cache_bytes=sum(level.values.nbytes for level in heights.levels),
                  bounds_enu_m=mesh.outer_bounds.tolist(), counters=counters,
                  seconds=dict(read=loaded - began, geometry=built - loaded,
                               software_render=time.perf_counter() - built),
                  scope="Cached geographic height-field reconstruction; fine normal clay diagnostic. "
                        "No native tile-plane equivalence, materials, water, shadows, TAA, "
                        "near-plane clipping or 240km/device-budget acceptance.")
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "source"}))


if __name__ == "__main__":
    main()
