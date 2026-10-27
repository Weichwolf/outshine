"""Categorical prefix coverage diagnostics; no RGB/depth/World admission implied."""

import argparse
import hashlib
import itertools
import json
import resource
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_urban_parents import camera
from PIL import Image
from triangle_image_reference import average_samples, image_rays, projected_bounds
from urban_parent_prefix import AtomicPrefix, dda_prefix
from urban_parent_reference import prepare_resolved, trace_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load
from urban_prefix_runs import PrefixRuns


def diagnostic_region(model, view, resolved=None):
    points = []
    for p in [model.product] + ([resolved] if resolved is not None else []):
        low = p["reference_origin_m"][:2]
        high = low + p["shape"] * p["header"]["pitch_m"]
        points.extend(
            itertools.product(
                [low[0], high[0]], [low[1], high[1]], [0, p["height_bounds"][1]]
            )
        )
    bounds = projected_bounds(np.asarray(points), view)
    return np.array(
        [
            np.maximum(np.floor(bounds[0]), [0, 0]),
            np.minimum(np.ceil(bounds[1]), [1280, 720]),
        ],
        int,
    )


def scaling_probes(parent):
    results = []
    for factor in [1, 4, 16]:
        grown = UrbanParent(**{k: a.copy() for k, a in parent.arrays().items()})
        grown.frame[3] *= factor
        started = time.perf_counter()
        model = AtomicPrefix(grown)
        build = time.perf_counter() - started
        p = model.product
        pitch = p["header"]["pitch_m"]
        x = p["reference_origin_m"][0] + (p["shape"][0] // 2 + 0.5) * pitch
        y = p["reference_origin_m"][1] - pitch
        o = np.array([[x, y, 2.0], [x + 0.48 * pitch, y, 2.0]])
        d = np.tile([0.0, 1.0, 0.0], (2, 1))
        started = time.perf_counter()
        intervals = np.array([model.interval(a, b) for a, b in zip(o, d, strict=True)])
        query = time.perf_counter() - started
        started = time.perf_counter()
        exact, work = dda_prefix(grown, o, d)
        results.append(
            {
                "factor": factor,
                "hypothetical_recipe_cells": int(np.prod(p["shape"])),
                "build_seconds": build,
                "interval_seconds": query,
                "dda_seconds": time.perf_counter() - started,
                "dda_work": work,
                "interval_work": model.work.copy(),
                "intervals": intervals.tolist(),
                "exact": exact.tolist(),
            }
        )
    return results


def compare(parent, args):
    if args.runs:
        return compare_runs(parent, args)
    started = time.perf_counter()
    model = AtomicPrefix(parent)
    build = time.perf_counter() - started
    view = camera(
        parent.frame[2] * parent.frame[3],
        args.distance,
        args.motion,
        altitude=args.altitude,
    )
    box = diagnostic_region(model, view)
    width, height = box[1] - box[0]
    if min(width, height) <= 0:
        raise ValueError("nonempty parent projection required")
    o, d = image_rays(view, box, width, height, 1)
    d /= np.linalg.norm(d, axis=1)[:, None]
    started = time.perf_counter()
    reference, reference_work = dda_prefix(parent, o, d)
    reference_seconds = time.perf_counter() - started
    started = time.perf_counter()
    bounds = np.array([model.interval(a, b) for a, b in zip(o, d, strict=True)])
    interval_seconds = time.perf_counter() - started
    violation = np.maximum(bounds[:, 0] - reference, reference - bounds[:, 1])
    if violation.max() > 2e-12:
        raise AssertionError(f"prefix interval missed DDA by {violation.max()}")
    arrays = {
        "reference_transmission": reference.reshape(height, width),
        "transmission_bounds": bounds.reshape(height, width, 2),
    }
    for name, field in [
        ("reference_coverage", 1 - reference),
        ("lower_coverage", 1 - bounds[:, 1]),
        ("upper_coverage", 1 - bounds[:, 0]),
        ("width", bounds[:, 1] - bounds[:, 0]),
    ]:
        image = np.zeros((720, 1280), np.uint8)
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = np.round(
            np.clip(field.reshape(height, width), 0, 1) * 255
        ).astype(np.uint8)
        Image.fromarray(image).save(args.output / (name + ".png"))
    widths = bounds[:, 1] - bounds[:, 0]
    return {
        "profile": [1280, 720],
        "roi": box.tolist(),
        "distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "rays": len(o),
        "build_seconds": build,
        "dda_seconds": reference_seconds,
        "interval_seconds": interval_seconds,
        "dda_work": reference_work,
        "interval_work": model.work,
        "max_interval_violation": float(max(violation.max(), 0)),
        "interval_width_mean": float(widths.mean()),
        "interval_width_max": float(widths.max()),
        "fraction_width_over_001": float((widths > 0.01).mean()),
        "limitations": [
            "Coverage-only component; no radiance, first-hit depth, readiness or opaque composition",
            "Exact rational counts; density/log/exp floating guards not proved for native culling",
            "No source/fine-child/GEOS work; no flat-ENU to terrain/curvature equivalence",
            "Python Fraction/big-integer costs are not device/frame/p99 evidence",
        ],
    }, arrays


def traced_query(call):
    tracemalloc.start()
    try:
        result = call()
        live, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            row.count for row in tracemalloc.take_snapshot().statistics("lineno")
        )
        return {
            "live_traced_bytes": live,
            "peak_traced_bytes": peak,
            "live_traced_blocks": blocks,
            "retained_result_items": len(result),
        }
    finally:
        tracemalloc.stop()


