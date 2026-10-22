"""Coverage-only interval diagnostics; no RGB/depth/World admission implied."""

import argparse
import hashlib
import itertools
import json
import resource
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from column_field_query import walk_cells
from compare_urban_parents import camera
from PIL import Image
from triangle_image_reference import image_rays, projected_bounds
from urban_parent_conditional import events, grid
from urban_parent_prefix import AtomicPrefix
from urban_parent_stats import UrbanParent


def dda_prefix(parent, origins, directions, stops=None):
    product = grid(parent)
    work = {"steps": 0, "cell_ray_visits": 0, "height_hypothesis_tests": 0}
    local = origins - product["reference_origin_m"]
    transmission = np.ones(len(origins))
    for ids, cells, begin, leave, keep in walk_cells(
        product, local, directions, work, maximum=stops
    ):
        probability = np.zeros(len(ids))
        for valid, _, _, mass in events(
            parent, product, local, directions, ids, cells, begin, leave
        ):
            probability += valid * mass
            work["height_hypothesis_tests"] += len(ids)
        transmission[ids] *= 1 - probability
        keep[:] = transmission[ids] > 0
    return transmission, work


def diagnostic_region(model, view):
    p = model.product
    low = p["reference_origin_m"][:2]
    high = low + p["shape"] * p["header"]["pitch_m"]
    xyz = np.array(
        list(
            itertools.product(
                [low[0], high[0]], [low[1], high[1]], [0, model.parent.heights[-1]]
            )
        )
    )
    bounds = projected_bounds(xyz, view)
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


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as archive:
        parent = UrbanParent(**{k: archive[k] for k in UrbanParent.__annotations__})
    report, arrays = compare(parent, args)
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        dirty=subprocess.check_output(["git", "status", "--porcelain"], text=True),
        parent_sha256=hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        scaling=scaling_probes(parent) if args.scaling_probe else [],
    )
    np.savez_compressed(args.output / "samples.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--scaling-probe", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
