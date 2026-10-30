"""Compare bounded filtered over with exact points, charging background preparation."""

import argparse
import gc
import hashlib
import json
import os
import statistics
import subprocess
import time
from pathlib import Path

import numpy as np
from experiment_output import create_output
from field_native_fixture import DATA, QUERIES, read_fixture


def cells(values):
    height, width = values.shape[:2]
    if height % 4 or width % 4:
        raise ValueError("Complete four-by-four point supports required")
    return values.reshape(height // 4, 4, width // 4, 4, *values.shape[2:]).transpose(
        0, 2, 1, 3, *range(4, values.ndim + 2)
    )


def reduced(values):
    return cells(values).mean((2, 3))


def filtered(front_mean, back_mean, back_span):
    p = front_mean[..., 3:]
    return front_mean + (1 - p) * back_mean, p * (1 - p) * back_span


def background(values):
    blocks = cells(values)
    return blocks.mean((2, 3)), blocks.max((2, 3)) - blocks.min((2, 3))


def adaptive(front_mean, back):
    p = front_mean[..., 3:]
    mixed = (p[..., 0] > 0) & (p[..., 0] < 1)
    selected = cells(back)[mixed]
    span = selected.max((1, 2)) - selected.min((1, 2))
    back_mean = reduced(back)
    bound = np.zeros_like(back_mean)
    bound[mixed] = (p * (1 - p))[mixed] * span
    return front_mean + (1 - p) * back_mean, bound


def analyze(samples, query, role):
    source, outside = (samples[key, query] for key in ("stored", role))
    original = samples["original", query]
    a, b, original_a = (
        value.rgba.astype(float) for value in (source, outside, original)
    )
    if not np.array_equal(source.reverse_z, original.reverse_z) or not np.array_equal(
        a[..., 3], original_a[..., 3]
    ):
        raise ValueError("Existing same native source depth/coverage required")
    ordered_points = (
        (a[..., 3] == 0) | (b[..., 3] == 0) | (source.reverse_z > outside.reverse_z)
    )
    ordered = cells(ordered_points).all((2, 3))
    choose_source = source.reverse_z > outside.reverse_z
    direct = reduced(samples["union-" + role, query].rgba.astype(float))
    exact = reduced(np.where(choose_source[..., None], original_a, b))
    if not np.array_equal(exact, direct):
        raise ValueError("Independent whole native union and original points differ")
    a_mean = reduced(a)
    approx, bound = filtered(a_mean, *background(b))
    source_error = reduced(abs(original_a - a))
    delta = abs(approx - direct)
    combined = bound + source_error
    excess = delta[ordered] - combined[ordered]
    if np.any(excess > 0):
        raise ValueError("Observed finite envelope violated; no tolerance relaxation")
    energy = reduced(original_a)[ordered, :3].sum()
    return {
        "query": query,
        "outside": role,
        "qualified_point_supports": int(ordered.sum()),
        "interleaved_supports_refused": int((~ordered).sum()),
        "mixed_qualified_supports": int(
            np.count_nonzero(ordered & (a_mean[..., 3] > 0) & (a_mean[..., 3] < 1))
        ),
        "observed_rgb_l1": float(delta[ordered, :3].sum()),
        "real_covariance_rgb_l1_bound": float(bound[ordered, :3].sum()),
        "observed_source_rgb_l1_bound": float(source_error[ordered, :3].sum()),
        "combined_relative_to_qualified_source_rgb": float(
            combined[ordered, :3].sum() / energy
        ),
        "observed_relative_to_qualified_source_rgb": float(
            delta[ordered, :3].sum() / energy
        ),
        "observed_max_envelope_excess": float(excess.max()),
        "general_float_certificate": False,
    }


