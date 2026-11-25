"""Model reading exact or lower-precision terrain packs from the same spatial index.

Reads inline and indexed native height fixtures. The benchmark's start and finish
enclose a straight camera path. Selection bounds geometric displacement across
all headings, not changes in lighting or material shading.
"""

import argparse
from collections import defaultdict
import json
from math import hypot, inf, nextafter, tan
from pathlib import Path
from time import perf_counter

import numpy as np

from terrain_cached_page import PrefixReader, eligible, package, page_prefix, page_records
from terrain_page_packs import compressor
from terrain_parent_error import frame, geographic


def projection(report, anchor):
    origin, axes = frame(*np.radians(anchor))
    camera_to_ecef = axes[[0, 2, 1]] * np.array([[1], [1], [-1]])
    motion = report["motion"]
    eyes = (
        np.asarray([motion[phase]["position_m"] for phase in ("start", "finish")])
        @ camera_to_ecef + origin
    )
    profile = report["profile"]
    fov = motion["start"]["perspective_fov_deg"]
    focal = profile["height_px"] / (2 * tan(np.radians(fov) / 2))
    tx = profile["width_px"] / (2 * focal)
    ty = profile["height_px"] / (2 * focal)
    return eyes.min(axis=0), eyes.max(axis=0), hypot(1, tx, ty), focal * hypot(2**0.5, tx + ty)


def page_bounds(address, nodes):
    zoom, x, y = address
    west, north = geographic(zoom, x, y)
    east, south = geographic(zoom, x + 1, y + 1)
    middle, axes = frame((west + east) / 2, (north + south) / 2)
    low, high = float(nodes.min()), float(nodes.max())
    middle += ((low + high) / 2) * axes[2]
    radius = (6_400_000 + max(abs(low), abs(high))) * ((north - south) + (east - west)) / 2
    radius += (high - low) / 2
    return middle - radius, middle + radius


def quantize(nodes, step):
    # A common zero origin rounds coincident samples equally across pack boundaries.
    rounded = np.rint(nodes.astype(np.float64) / step)
    offset, end = float(rounded.min()), float(rounded.max())
    if not (-(2**31) <= offset <= end < 2**31) or end - offset > 65535:
        return None
    offset = int(offset)
    relative = rounded - offset
    most = int(end) - offset
    restored = (rounded * step).astype("<f4")
    error = float(np.max(abs(restored.astype(np.float64) - nodes)))
    dtype = np.dtype("<u1" if most <= 255 else "<u2")
    encoded = bytes((dtype.itemsize,)) + offset.to_bytes(4, "little", signed=True)
    return encoded + relative.astype(dtype).tobytes(), restored, error


def fits(error_m, distance_m, depth_scale, displacement_scale, pixels):
    if error_m == 0:
        return True
    # Both products interpolate the same triangles. Their height difference is
    # bounded by the largest vertex residual. Expand the bounds by that residual.
    denominator = displacement_scale * depth_scale + pixels * (depth_scale + 1)
    limit = nextafter(nextafter(pixels / nextafter(denominator, inf), 0) * distance_m, 0)
    return error_m <= limit


def region_manifest(data):
    outer = PrefixReader(data)
    version, ground_size = outer.read("QQ")
    if version == 0x35524E474F:
        return None
    if version != 0x36524E474F:
        raise ValueError("unsupported ground region version")
    ground = PrefixReader(outer.bytes(ground_size))
    if ground.read("Q")[0] != 0x0005314E5247:
        raise ValueError("unsupported native ground version")
    anchor = ground.read("dd")
    outer.bytes(outer.read("Q")[0])
    sheets, packs = outer.read("II")
    if not 0 < packs <= sheets <= 65536:
        raise ValueError("invalid terrain manifest")
    directory = {}
    slots = []
    for _ in range(packs):
        key = bytes(outer.array(1)).decode("ascii")
        if len(key) != 64 or key in directory or any(c not in "0123456789abcdef" for c in key):
            raise ValueError("invalid terrain pack identity")
        slot_data = outer.array(4)
        count = len(slot_data) // 4
        if not 0 < count <= 64:
            raise ValueError("invalid terrain pack page count")
        directory[key] = count
        slots.extend(np.frombuffer(slot_data, dtype="<u4").tolist())
    if outer.at != len(data) or sorted(slots) != list(range(sheets)):
        raise ValueError("incomplete terrain pack coverage")
    return anchor, directory


