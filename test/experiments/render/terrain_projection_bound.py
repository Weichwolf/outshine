"""Compare conservative projective displacement bounds without changing the pixel allowance."""

import argparse
import csv
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds
from terrain_grid_lod import page_geometry, raster_view
from terrain_patch_lod import patch_bounds


def row_sum_bound(rows, low, high, viewport):
    lengths = np.linalg.norm(rows, axis=1)
    scale = 0.5 * viewport * (lengths[:2] + np.maximum(np.abs(low), np.abs(high)) * lengths[2])
    return np.linalg.norm(scale, axis=1)


def spectral_bound(rows, low, high, viewport):
    result = np.zeros(len(low), dtype=np.result_type(rows, low, high, viewport))
    for x in (low[:, 0], high[:, 0]):
        for y in (low[:, 1], high[:, 1]):
            a = (rows[0] - x[:, None] * rows[2]) * (0.5 * viewport[0])
            b = (rows[1] - y[:, None] * rows[2]) * (0.5 * viewport[1])
            aa, bb, ab = (np.einsum("ni,ni->n", p, q) for p, q in ((a, a), (b, b), (a, b)))
            trace = aa + bb
            ratio = np.divide(aa - bb, trace, out=np.zeros_like(trace), where=trace > 0)
            off = np.divide(2 * ab, trace, out=np.zeros_like(trace), where=trace > 0)
            norm = np.sqrt(0.5 * trace * (1 + np.sqrt(ratio * ratio + off * off)))
            underflow = (trace == 0) & ((a != 0).any(axis=1) | (b != 0).any(axis=1))
            norm = np.where(underflow, np.inf, norm)
            result = np.maximum(result, norm)
    return result + 64 * np.finfo(np.float32).eps * row_sum_bound(rows, low, high, viewport)


def displacement_bound(errors, minimum_w, w_scale, numerator):
    denominator = minimum_w[:, None] - errors * w_scale
    result = np.divide(errors * numerator[:, None], denominator, out=np.full_like(errors, np.inf), where=denominator > 0)
    result[:, 0] = 0
    return result


def compare(pages, scenario):
    started = time.perf_counter()
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    width, height = 1280, 720
    views = [raster_view(scenario, rows, pages, width, height, yaw) for yaw in (0, 180)]
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    results = [dict(yaw=yaw, old_interiors=0, spectral_interiors=0, skirts=0, old_levels=np.zeros(4, dtype=int), spectral_levels=np.zeros(4, dtype=int)) for yaw in (0, 180)]
    templates = {}
    for row in rows:
        fractions, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        whole = world.min(axis=0)[None], world.max(axis=0)[None]
        visible_pages = [box_bounds(whole, view)[4][0] for view in views]
        if not any(visible_pages):
            continue
        errors, counts, nodes, _ = patch_bounds(fractions, local, 8, templates)
        low, high = world[nodes].min(axis=1), world[nodes].max(axis=1)
        for view, result, visible_page in zip(views, results, visible_pages):
            lo, hi, _, safe, visible, near = box_bounds((low, high), view)
            ndc_low = np.column_stack((2 * lo[:, 0] / width - 1, 1 - 2 * hi[:, 1] / height))
            ndc_high = np.column_stack((2 * hi[:, 0] / width - 1, 1 - 2 * lo[:, 1] / height))
            matrix = view[1] * np.array([2 * view[2] / width, 2 * view[2] / height, 1])[:, None]
            old = row_sum_bound(matrix, ndc_low, ndc_high, np.array([width, height]))
            new = spectral_bound(matrix, ndc_low, ndc_high, np.array([width, height]))
            for name, numerator in (("old", old), ("spectral", new)):
                projected = displacement_bound(errors, near, np.linalg.norm(matrix[2]), numerator)
                chosen = np.max(np.where((projected <= 0.5) & safe[:, None], np.arange(4), 0), axis=1)
                result[name + "_interiors"] += int(counts[np.arange(len(nodes)), chosen][visible].sum())
                result[name + "_levels"] += np.bincount(chosen[visible], minlength=4)
            result["skirts"] += 256 * int(visible_page)
    for result in results:
        for name in ("old", "spectral"):
            result[name + "_with_skirts"] = result[name + "_interiors"] + result["skirts"]
            result[name + "_levels"] = result[name + "_levels"].tolist()
    return dict(pages=len(rows), width=width, height=height, pixel_allowance=0.5,
                cases=results, seconds=time.perf_counter() - started,
                input_sha256=fingerprint.hexdigest(), scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),
                source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                scope="Same exact cached local interpolation errors; compare the shader row-sum bound with a spectral-norm bound. Native CPU bounds and float32 projection have additional margins. No depth rejection or native performance/pixel proof.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    report = compare(args.pages, args.scenario)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
