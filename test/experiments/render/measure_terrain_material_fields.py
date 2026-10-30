"""Exact material costs: real point interpretation and separate bounded field reuse."""

import argparse
import gc
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from probe_terrain_material_substrate import case, prepared_sources, query
from replay_terrain_material_observations import METHODS
from terrain_material_capture import load
from terrain_material_observations import evaluate_capture


def allocated(call, trace):
    import memray

    gc.collect()
    with memray.Tracker(str(trace), trace_python_allocators=True, native_traces=True):
        value = call()
    reader = memray.FileReader(str(trace))
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    count, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in frees and event.size > 0:
            count += event.n_allocations
            requested += event.size
    return value, {
        "allocation_events": count,
        "requested_bytes": requested,
        "scoped_high_watermark_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "trace": trace.name,
        "profiler": "memray " + memray.__version__,
    }


def compare(methods, output, prefix):
    names = tuple(methods)
    reference = methods[names[0]]()
    records = {name: {"rounds": []} for name in names}
    for iteration in range(9):
        order = names[iteration % len(names) :] + names[: iteration % len(names)]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            timing = {
                "cpu_s": time.process_time() - cpu,
                "wall_s": time.perf_counter() - wall,
            }
            if (
                value.dtype != reference.dtype
                or value.shape != reference.shape
                or value.tobytes() != reference.tobytes()
            ):
                raise ValueError("equal-contract full material result changed")
            records[name]["rounds"].append(timing)
            del value
    for name in names:
        value, allocations = allocated(
            methods[name], output / (prefix + "-" + name + ".bin")
        )
        if value.tobytes() != reference.tobytes():
            raise ValueError("allocation-profiled material result changed")
        records[name].update(allocations)
        records[name]["cpu_median_ms"] = 1000 * float(
            np.median([r["cpu_s"] for r in records[name]["rounds"]])
        )
        records[name]["wall_median_ms"] = 1000 * float(
            np.median([r["wall_s"] for r in records[name]["rounds"]])
        )
        records[name]["returned_array_bytes"] = value.nbytes
    return records


def prepare_patch(data, extent_m):
    bounds = [[1000.1, -200.2], [1000.1 + extent_m, -200.2 + extent_m]]
    values, footprint = data["values"], max(data["steps"])
    coverage, coarse = values["coverage"], values["recipe"][2]
    mask_needed = 0 < coverage < 1 and coarse > 0 and footprint < 2 * coarse
    base, base_records, base_fields = prepared_sources(
        bounds,
        footprint,
        values["recipe"],
        enabled=coverage < 1,
        mask_needed=mask_needed,
    )
    top, top_records, top_fields = prepared_sources(
        bounds, footprint, values["litter_recipe"], enabled=coverage > 0
    )
    return {
        "base_sources": base,
        "litter_sources": top,
        "mask_source": base[0] if mask_needed else data["mask_source"],
        "fields": base_fields + top_fields,
        "records": {"base": base_records, "litter": top_records},
    }


def actual_points(capture, output, index):
    methods = {
        name: (
            lambda p=prepared, s=selected: evaluate_capture(
                capture, prepared=p, prune_support=s
            )[0]
        )
        for name, prepared, selected in METHODS
    }
    report = compare(methods, output, f"actual-{index}")
    for name, prepared, selected in METHODS:
        _, work = evaluate_capture(capture, prepared=prepared, prune_support=selected)
        report[name]["work"] = work
        report[name]["known_frozen_backing_copy_lower_bound_bytes"] = (
            work["singleton_lattice_bytes_constructed"]
            + work["singleton_plane_bytes_constructed"]
        )
    return {
        "scene": capture.metadata["scene"],
        "input_sha256": capture.input_sha256,
        "palette_sha256": capture.metadata["palette_sha256"],
        "query_count": len(capture.points),
        "scope": "Real instrumented observations, full ideal-Double material interpretation including Python diagnostic counters. Singleton lookup includes pointwise field construction; no prepared hit or native/GPU timing.",
        "methods": report,
    }


def reused_patch(capture, output, index, row, extent_m, sign):
    words = capture.palette_words
    layout = {"rows": int(words[0]), "layer_offset_floats": int(words[3])}
    data = case(capture.palette, layout, row, extent_m, sign)
    prepare_call = lambda: prepare_patch(data, extent_m)
    cpu, wall = time.process_time(), time.perf_counter()
    initial = prepare_call()
    build = {
        "single_cpu_s": time.process_time() - cpu,
        "single_wall_s": time.perf_counter() - wall,
    }
    del initial
    prepared, allocations = allocated(prepare_call, output / f"patch-{index}-build.bin")
    build.update(allocations)
    data.update(prepared)
    selection = np.linspace(0, len(data["at"]) - 1, 2048, dtype=np.int64)
    for name in ("at", "dx", "dy"):
        data[name] = data[name][selection].copy()
    methods = {
        "analytic": lambda: query(data, prepared=False),
        "original_words": lambda: query(data, prepared=True),
    }
    costs = compare(methods, output, f"patch-{index}")
    owned = sum(f.retained_bytes for f in data["fields"])
    return {
        "row": row,
        "extent_control_m": extent_m,
        "slope_sign": sign,
        "query_count": 2048,
        "palette_sha256": capture.metadata["palette_sha256"],
        "owned_field_bytes": owned,
        "known_frozen_backing_copy_lower_bound_bytes": owned,
        "preserved_full_patch_footprint_uv_m": data["steps"].tolist(),
        "request_selection": data["records"],
        "field_build": build,
        "methods": costs,
        "scope": "Current actual captured palette; controlled positions and footprint. Fields use two known patch bounds before raster work. Repeated 2048-query subset of the original 256x192 patch; field preparation/input construction excluded from query timing. This is reuse, not an asset DB hit, source-bound native working set or GPU sampling.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", str(Path(__file__).parent)], text=True
    ):
        raise ValueError("freeze the material producer before measuring")
    args.output.mkdir(parents=True, exist_ok=False)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "platform": platform.platform(),
        "loadavg_start": os.getloadavg(),
        "normal_priority": os.getpriority(os.PRIO_PROCESS, 0),
        "actual_inputs": [],
        "controlled_reuse": [],
        "total_internal_copied_bytes": None,
        "native_gpu_physical_transactions": None,
        "scope": "Nine rotating normal-priority CPU rounds; existing inputs excluded, separate Memray/native-stack traces. Scoped allocations/high watermark exclude previously held source/products; not RSS/UMA. All six returned channels must be byte-exact. Preparation, repeated query and pointwise construction reported separately; no native/frame/IO/DB/working-set admission.",
    }
    captures = [load(path) for path in args.input]
    for index, capture in enumerate(captures):
        report["actual_inputs"].append(actual_points(capture, args.output, index))
    for index, (row, extent, sign) in enumerate(((0, 0.05, 1), (3, 8.0, -1))):
        report["controlled_reuse"].append(
            reused_patch(captures[0], args.output, index, row, extent, sign)
        )
    report["loadavg_end"] = os.getloadavg()
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "state": "all_exact",
                "actual_input_sets": len(captures),
                "controlled_patches": 2,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
