"""Equal-input local-light candidate work and radiance checks; CPU only (#448)."""

import argparse
import hashlib
import json
import os
import platform
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from local_light_grid import build_grid
from local_light_inputs import ROOT, Lights, camera, receivers, street_lights


def contributions(points, normals, lights, ids):
    offset = lights.positions[ids][None, :, :] - points[:, None, :]
    square = np.einsum("ijk,ijk->ij", offset, offset)
    positive = square > 0
    reach = square / np.where(lights.ranges[ids] > 0, lights.ranges[ids] ** 2, np.inf)
    support = positive & (reach < 1)
    lambert = np.maximum(0, np.einsum("ijk,ik->ij", offset, normals))
    weight = np.divide(
        np.maximum(0, 1 - reach**2) * lambert,
        square * np.sqrt(square),
        out=np.zeros_like(square),
        where=positive,
    )
    return weight @ lights.colours[ids] * (0.5 / np.pi), support


def evaluate(points, normals, lights, grid):
    result = np.zeros_like(points)
    active_count = np.zeros(len(points), dtype=np.int64)
    visits = 0
    if grid is None:
        ids = np.arange(len(lights.positions))
        for begin in range(0, len(points), 64):
            end = min(begin + 64, len(points))
            result[begin:end], support = contributions(
                points[begin:end], normals[begin:end], lights, ids
            )
            active_count[begin:end] = support.sum(axis=1)
            visits += (end - begin) * len(ids)
        return result, active_count, visits
    keys = grid.keys(points)
    for key in np.unique(keys):
        selected = np.flatnonzero(keys == key)
        begin, count = map(int, grid.headers[key])
        ids = np.r_[grid.globals, grid.indices[begin : begin + count]]
        if len(ids) != len(np.unique(ids)):
            raise ValueError("a cell repeats an emitter ID")
        for first in range(0, len(selected), 64):
            batch = selected[first : first + 64]
            result[batch], support = contributions(
                points[batch], normals[batch], lights, ids
            )
            active_count[batch] = support.sum(axis=1)
            visits += len(batch) * len(ids)
    return result, active_count, visits


def scene(name):
    lights, receipt = street_lights(
        spacing=4 if name == "dense" else 16, count=8189 if name == "dense" else None
    )
    if name == "overlap":
        lights = Lights(
            np.tile(np.median(lights.positions, axis=0), (1021, 1)),
            np.full(1021, 128.0),
            np.tile([600, 390, 180], (1021, 1)),
            ["declared-overlap/" + str(i) for i in range(1021)],
        )
    elif name == "global":
        lights.ranges[::127] = 0
    receipt["scene_recipe"] = name
    receipt["retained_source_lamps"] = 0 if name == "overlap" else len(lights.positions)
    return lights, receipt


