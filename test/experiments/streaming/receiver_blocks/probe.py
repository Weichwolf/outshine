"""Check interval blocks against every scalar sample in selected captured tiles."""

import argparse
import array
import ctypes
import hashlib
import json
import math
import sys
import time
from pathlib import Path

from interval import project


def depth(path, count):
    library = ctypes.CDLL("libzstd.so.1")
    library.ZSTD_decompress.argtypes = [
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_void_p,
        ctypes.c_size_t,
    ]
    library.ZSTD_decompress.restype = ctypes.c_size_t
    raw = path.read_bytes()
    target = ctypes.create_string_buffer(count * 4)
    size = library.ZSTD_decompress(target, len(target), raw, len(raw))
    if size != len(target):
        raise ValueError("invalid depth frame")
    result = array.array("f")
    result.frombytes(target.raw)
    if sys.byteorder != "little":
        result.byteswap()
    return result


def apply(matrix, point):
    output = []
    for row in range(4):
        value = matrix[12 + row]
        for column in range(3):
            value += matrix[column * 4 + row] * point[column]
        output.append(value)
    return output


def scalar_point(x, y, d, frame):
    width, height = frame["width"], frame["height"]
    ndc = [2 * (x + 0.5) / width - 1, 1 - 2 * (y + 0.5) / height, 1 - 2 * d]
    world_h = apply(frame["inverse_camera_clip"], ndc)
    if not math.isfinite(world_h[3]) or world_h[3] <= 0:
        return None
    world = [world_h[i] / world_h[3] - frame["eye"][i] for i in range(3)]
    result = []
    for light in frame["shadow"]["regions"]:
        point = apply(light["projection"], world)
        result.append([point[i] / point[3] for i in range(3)])
    return result


def check_tile(left, top, side, values, frame):
    width, height = frame["width"], frame["height"]
    right, bottom = min(width, left + side), min(height, top + side)
    valid = [
        (x, y, values[y * width + x])
        for y in range(top, bottom)
        for x in range(left, right)
        if math.isfinite(values[y * width + x]) and 0 < values[y * width + x] <= 1
    ]
    if not valid:
        return {"origin": [left, top], "empty": True}
    minimum, maximum = min(v[2] for v in valid), max(v[2] for v in valid)
    ndc = [
        (2 * (left + 0.5) / width - 1, 2 * (right - 0.5) / width - 1),
        (1 - 2 * (bottom - 0.5) / height, 1 - 2 * (top + 0.5) / height),
        (1 - 2 * maximum, 1 - 2 * minimum),
    ]
    start = time.perf_counter()
    try:
        bounds = project(
            ndc,
            frame["inverse_camera_clip"],
            frame["eye"],
            [r["projection"] for r in frame["shadow"]["regions"]],
        )
    except ArithmeticError as error:
        return {
            "origin": [left, top],
            "unknown": str(error),
            "valid_samples": len(valid),
        }
    bound_seconds = time.perf_counter() - start
    start = time.perf_counter()
    checked = 0
    for x, y, d in valid:
        samples = scalar_point(x, y, d, frame)
        if samples is None:
            continue
        for interval, point in zip(bounds, samples, strict=True):
            for limits, value in zip(interval, point, strict=True):
                if not limits[0] <= value <= limits[1]:
                    raise ValueError(
                        f"escaped interval at {x},{y}: {limits} vs {value}"
                    )
        checked += 1
    return {
        "origin": [left, top],
        "valid_samples": len(valid),
        "checked_samples": checked,
        "bounds": bounds,
        "bound_seconds": bound_seconds,
        "scalar_seconds": time.perf_counter() - start,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frame", required=True, type=Path)
    parser.add_argument("--captures", required=True, type=Path)
    parser.add_argument("--tile", default=16, type=int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    frame = json.loads(args.frame.read_text())
    values = depth(args.captures / frame["depth"], frame["width"] * frame["height"])
    width, height = frame["width"], frame["height"]
    origins = [
        (x, y)
        for y in (0, height // 4, height // 2, 3 * height // 4, height - args.tile)
        for x in (0, width // 4, width // 2, 3 * width // 4, width - args.tile)
    ]
    results = [check_tile(x, y, args.tile, values, frame) for x, y in origins]
    report = {
        "frame_sha256": hashlib.sha256(args.frame.read_bytes()).hexdigest(),
        "tile": args.tile,
        "profile": [width, height],
        "results": results,
        "success": True,
        "qualified_for_page_suppression": False,
        "scope": "Selected finite CPU tiles, outward sequential arithmetic; no native timing or raster/receiver completeness qualification",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
