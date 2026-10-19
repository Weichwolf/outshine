"""Same-ray low-eye Fine versus actual Shell/Massed selection of a complete cell."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_source_billboard import depth_difference, difference, panel
from source_billboard import (
    average_samples,
    image_rays,
    projected_bounds,
    reference_samples,
)
from source_far_assets import load_source
from source_far_reprojection import prepare_source
from source_surface_fields import relight


def load_pair(a, b, *, allow_mixed_codes=False):
    for name in ("subset.index", "subset.forms"):
        if (a / name).read_bytes() != (b / name).read_bytes():
            raise ValueError("candidate changes original cell input")
    native = [json.loads((p / "native.json").read_text()) for p in (a, b)]
    source = [load_source(a), load_source(b, allow_mixed_codes=allow_mixed_codes)]
    for key in ("original_parent", "original_cell", "original_sources"):
        if source[0]["metadata"][key] != source[1]["metadata"][key]:
            raise ValueError("cell ownership changed")
    count = len(source[0]["metadata"]["original_sources"])
    if native[0]["anchor_ecef"] != native[1]["anchor_ecef"]:
        raise ValueError("different coordinate anchors")
    if native[0]["results"][0]["details"] != [0] * count:
        raise ValueError("reference must retain every original Fine source")
    for path in (a, b):
        receipt = json.loads((path / "mesh/receipt.json").read_text())
        if (
            receipt["original_sources"] != source[0]["metadata"]["original_sources"]
            or receipt["captures"][0] != receipt["captures"][1]
        ):
            raise ValueError("missing verified whole/cell source mapping")
    details = native[1]["results"][0]["details"]
    if len(details) != count or any(d not in (0, 1, 2) for d in details):
        raise ValueError("only observed Fine/Shell/Massed controls supported")
    return source, native, details


def low_eye_view(positions, up, east, north, focal, distance, yaw, move):
    xyz = positions.astype(float)
    centre = (xyz.min(0) + xyz.max(0)) / 2
    angle = np.radians(yaw)
    eye = centre + distance * np.ptp(xyz, axis=0).max() * (
        np.cos(angle) * east + np.sin(angle) * north
    )
    eye += ((xyz @ up).min() + 1.7 - eye @ up) * up
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    return eye + move * right, basis, focal, 1280, 720


def render(source, prepared, view, suns):
    boxes = [projected_bounds(s["positions"], view) for s in source]
    lo = np.maximum(np.floor(np.min(boxes, axis=(0, 1))).astype(int), 0)
    hi = np.minimum(np.ceil(np.max(boxes, axis=(0, 1))).astype(int), view[3:])
    width, height = hi - lo
    if width <= 0 or height <= 0:
        raise ValueError("empty comparison")
    origins, directions = image_rays(view, np.array([lo, hi]), width, height)
    geometry, seconds = [], []
    for item, index in zip(source, prepared, strict=True):
        start = time.perf_counter()
        geometry.append(reference_samples(item, index, origins, directions))
        seconds.append(time.perf_counter() - start)
    images, errors = [], []
    for sun in suns:
        light = {}
        for name, (depth, fields) in zip(
            ("Fine", "Native selection"), geometry, strict=True
        ):
            values = np.column_stack((relight(fields, sun, depth), np.isfinite(depth)))
            light[name] = average_samples(values, width, height, 2)
        images.append(light)
        errors.append(difference(light["Fine"], light["Native selection"]))
    return images, {
        "roi": [lo.tolist(), hi.tolist()],
        "sample_rays": len(origins),
        "query_seconds_fine_candidate": seconds,
        "lights": errors,
        "depth": depth_difference(
            geometry[0][0], geometry[1][0], np.isfinite(geometry[1][0])
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fine", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    source, native, details = load_pair(args.fine, args.candidate)
    out.mkdir(parents=True, exist_ok=False)
    up = np.array(native[0]["anchor_ecef"])
    up /= np.linalg.norm(up)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    suns = [0.3 * east - 0.4 * north + 0.8 * up, -0.6 * east + 0.3 * north + 0.5 * up]
    start = time.perf_counter()
    prepared = [prepare_source(s) for s in source]
    prepare_s = time.perf_counter() - start
    records = []
    for scale in (8, 32, 128):
        for yaw in (0, 44, 46, 90, 180, 270):
            for move in (0, 32):
                view = low_eye_view(
                    source[0]["positions"],
                    up,
                    east,
                    north,
                    native[0]["view"]["focal_px"],
                    scale,
                    yaw,
                    move,
                )
                images, record = render(source, prepared, view, suns)
                tag = f"{scale}-{yaw}-{move}"
                panel(out / (tag + ".png"), images, tag)
                np.savez_compressed(
                    out / (tag + ".npz"),
                    **{
                        f"{i}-{k}": v
                        for i, light in enumerate(images)
                        for k, v in light.items()
                    },
                )
                record.update(
                    distance_extents=scale,
                    yaw=yaw,
                    translation_m=move,
                    eye=view[0].tolist(),
                    basis=view[1].tolist(),
                )
                records.append(record)
                print(json.dumps({"case": tag, "errors": record["lights"]}), flush=True)
    report = {
        "scope": "Complete original cell, low-eye isolated diagnostic at 1280x720/original focal; 2x2 AA; colour-factor/Lambert only. Eye 1.7m above lowest source vertex plane, not DEM-verified ground. No native PBR/terrain/world/GPU gate or runtime admission.",
        "sources": source[0]["metadata"],
        "candidate_details": details,
        "focal_px": native[0]["view"]["focal_px"],
        "suns": [s.tolist() for s in suns],
        "cost": {
            "triangles": [len(s["indices"]) for s in source],
            "raw_native_bytes": [s["native_bytes"] for s in source],
            "prepare_reference_s": prepare_s,
            "peak_process_rss_linux_kib": resource.getrusage(
                resource.RUSAGE_SELF
            ).ru_maxrss,
            "scope": "Native Raised bytes, not GPU residency. Sample ray counts and Python/Rtree query times separate; allocations, internal candidate tests and memory traffic incomplete. No device-time extrapolation.",
        },
        "input_sha256": {
            str(p / name): hashlib.sha256((p / name).read_bytes()).hexdigest()
            for p in (args.fine, args.candidate)
            for name in (
                "input.json",
                "subset.index",
                "subset.forms",
                "native.json",
                "mesh/receipt.json",
            )
        },
        "code_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "source_far_assets.py",
                "source_far_reprojection.py",
                "source_surface_fields.py",
                "source_billboard.py",
                "compare_source_billboard.py",
                "packed_normals.py",
            )
        },
        "cases": records,
        "fallback": "Unvalidated cell selections retain original Fine; metrics precede fallback. Source membership is not coverage/appearance admission. No per-mass member IDs inferred from append ranges.",
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