def costs(samples, output, pairs, profile):
    a, b = (samples[key, "viewer"].rgba.astype(float) for key in ("stored", "terrain"))
    z_a, z_b = (samples[key, "viewer"].reverse_z for key in ("stored", "terrain"))
    if np.any((a[..., 3] > 0) & (b[..., 3] > 0) & (z_a <= z_b)):
        raise ValueError("Entire cost-control support must be source-first")
    a_mean, (b_mean, span) = reduced(a), background(b)
    operations = {
        "joint_points": lambda: reduced(a + (1 - a[..., 3:]) * b),
        "prepare_all_background_ranges": lambda: filtered(a_mean, *background(b)),
        "prepare_mixed_background_ranges": lambda: adaptive(a_mean, b),
        "held_means_and_ranges_only": lambda: filtered(a_mean, b_mean, span),
    }
    expected_joint = operations["joint_points"]()
    expected_filtered = operations["prepare_all_background_ranges"]()

    def check(name, value):
        if name == "joint_points":
            same = np.array_equal(value, expected_joint)
        else:
            same = all(
                np.array_equal(v, e)
                for v, e in zip(value, expected_filtered, strict=True)
            )
        if not same:
            raise ValueError("Cost operation changed its complete finite output")

    result = {name: {"rounds": []} for name in operations}
    for pair in range(pairs):
        order = tuple(operations) if pair % 2 == 0 else tuple(operations)[::-1]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = operations[name]()
            note = {
                "cpu_s": time.process_time() - cpu,
                "wall_s": time.perf_counter() - wall,
            }
            check(name, value)
            result[name]["rounds"].append(note)
            del value
    for name, operation in operations.items():
        note = result[name]
        for kind in ("cpu_s", "wall_s"):
            note["median_" + kind] = statistics.median(
                row[kind] for row in note["rounds"]
            )
        if profile:
            import memray

            path = output / (name + ".bin")
            gc.collect()
            with memray.Tracker(str(path), trace_python_allocators=True):
                value = operation()
            check(name, value)
            del value
            reader = memray.FileReader(path)
            events = requested = 0
            for record in reader.get_allocation_records():
                if record.size > 0:
                    events += record.n_allocations
                    requested += record.size
            note["profile"] = {
                "allocation_events": events,
                "requested_bytes": requested,
                "scoped_peak_heap_bytes": reader.metadata.peak_memory,
                "trace_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
    p = a_mean[..., 3]
    return {
        "methods": result,
        "point_samples": len(a.reshape(-1, 4)),
        "filtered_supports": len(a_mean.reshape(-1, 4)),
        "mixed_range_samples": int(np.count_nonzero((p > 0) & (p < 1))) * 16,
        "source_mean_bytes": a_mean.nbytes,
        "joint_output_bytes": expected_joint.nbytes,
        "held_background_mean_span_bytes": b_mean.nbytes + span.nbytes,
        "scope": "Float64 evaluation of identical held finite native Float32 operands. Source mean preparation excluded from all methods. Full/adaptive paths charge background mean/range preparation; held query explicitly excludes it. Common source conversion, input/geometry loading, validation and checks are excluded. Profiles are scoped incremental allocation, not held/process/UMA peaks. Exact output has zero composition approximation; other paths have the reported finite envelope. No native/device/frame claim.",
    }


def run(args):
    create_output(args.output)
    manifest, status, samples, work = read_fixture(args.inputs)
    rows = [
        analyze(samples, query, role)
        for query in QUERIES
        for role in ("terrain", "external")
    ]
    note = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "native_commit": manifest["native_commit"],
        "reference_status": status,
        "input_work": work,
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "cases": rows,
        "costs": costs(samples, args.output, args.pairs, args.profile),
        "scope": "Real-arithmetic covariance derivation and observed finite native opponent comparison; not a rounded arithmetic or general source/pose/World certificate. Known native repeatability defect remains visible. No new GPU run, admission policy or runtime path.",
    }
    (args.output / "report.json").write_text(json.dumps(note, indent=2) + "\n")
    print(
        json.dumps(
            {
                name: row["median_cpu_s"]
                for name, row in note["costs"]["methods"].items()
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 10), default=7)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    run(args)
