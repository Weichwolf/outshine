"""Pin coarse Terrarium input and sample heights at the existing road vertices."""

import argparse
from datetime import datetime, timezone
import hashlib
import io
import json
import math
from pathlib import Path
import subprocess
import time

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[3]
GIRTH_M = 40075016.68557849
TEMPLATE = "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp"


def world_coordinates(origin, xy):
    latitude, longitude = origin
    anchor = np.array([longitude / 360 * GIRTH_M,
                       math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi)])
    mercator = xy / math.cos(math.radians(latitude)) + anchor
    return (mercator + [GIRTH_M / 2, -GIRTH_M / 2]) * [1 / GIRTH_M, -1 / GIRTH_M]


def terrain_tile(sources, coordinate, fetch):
    url = TEMPLATE.format(z=coordinate[0], x=coordinate[1], y=coordinate[2])
    path = sources / (hashlib.sha256(url.encode()).hexdigest() + ".webp")
    began = time.perf_counter()
    if path.exists():
        data, origin = path.read_bytes(), "retained"
    elif fetch:
        data = subprocess.check_output(["curl", "--fail", "--silent", "--show-error", "--location",
                                        "--max-time", "30", "--max-filesize", "4194304", url])
        origin = "network"
    else:
        raise FileNotFoundError(f"{path}; acquisition requires --fetch")
    if len(data) > 4 * 1024**2:
        raise ValueError("DEM source exceeds byte limit")
    with Image.open(io.BytesIO(data)) as image:
        if image.size != (512, 512) or image.format != "WEBP":
            raise ValueError("512×512 Terrarium WebP required")
        rgb = np.asarray(image.convert("RGB"), dtype=np.float64)
    if origin == "network":
        sources.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(".tmp")
        temporary.write_bytes(data)
        temporary.replace(path)
    heights = rgb[:, :, 0] * 256 + rgb[:, :, 1] + rgb[:, :, 2] / 256 - 32768
    receipt = dict(url=url, path=str(path), tile=list(coordinate), sha256=hashlib.sha256(data).hexdigest(),
                   bytes=len(data), origin=origin, acquisition_and_decode_ms=(time.perf_counter() - began) * 1000)
    return heights, receipt


def sample_targets(origin, xy, sources, zoom, fetch=False):
    count = 1 << zoom
    world = world_coordinates(origin, xy)
    if np.any(world[:, 1] < 0) or np.any(world[:, 1] > 1):
        raise ValueError("DEM request lies outside Web Mercator coverage")
    pixel = world * (count * 512) - .5
    base = np.floor(pixel).astype(np.int64)
    u, v = (pixel - base).T
    columns = np.stack((base[:, 0], base[:, 0] + 1, base[:, 0], base[:, 0] + 1), axis=1) % (count * 512)
    rows = np.clip(np.stack((base[:, 1], base[:, 1], base[:, 1] + 1, base[:, 1] + 1), axis=1), 0, count * 512 - 1)
    coordinates = np.stack((columns // 512, rows // 512), axis=-1).reshape(-1, 2)
    tiles, owners = np.unique(coordinates, axis=0, return_inverse=True)
    values = np.empty((len(xy), 4))
    receipts = []
    for i, tile in enumerate(tiles):
        heights, receipt = terrain_tile(sources, (zoom, *map(int, tile)), fetch)
        receipts.append(receipt)
        selected = np.flatnonzero(owners == i)
        col, row = columns.ravel()[selected] % 512, rows.ravel()[selected] % 512
        values.ravel()[selected] = heights[row, col]
    result = ((1 - v) * ((1 - u) * values[:, 0] + u * values[:, 1])
              + v * ((1 - u) * values[:, 2] + u * values[:, 3]))
    return result, receipts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--zoom", type=int, default=11)
    parser.add_argument("--sources", type=Path, default=Path.home() / ".local/share/outshine/outshine/sources/transport-experiments/mapterhorn")
    parser.add_argument("--output", type=Path, default=ROOT / "build/experiments/transport/heights")
    args = parser.parse_args()
    if not 0 <= args.zoom <= 14:
        parser.error("coarse DEM zoom must be between 0 and 14")
    mesh_path = ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz"
    with np.load(mesh_path) as mesh:
        xy, faces = mesh["vertices"], mesh["indices"]
    original = ROOT / "build/experiments/transport/inputs/Vienna-input.json"
    origin = json.loads(original.read_text())["origin_lat_lon"]
    targets, receipts = sample_targets(origin, xy, args.sources, args.zoom, args.fetch)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.output / "Vienna-targets.npz", xy=xy, faces=faces, targets=targets)
    report = dict(origin_lat_lon=origin, dem_sources=receipts, mesh_sha256=hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
                  sampled_at=datetime.now(timezone.utc).isoformat(), zoom=args.zoom, height_range_m=[float(targets.min()), float(targets.max())],
                  sampling="bilinear at raster pixel centres with adjacent tile support, longitude wrapping and polar raster clamping", scope="Coarse prior for constructive profiles, not measured road elevations.")
    (args.output / "Vienna-targets.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