def packed_pages(data, expected):
    reader = PrefixReader(data)
    version, count = reader.read("II")
    if version != 1 or count != expected:
        raise ValueError("invalid indexed terrain pack header")
    yield from page_records(reader, count)
    if reader.at != len(data):
        raise ValueError("unexpected indexed terrain pack contents")


def native_pages(data, cache):
    manifest = region_manifest(data)
    if manifest is None:
        reader, anchor, count = page_prefix(data)
        return anchor, page_records(reader, count)
    anchor, directory = manifest

    def rows():
        for key, count in directory.items():
            body, _ = package(cache, key, kind="ground-terrain-pack")
            yield from packed_pages(body, count)

    return anchor, rows()


def read_packs(data, report, cache=None):
    anchor, records = native_pages(data, cache)
    view = projection(report, anchor)
    packs = defaultdict(list)
    for row, body in records:
        zoom, x, y = address = row["zoom"], row["x"], row["y"]
        nodes = np.frombuffer(body, dtype="<f4")
        bounds = page_bounds(address, nodes) if eligible(row) and np.isfinite(nodes).all() else None
        packs[zoom, x // 8, y // 8].append((bounds, nodes))
    return packs, view


def choose(group, step, view, pixels, compress):
    original = compress(b"".join(nodes.tobytes() for _, nodes in group))
    if any(bounds is None for bounds, _ in group):
        return original, original, []
    eye_low, eye_high, depth_scale, displacement_scale = view
    low = np.min([bounds[0] for bounds, _ in group], axis=0)
    high = np.max([bounds[1] for bounds, _ in group], axis=0)
    delta = np.maximum(0, np.maximum(eye_low - high, low - eye_high))
    gap = nextafter(hypot(*delta), 0)
    candidates = [quantize(nodes, step) for _, nodes in group]
    if any(candidate is None for candidate in candidates):
        return original, original, []
    error = max(candidate[2] for candidate in candidates)
    if not fits(error, gap, depth_scale, displacement_scale, pixels):
        return original, original, []
    selected = compress(b"".join(candidate[0] for candidate in candidates))
    return (original, selected, candidates) if selected < original else (original, original, [])


def exact_residual(group, candidates):
    words = [
        original.view("<u4") ^ restored.view("<u4")
        for (_, original), (_, restored, _) in zip(group, candidates)
    ]
    return np.concatenate(words).view("<u1").reshape(-1, 4).T.tobytes()


def measure(data, report, cache, steps, pixels):
    packs, view = read_packs(data, report, cache)
    compress = compressor()
    for step in steps:
        original = selected = exact = compact_packs = compact_pages = 0
        for group in packs.values():
            before, after, candidates = choose(group, step, view, pixels, compress)
            original += before
            selected += after
            exact += after
            if candidates:
                exact += compress(exact_residual(group, candidates))
                compact_packs += 1
                compact_pages += len(candidates)
        yield {
            "step_m": step,
            "packs": len(packs),
            "compact_packs": compact_packs,
            "compact_pages": compact_pages,
            "exact_bytes": original,
            "selected_bytes": selected,
            "with_exact_residual_bytes": exact,
        }


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cache", type=Path, required=True)
    cli.add_argument("--key", required=True)
    cli.add_argument("--report", type=Path, required=True)
    cli.add_argument("--pixels", type=float, default=1)
    args = cli.parse_args()
    if not np.isfinite(args.pixels) or args.pixels <= 0:
        cli.error("--pixels requires a positive finite error budget")
    start = perf_counter()
    data, _ = package(args.cache, args.key)
    report = json.loads(args.report.read_text())
    variants = list(
        measure(data, report, args.cache, (0.0625, 0.125, 0.25, 0.5, 1, 2), args.pixels)
    )
    print(json.dumps(dict(seconds=perf_counter() - start, variants=variants)))


if __name__ == "__main__":
    main()