def process_memory():
    status = Path("/proc/self/status")
    if status.exists():
        fields = dict(line.split(":", 1) for line in status.read_text().splitlines())
        return {
            "process_vm_hwm_bytes": int(fields["VmHWM"].split()[0]) * 1024,
            "process_vm_rss_bytes": int(fields["VmRSS"].split()[0]) * 1024,
        }
    if sys.platform == "darwin":
        return {
            "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        }
    return {"process_peak_rss_bytes": None}


def run(args):
    lights, provenance = scene(args.scene)
    frame = camera(lights, args.camera)
    points, normals = [], []
    depths = [25.0, 250.0, 500.0, 5000.0]
    for depth in depths:
        p, n = receivers(frame, depth, args.samples_x, args.samples_y)
        points.append(p)
        normals.append(n)
    points, normals = np.vstack(points), np.vstack(normals)
    centre_sample = (args.samples_y // 2) * args.samples_x + args.samples_x // 2
    far_point = points[
        (len(depths) - 1) * args.samples_x * args.samples_y + centre_sample
    ]
    controls = np.array(
        [
            frame.eye - frame.basis[2] * 6,
            frame.eye + frame.basis[2] * 10,
            far_point - frame.basis[2] * 6,
        ],
        dtype="<f4",
    ).astype(float)
    lights.positions = np.vstack((lights.positions, controls))
    lights.ranges = np.r_[lights.ranges, [32, 32, 32]]
    lights.colours = np.vstack((lights.colours, np.tile([600, 390, 180], (3, 1))))
    lights.source_ids += [
        "authored-control/behind-eye",
        "authored-control/near",
        "authored-control/infinite-tail",
    ]
    provenance["additional_authored_controls"] = lights.source_ids[-3:]
    if args.mode == "brute":
        grid = None
        build_ms = peak = live_allocations = 0
    else:
        tracemalloc.start()
        began = time.perf_counter()
        grid = build_grid(
            lights,
            frame,
            args.mode,
            args.tile,
            index_budget=args.index_bytes,
            refine=not args.no_refine,
        )
        build_ms = (time.perf_counter() - began) * 1000
        _, peak = tracemalloc.get_traced_memory()
        live_allocations = sum(
            stat.count for stat in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    began = time.perf_counter()
    values, active, visits = evaluate(points, normals, lights, grid)
    query_ms = (time.perf_counter() - began) * 1000
    reference, reference_active, all_visits = evaluate(points, normals, lights, None)
    error = float(np.max(np.abs(values - reference) / np.maximum(reference, 1e-5)))
    missing = int(np.count_nonzero(active != reference_active))
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        raise ValueError("private outputs belong outside the repository")
    output.mkdir(parents=True, exist_ok=True)
    arrays = {
        "positions": lights.positions,
        "ranges": lights.ranges,
        "colours": lights.colours,
        "points": points,
        "normals": normals,
        "radiance": values,
        "reference": reference,
        "reference_support_count": reference_active,
    }
    storage = len(lights.positions) * 64
    if grid is not None:
        arrays.update(
            headers=grid.headers,
            indices=grid.indices,
            globals=grid.globals,
            edges=grid.edges,
            keys=grid.keys(points),
        )
        storage += (
            grid.headers.nbytes + grid.indices.nbytes + grid.globals.nbytes + 16 + 64
        )
    np.savez_compressed(output / "captures.npz", **arrays)
    report = {
        "scope": "CPU FP64 evaluation of the existing finite point attenuation and ideal diffuse response. No GPU/BRDF-family/runtime/Place/device-budget acceptance. Source inputs are quantized FP32; no general FP32 culling bound claimed.",
        "provenance": provenance,
        "scene": args.scene,
        "camera": args.camera,
        "mode": args.mode,
        "tile_px": args.tile,
        "logical_profile_px": [frame.width, frame.height],
        "sample_grid_per_depth": [args.samples_x, args.samples_y],
        "depths_m": depths,
        "lights": len(lights.positions),
        "queries": len(points),
        "source_ids": lights.source_ids,
        "light_tests_all": all_visits,
        "nonzero_queries": int(np.count_nonzero(np.any(reference > 0, axis=1))),
        "queries_with_geometric_support": int(np.count_nonzero(reference_active)),
        "maximum_radiance": float(reference.max()),
        "nonzero_per_depth": [
            int(np.count_nonzero(np.any(a > 0, axis=1)))
            for a in np.array_split(reference, len(depths))
        ],
        "light_tests_selected": visits,
        "radiance_relative_max_floor_1e5": error,
        "support_count_mismatches": missing,
        "build_with_tracemalloc_ms": build_ms,
        "tracemalloc_build_peak_bytes": peak,
        "query_ms": query_ms,
        **process_memory(),
        "tracemalloc_live_allocations_after_build": live_allocations,
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "threads": {
                key: os.environ.get(key)
                for key in [
                    "OPENBLAS_NUM_THREADS",
                    "OMP_NUM_THREADS",
                    "MKL_NUM_THREADS",
                ]
            },
            "load_average": list(os.getloadavg()),
        },
        "accounting_scope": "Build-only traced peak and allocations still live after build; not cumulative allocations/copies or driver/GPU peak. Linux VmHWM resets on exec, unlike inherited getrusage high-water values. GPU payload is logical buffers only; index cap is an experiment control, not a device budget.",
        "logical_gpu_payload_bytes": storage,
        "grid": {} if grid is None else grid.stats,
        "capture_sha256": hashlib.sha256(
            (output / "captures.npz").read_bytes()
        ).hexdigest(),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in report.items()
                if k not in ["provenance", "source_ids", "scope"]
            }
        )
    )
    return 0 if missing == 0 and error <= 1e-10 else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--scene", choices=["street", "dense", "overlap", "global"], default="street"
    )
    parser.add_argument(
        "--camera", choices=["street", "turn", "ortho"], default="street"
    )
    parser.add_argument(
        "--mode", choices=["brute", "tiles", "linear", "log"], default="log"
    )
    parser.add_argument("--tile", type=int, default=64)
    parser.add_argument("--samples-x", type=int, default=96)
    parser.add_argument("--samples-y", type=int, default=54)
    parser.add_argument("--index-bytes", type=int, default=16 * 1024**2)
    parser.add_argument("--no-refine", action="store_true")
    args = parser.parse_args()
    if min(args.tile, args.samples_x, args.samples_y, args.index_bytes) <= 0:
        parser.error("positive grid, sample and storage extents required")
    if args.tile < 8 or args.samples_x * args.samples_y > 262144:
        parser.error(
            "experiment requires tiles of at least 8 pixels and at most 262144 receivers per depth"
        )
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