def scaling_run_probes(parent):
    result = []
    for factor in [1, 4, 16]:
        grown = UrbanParent(**{k: a.copy() for k, a in parent.arrays().items()})
        grown.frame[3] *= factor
        model = PrefixRuns(grown)
        p = model.product
        pitch = p["header"]["pitch_m"]
        x = p["reference_origin_m"][0] + (p["shape"][0] // 2 + 0.5) * pitch
        y = p["reference_origin_m"][1] - pitch
        o = np.array([[x, y, 2], [x + 0.48 * pitch, y, 2]])
        d = np.tile([0, 1.0, 0], (2, 1))
        started = time.perf_counter()
        value, work = model.transmission(o, d)
        seconds = time.perf_counter() - started
        started = time.perf_counter()
        reference, control = dda_prefix(grown, o, d)
        direct = time.perf_counter() - started
        np.testing.assert_allclose(value, reference, atol=3e-12, rtol=3e-11)
        assert value[1] == 1
        result.append(
            {
                "factor": factor,
                "potential_cells": int(np.prod(p["shape"])),
                "run_seconds": seconds,
                "dda_seconds": direct,
                "run_work": work,
                "dda_work": control,
                "transmission": value.tolist(),
                "reference": reference.tolist(),
            }
        )
    return result


def compare_runs(parent, args):
    started = time.perf_counter()
    model = PrefixRuns(parent)
    build = time.perf_counter() - started
    resolved = None
    preparation = 0.0
    if args.resolved:
        started = time.perf_counter()
        resolved = prepare_resolved(load(args.resolved))
        preparation = time.perf_counter() - started
    view = camera(
        parent.frame[2] * parent.frame[3],
        args.distance,
        args.motion,
        altitude=args.altitude,
    )
    box = diagnostic_region(model, view, resolved)
    width, height = box[1] - box[0]
    if min(width, height) <= 0:
        raise ValueError("nonempty parent projection required")
    o, d = image_rays(
        view,
        box,
        width,
        height,
        args.samples,
        jitter_seed=1113 if args.samples > 1 else None,
    )
    d /= np.linalg.norm(d, axis=1)[:, None]
    started = time.perf_counter()
    reference, reference_work = dda_prefix(parent, o, d)
    reference_seconds = time.perf_counter() - started
    started = time.perf_counter()
    value, work = model.transmission(o, d)
    seconds = time.perf_counter() - started
    forced = PrefixRuns(parent, force=True)
    started = time.perf_counter()
    raw, raw_work = forced.transmission(o, d)
    raw_seconds = time.perf_counter() - started
    np.testing.assert_allclose(value, reference, atol=3e-12, rtol=3e-11)
    np.testing.assert_allclose(raw, reference, atol=3e-12, rtol=3e-11)
    arrays = {
        "reference_transmission": reference,
        "run_transmission": value,
        "forced_transmission": raw,
    }
    fields = {
        "reference_coverage": 1 - reference,
        "run_coverage": 1 - value,
        "absolute_error": np.abs(reference - value) / 3e-12,
    }
    resolved_work = {}
    resolved_seconds = 0.0
    if resolved is not None:
        started = time.perf_counter()
        depth, _, _, resolved_work = trace_resolved(resolved, o, d)
        resolved_seconds = time.perf_counter() - started
        fields["resolved_coverage"] = np.isfinite(depth).astype(float)
    for name, field in fields.items():
        crop = average_samples(field, width, height, args.samples)[..., 0]
        arrays[name] = crop
        image = np.zeros((720, 1280), np.uint8)
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = np.round(
            np.clip(crop, 0, 1) * 255
        )
        Image.fromarray(image).save(args.output / (name + ".png"))
    memory = {}
    if args.trace_memory:
        memory = {
            "dda": traced_query(lambda: dda_prefix(parent, o, d)),
            "runs": traced_query(lambda: model.transmission(o, d)),
        }
    report = {
        "profile": [1280, 720],
        "roi": box.tolist(),
        "distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "samples_per_axis": args.samples,
        "rays": len(o),
        "build_seconds": build,
        "dda_seconds": reference_seconds,
        "run_seconds": seconds,
        "forced_seconds": raw_seconds,
        "dda_work": reference_work,
        "run_work": work,
        "forced_work": raw_work,
        "max_transmission_difference": float(np.abs(value - reference).max()),
        "forced_max_transmission_difference": float(np.abs(raw - reference).max()),
        "tracemalloc_query_pass": memory,
        "retained_extra_array_bytes": sum(a.nbytes for a in model.density.cuts)
        + model.density.tail.nbytes
        + sum(a.nbytes for a in model.product.values() if isinstance(a, np.ndarray)),
        "resolved_preparation_seconds": preparation,
        "resolved_seconds": resolved_seconds,
        "resolved_work": resolved_work,
        "resolved_control_shape": resolved["shape"].tolist()
        if resolved is not None
        else None,
        "resolved_control_masses": len(resolved["columns"])
        if resolved is not None
        else None,
        "scope": "Categorical prefix probability only; no first-hit depth, normals, radiance, ready-child/foreign-surface composition, curved World or device admission. Source/control images are projected ROI samples, not pixel-footprint certificates.",
    }
    if resolved is not None:
        report["resolved_coverage_l1_pixels"] = float(
            np.abs(arrays["run_coverage"] - arrays["resolved_coverage"]).sum()
        )
    return report, arrays


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with np.load(args.parent, allow_pickle=False) as archive:
        parent = UrbanParent(**{k: archive[k] for k in UrbanParent.__annotations__})
    parent_load = time.perf_counter() - started
    report, arrays = compare(parent, args)
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        dirty=subprocess.check_output(["git", "status", "--porcelain"], text=True),
        parent_sha256=hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        parent_load_seconds=parent_load,
        resolved_sha256=hashlib.sha256(args.resolved.read_bytes()).hexdigest()
        if args.resolved
        else None,
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        scaling=(scaling_run_probes(parent) if args.runs else scaling_probes(parent))
        if args.scaling_probe
        else [],
    )
    report["process_peak_rss_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    np.savez_compressed(args.output / "samples.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", action="store_true")
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--resolved", type=Path)
    parser.add_argument("--samples", type=int, choices=range(1, 17), default=1)
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--scaling-probe", action="store_true")
    args = parser.parse_args()
    if not args.runs and (args.trace_memory or args.resolved or args.samples != 1):
        parser.error("memory/resolved/multiple samples require --runs")
    run(args)


if __name__ == "__main__":
    main()
