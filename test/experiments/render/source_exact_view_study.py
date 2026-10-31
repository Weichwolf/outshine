"""Compare selected original-source triangles with their whole original group.

This is a finite CPU geometry control, not native raster/World/device admission.
"""

import argparse
import hashlib
import json
import statistics
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from PIL import Image, ImageDraw
from source_binding_columns import encode
from source_cluster_bounds import frame
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts
from triangle_columns import world_rays
from triangle_image_reference import intersect_source, prepare_source

ROOT = Path(__file__).resolve().parents[3]


def cost(call):
    times = []
    for _ in range(7):
        start = time.process_time_ns()
        result = call()
        times.append((time.process_time_ns() - start) / 1e6)
    tracemalloc.start()
    result = call()
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return result, {
        "cpu_ms_median": statistics.median(times),
        "cpu_ms_max": max(times),
        "incremental_traced_current_bytes": current,
        "incremental_traced_peak_bytes": peak,
    }


def bytes_of(parts):
    return sum(value.nbytes for _, arrays in parts for value in arrays.values())


def check_words(full, small):
    for (a, aa), (b, bb) in zip(full, small, strict=True):
        if a["material"] != b["material"] or a["triangles"] != b["triangles"]:
            raise ValueError(
                "Native part/material order and all triangles must survive"
            )
        for field in ("positions", "normals", "uv", "uv1", "colours"):
            if not np.array_equal(
                aa[field][aa["indices"]].view("u4"), bb[field][bb["indices"]].view("u4")
            ):
                raise ValueError("Original corner words changed: " + field)


def projected_pixel_centres(source, ordinals, transform):
    vertices = source["positions"][np.unique(source["indices"][ordinals])]
    clip = np.column_stack((vertices, np.ones(len(vertices)))) @ transform.T
    if not len(clip) or np.any(clip[:, 3] <= 0):
        raise ValueError("Finite positive-w projected source region required")
    pixels = (clip[:, :2] / clip[:, 3, None] * [1, -1] + 1) * [1280, 720]
    lo = np.maximum(np.floor(pixels.min(axis=0)).astype(int) - 2, [0, 0])
    hi = np.minimum(np.ceil(pixels.max(axis=0)).astype(int) + 2, [2560, 1440])
    size = hi - lo
    if np.any(size <= 0) or np.prod(size) > 65536:
        raise ValueError("Projected source region must be nonempty and bounded")
    x, y = np.meshgrid(np.arange(lo[0], hi[0]) + 0.5, np.arange(lo[1], hi[1]) + 0.5)
    return np.column_stack((x.ravel(), y.ravel())), lo, hi


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    args.output.mkdir(parents=True)
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    view = json.loads(args.view.read_text())
    if hashlib.sha256(args.bounds.read_bytes()).hexdigest() != view["bounds_sha256"]:
        raise ValueError("Recorded view bounds identity required")
    row = next(row for row in view["cases"] if row["angle_degrees"] == args.angle)
    selected_ids = set(row["selected_source_ids"])
    binding = context["triangle_binding"]
    ordinals = np.flatnonzero(np.isin(binding[:, 1], list(selected_ids))).astype("<i4")
    if set(map(int, binding[ordinals, 1])) != selected_ids:
        raise ValueError("Every selected actual source needs its whole original")
    encoded = encode(binding, bitpack=True)
    if not np.array_equal(
        encoded.gather(ordinals).view("u4"), binding[ordinals].view("u4")
    ):
        raise ValueError("Original source bindings changed")
    direct, direct_cost = cost(lambda: triangle_parts(source, ordinals))
    compact, compact_cost = cost(
        lambda: triangle_parts(source, ordinals, referenced_vertices=True)
    )
    check_words(direct, compact)
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        operands = frame(views[args.angle], z["placements/words"])
    if operands is None:
        raise ValueError("Recorded native view operands required")
    placement, shift, matrix = operands
    translate = np.eye(4)
    translate[:, 3] += shift
    transform = matrix @ translate @ placement
    points, lo, hi = projected_pixel_centres(source, ordinals, transform)
    origins, directions = world_rays(transform, points, [2560, 1440])
    whole = {"positions": source["positions"], "indices": source["indices"]}
    selected = {
        "positions": source["positions"],
        "indices": source["indices"][ordinals],
    }
    reference, _ = intersect_source(
        whole, origins, directions, prepared=prepare_source(whole)
    )
    candidate, _ = intersect_source(
        selected, origins, directions, prepared=prepare_source(selected)
    )
    r, c = np.isfinite(reference), np.isfinite(candidate)
    common = r & c
    error = np.abs(reference[common] - candidate[common])
    if not r.any():
        raise ValueError("Projected region must contain original ray hits")
    width, height = hi - lo
    scale = 12
    panel_width = max(128, width * scale)
    image = Image.new("RGB", (panel_width * 3, height * scale + 18), "white")
    draw = ImageDraw.Draw(image)
    diff = np.zeros((len(points), 3), np.uint8)
    diff[r & ~c] = [255, 0, 0]
    diff[~r & c] = [0, 180, 0]
    for i, (label, pixels) in enumerate(
        zip(
            ("Original", "Selected", "Difference"),
            (
                np.repeat((r * 255).astype("u1")[:, None], 3, axis=1),
                np.repeat((c * 255).astype("u1")[:, None], 3, axis=1),
                diff,
            ),
            strict=True,
        )
    ):
        draw.text((panel_width * i + 2, 2), label, fill="black")
        panel = Image.fromarray(pixels.reshape(height, width, 3)).resize(
            (width * scale, height * scale), Image.Resampling.NEAREST
        )
        image.paste(panel, (panel_width * i, 18))
    image.save(args.output / "coverage.png")
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "angle_degrees": args.angle,
        "selected_sources": len(selected_ids),
        "selected_triangles": len(ordinals),
        "original_triangles": len(binding),
        "binding_bitpacked_array_bytes": encoded.payload_bytes,
        "direct_native_part_array_bytes": bytes_of(direct),
        "compact_native_part_array_bytes": bytes_of(compact),
        "direct_vertices": sum(part["vertices"] for part, _ in direct),
        "compact_vertices": sum(part["vertices"] for part, _ in compact),
        "direct_cost": direct_cost,
        "compact_cost": compact_cost,
        "rays": len(points),
        "raster_region_xyxy": [*map(int, lo), *map(int, hi)],
        "whole_hits": int(r.sum()),
        "selected_hits": int(c.sum()),
        "missing_rays": int((r & ~c).sum()),
        "extra_rays": int((~r & c).sum()),
        "maximum_common_ray_distance_error": float(error.max(initial=0)),
        "scope": "Complete original source triangles, original per-corner attribute words and actual selected binding rows. CPU ray and projection controls only: no native raster/PBR, shadows, simulation, live World, physical IO or device acceptance. CPU compaction cost starts with all original inputs resident; traced peaks exclude held source and native/OS allocations. Bitpacked source-binding payload excludes descriptors and other asset metadata. The recorded view was selected using offline original/candidate bounds, not a production lifetime contract.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--view", type=Path, required=True)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--angle", type=int, default=65)
    run(parser.parse_args())
