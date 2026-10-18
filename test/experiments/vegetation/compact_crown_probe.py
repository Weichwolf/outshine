"""Actual original90k poses, compact continuous groups and explicit error/cost proof."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compact_crown import CompactCrown
from crown_demand import CrownDemand
from crown_demand_probe import cameras
from spatial_group_source import load_source


def output_digest(data):
    digest = hashlib.sha256()
    for key in ("ids", "variants", "origins", "frames"):
        digest.update(data[key].tobytes())
    return digest.hexdigest()


def quality(source, product, approximate):
    exact = product.evaluate(
        np.ones(len(product.data["hosts"]), bool), max_leaves=100000, anchors=True
    )
    reduced = approximate.evaluate(
        np.ones(len(product.data["hosts"]), bool), max_leaves=100000, anchors=True
    )
    maximum = {key: 0.0 for key in ("origins", "frames", "nodes")}
    position_norm = 0.0
    normal_angle = 0.0
    # Independent original group path, no compact kernel in the comparator.
    for i, node in enumerate(source.crown.hosts):
        source.crown._groups.clear()
        original = source.crown.group(int(node), 100000)
        lo, hi = map(int, product.data["offsets"][i : i + 2])
        for key in exact:
            assert exact[key][lo:hi].tobytes() == original[key].tobytes(), key
        for key in maximum:
            error = abs(reduced[key][lo:hi].astype(float) - original[key])
            maximum[key] = max(maximum[key], float(error.max(initial=0)))
        delta = reduced["origins"][lo:hi].astype(float) - original["origins"]
        position_norm = max(position_norm, float(np.linalg.norm(delta, axis=1).max()))
        a = reduced["frames"][lo:hi].astype(float).reshape(-1, 3, 3)
        b = original["frames"]
        # atan2(cross,dot) avoids acos amplification at nearly equal normals.
        cross = np.cross(a.swapaxes(1, 2), b.swapaxes(1, 2))
        dot = np.einsum("nij,nij->ni", a.swapaxes(1, 2), b.swapaxes(1, 2))
        angle = np.arctan2(np.linalg.norm(cross, axis=-1), dot)
        normal_angle = max(normal_angle, float(angle.max()))
    if maximum["origins"] > 1e-5 or normal_angle > 1e-4:
        raise AssertionError("predeclared Float32 source position/frame error exceeded")
    return {
        "original_leaf_rows_bit_identical": len(exact["ids"]),
        "float64_output_sha256": output_digest(exact),
        "float32_maximum_absolute_component_error": maximum,
        "float32_maximum_position_distance_m": position_norm,
        "float32_maximum_basis_direction_angle_rad": normal_angle,
        "float32_position_component_limit_m": 1e-5,
        "float32_basis_angle_limit_rad": 1e-4,
    }


def memory_call(function):
    tracemalloc.start()
    before = tracemalloc.take_snapshot()
    value = function()
    current, peak = tracemalloc.get_traced_memory()
    after = tracemalloc.take_snapshot()
    tracemalloc.stop()
    positive = [v for v in after.compare_to(before, "lineno") if v.count_diff > 0]
    return {
        "final_array_bytes": sum(v.nbytes for v in value.values()),
        "traced_current_bytes": current,
        "traced_peak_bytes": peak,
        "positive_live_allocation_count": sum(v.count_diff for v in positive),
        "output_sha256": output_digest(value),
    }


def query_costs(source, product):
    plan = CrownDemand(source)
    product32 = product.quantized()
    records = []
    for name, camera in cameras().items():
        query = plan.query((camera,), max_leaves=100000)
        selected = query["detail"]
        # Same prebuilt bounds; each timed path repeats the identical camera query.
        for method in ("original", "compact64", "compact32"):

            def call(method=method):
                if method == "original":
                    _, poses = plan.prepare((camera,), max_leaves=100000)
                    return poses
                candidate = product if method == "compact64" else product32
                request = plan.query((camera,), max_leaves=100000)
                return candidate.evaluate(request["detail"], max_leaves=100000)

            times = []
            for _ in range(3):
                start = time.perf_counter()
                output = call()
                times.append(time.perf_counter() - start)
            records.append(
                {
                    "view": name,
                    "method": method,
                    "selected_hosts": int(selected.sum()),
                    "leaf_poses": len(output["ids"]),
                    "median_s": float(np.median(times)),
                    **memory_call(call),
                }
            )
    return records


def run(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    start = time.perf_counter()
    with (
        patch("crown_shoots.CrownShoots.group", side_effect=AssertionError("poses")),
        patch("outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError),
    ):
        product = CompactCrown.build(source)
    build_s = time.perf_counter() - start
    approximate = product.quantized()
    for name, value in (("compact64", product), ("compact32", approximate)):
        value.save(output / f"{name}.npz")
    proof = quality(source, product, approximate)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "build_s": build_s,
        "parameter_array_bytes": {
            "float64": sum(v.nbytes for v in product.data.values()),
            "float32": sum(v.nbytes for v in approximate.data.values()),
        },
        "per_field_parameter_bytes": {k: v.nbytes for k, v in product.data.items()},
        "quality": proof,
        "cases": query_costs(source, product),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Experimental original-host scalar parameters plus shared original arc tables; Float64 bitwise source equality and separate Float32 approximation. CPU pose expansion for verification, no native/GPU runtime claim. Traced live counts/peaks not cumulative allocations or traffic. Shared-host wall times, no shadow/wind/forest/LOD admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run(args.inputs, args.output)
