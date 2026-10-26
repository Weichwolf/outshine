"""Compare exact scan/index noding on the same pinned original pool window."""

import argparse
import gc
import hashlib
import importlib.util
import json
import os
import subprocess
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import numpy as np
import pool_cell_partition
from pool_body_experiment import FIXTURE, construct, original
from pool_native_neighborhood import load_neighborhood
from pool_neighborhood_experiment import witness
from pool_region_contact import reconcile_window
from pool_terrain_contact import contact_patch
from process_memory import peak_rss_bytes
from terrain_grid_lod import triangles

REFERENCE = "d569b2b1b"
REFERENCE_PATH = "test/experiments/water/pool_cell_partition.py"
SEARCH_KEYS = (
    "edge_node_interval_visits",
    "edge_node_binary_searches",
    "edge_node_sorted_references",
)


def reference(output):
    raw = subprocess.check_output(["git", "show", f"{REFERENCE}:{REFERENCE_PATH}"])
    path = output / "scan_reference.py"
    path.write_bytes(raw)
    specification = importlib.util.spec_from_file_location("scan_reference", path)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)

    def scan(*args):
        result = module._triangulate(*args)
        return (
            *result,
            dict(zip(SEARCH_KEYS, (result[3], 0, 0), strict=True)),
        )

    return scan, hashlib.sha256(raw).hexdigest()


def timed(function):
    wall, cpu = time.perf_counter(), time.process_time()
    result = function()
    return result, {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
    }


def compare_patches(methods, body, vertices, faces):
    rows, allocations = [], {}
    for round_ in range(8):
        order = list(methods) if round_ % 2 else list(reversed(methods))
        for name in order:
            with patch.object(pool_cell_partition, "_triangulate", methods[name]):
                _, cost = timed(lambda: contact_patch(body, vertices, faces))
            rows.append(dict(method=name, round=round_, **cost))
    for name, method in methods.items():
        gc.collect()
        tracemalloc.start()
        with patch.object(pool_cell_partition, "_triangulate", method):
            result = contact_patch(body, vertices, faces)
        _, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        allocations[name] = {
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": blocks,
            "returned_numeric_bytes": sum(a.nbytes for a in result[:3]),
        }
    return rows, allocations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    baseline, digest = reference(args.output)
    methods = {"scan": baseline, "indexed": pool_cell_partition._triangulate}
    neighborhood = load_neighborhood()
    data, metadata, _, water, _ = original(FIXTURE)
    body = construct(data, metadata, water)
    witness_ids, row, col = witness(neighborhood, data)
    row, col = row // 4 * 4, col // 4 * 4
    side = len(neighborhood.heights)
    probe = neighborhood.vertices.reshape(side, side, 3)[
        row : row + 5, col : col + 5
    ].reshape(-1, 3)
    load = os.getloadavg()
    rows, allocations = compare_patches(methods, body, probe, triangles(5, 1))
    results, costs = {}, {}
    for name, method in methods.items():
        print(f"whole-window {name}", flush=True)
        with patch.object(pool_cell_partition, "_triangulate", method):
            results[name], costs[name] = timed(
                lambda: reconcile_window(body, neighborhood.vertices, side)
            )
    before, after = results.values()
    equal = [a.tobytes() == b.tobytes() for a, b in zip(before[:3], after[:3])]
    comparable = []
    for result in (before, after):
        report = dict(result[3])
        report["work"] = {
            k: v for k, v in report["work"].items() if k not in SEARCH_KEYS
        }
        comparable.append(report)
    if not all(equal) or comparable[0] != comparable[1]:
        raise ValueError("indexed noding changed original geometry or qualification")
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "reference_commit": REFERENCE,
        "reference_source_sha256": digest,
        "source": neighborhood.report,
        "load_before": load,
        "paired_patch_samples": rows,
        "representative_patch_allocations": allocations,
        "whole_window_cost": costs,
        "whole_window_work": {k: v[3]["work"] for k, v in results.items()},
        "bitwise_equal_vertices_faces_source_indices": equal,
        "existing_qualification_fields_equal": comparable[0] == comparable[1],
        "output_numeric_bytes": sum(a.nbytes for a in after[:3]),
        "process_peak_rss_before_plot_bytes": peak_rss_bytes(),
        "scope": "One paired whole-window observation and seven following paired patch samples, normal-priority Python CPU reference. Scoped tracemalloc excludes GEOS heap; live blocks are not cumulative allocations. RSS includes both alternatives and libraries. No native/device/world/p99 or memory-reduction claim.",
    }
    np.savez(
        args.output / "candidate.npz",
        vertices=after[0],
        faces=after[1],
        source_indices=after[2],
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.render:
        from pool_neighborhood_view import display

        display(body, neighborhood, after[0], after[1], witness_ids, args.output)


if __name__ == "__main__":
    main()
