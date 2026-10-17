"""Bound adaptive terrain detail on exact cached subpatches; no runtime claim."""

import argparse
import csv
import hashlib
import json
import resource
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds
from terrain_grid_lod import page_geometry, raster_view, stencil
from terrain_visibility import partition


def patch_bounds(fractions, local, cells, templates):
    _, nodes = partition(cells)
    strides = tuple(2**k for k in range(cells.bit_length()))
    errors = np.zeros((len(nodes), len(strides)))
    counts = np.full_like(errors, 2 * cells * cells, dtype=np.int64)
    for patch, offset in enumerate(nodes[:, 0]):
        y, x = divmod(int(offset), 33)
        fx, fy = fractions[x : x + cells + 1], fractions[y : y + cells + 1]
        fx, fy = (fx - fx[0]) / (fx[-1] - fx[0]), (fy - fy[0]) / (fy[-1] - fy[0])
        for level, stride in enumerate(strides[1:], 1):
            key = (fx.tobytes(), fy.tobytes(), stride)
            if key not in templates:
                templates[key] = stencil(fx, stride, vertical=fy)
            indices, weights, count = templates[key]
            delta = np.einsum("ijk,ij->ik", local[nodes[patch][indices]], weights)
            errors[patch, level] = np.linalg.norm(delta, axis=1).max() + 1e-9
            counts[patch, level] = count
    return errors, counts, nodes, strides


def projected_error(errors, distances, focal, cosine):
    denominator = cosine * (distances[:, None] * cosine - errors)
    result = np.divide(
        focal * errors,
        denominator,
        out=np.full_like(errors, np.inf),
        where=denominator > 0,
    )
    result[:, 0] = 0
    return result


def compare(pages, scenario, cells, width, height):
    began = time.perf_counter()
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    views = [raster_view(scenario, rows, pages, width, height, yaw) for yaw in (0, 180)]
    eye, _, focal, _, _ = views[0]
    cosine = 1 / np.sqrt(1 + (width * width + height * height) / (4 * focal * focal))
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    templates, results = {}, []
    allowances = (0.25, 0.5, 1.0, 2.0, 4.0)
    baseline_all, baseline_views = 0, np.zeros(2, dtype=np.int64)
    totals = np.zeros(len(allowances), dtype=np.int64)
    view_totals = np.zeros((len(allowances), 2), dtype=np.int64)
    levels_count = np.zeros((len(allowances), cells.bit_length()), dtype=np.int64)
    for row in rows:
        fractions, local, _, world, page_hash = page_geometry(row, pages)
        fingerprint.update(page_hash)
        errors, counts, nodes, strides = patch_bounds(fractions, local, cells, templates)
        low, high = world[nodes].min(axis=1), world[nodes].max(axis=1)
        distances = np.maximum(
            0, np.linalg.norm((low + high) / 2 - eye, axis=1) - np.linalg.norm((high - low) / 2, axis=1)
        )
        projected = projected_error(errors, distances, focal, cosine)
        masks = np.stack([box_bounds((low, high), view)[4] for view in views])
        page_visible = np.array([box_bounds((world.min(axis=0)[None], world.max(axis=0)[None]), view)[4][0] for view in views])
        baseline_all += int(counts[:, 0].sum()) + 256
        baseline_views += masks @ counts[:, 0] + 256 * page_visible
        for index, pixels in enumerate(allowances):
            selected = np.max(np.where(projected <= pixels, np.arange(len(strides)), 0), axis=1)
            chosen = counts[np.arange(len(nodes)), selected]
            totals[index] += int(chosen.sum()) + 256
            view_totals[index] += masks @ chosen + 256 * page_visible
            levels_count[index] += np.bincount(selected, minlength=len(strides))
    for index, pixels in enumerate(allowances):
        results.append({
            "pixel_allowance": pixels,
            "all_resident_triangles": int(totals[index]),
            "view_triangles": view_totals[index].tolist(),
            "patches_by_stride": dict(zip(map(str, strides), levels_count[index].tolist())),
        })
    return {
        "pages": len(rows), "cells": cells, "width": width, "height": height,
        "view_yaws": [0, 180], "baseline_all_resident_triangles": baseline_all,
        "baseline_view_triangles": baseline_views.tolist(), "comparisons": results,
        "template_count": len(templates), "seconds": time.perf_counter() - began,
        "patches_processed": len(rows) * (32 // cells) ** 2,
        "height_bytes_read": len(rows) * 35 * 35 * 4,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024),
        "input_sha256": fingerprint.hexdigest(),
        "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "Ideal common-refinement positional bounds; every patch boundary and all outer skirts retained. Frustum only, no depth rejection. Full normal detail requires independent sampling. Shader rounding, raster quality and native CPU/GPU costs unproved.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cells", type=int, choices=(4, 8, 16), default=8)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = compare(args.pages, args.scenario, args.cells, 1280, 720)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
