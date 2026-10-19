"""Fresh-process Python mask preparation costs, separate from native tracing."""

import argparse
import gc
import hashlib
import json
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_cube_capture_masks import face_rectangles, load_original
from cube_capture_mask import occupied_tiles, rectangles, tile_shape, word_masks


def kernel_memory():
    fields = {}
    path = Path("/proc/self/status")
    if path.exists():
        for line in path.read_text().splitlines():
            if line.startswith(("VmHWM:", "VmRSS:")):
                name, value, unit = line.split()
                if unit != "kB":
                    raise ValueError("explicit Linux kernel memory units required")
                fields[name.removesuffix(":") + "_bytes"] = int(value) * 1024
    return fields


def dense_tiles(tiles, values, size):
    masks = {
        tile: np.zeros(tile_shape(tile, size), dtype=bool) for tile in sorted(tiles)
    }
    fills, visits = 0, 0
    for left, top, right, bottom in rectangles(values, size):
        for y in range(top // 64, (bottom - 1) // 64 + 1):
            for x in range(left // 64, (right - 1) // 64 + 1):
                visits += 1
                if (y, x) not in masks:
                    continue
                a, b = max(left - x * 64, 0), min(right - x * 64, 64)
                c, d = max(top - y * 64, 0), min(bottom - y * 64, 64)
                masks[y, x][c:d, a:b] = True
                fills += (b - a) * (d - c)
    return masks, {
        "tile_pair_visits": visits,
        "selected_rays": sum(int(mask.sum()) for mask in masks.values()),
        "row_array_allocations": len(masks),
        "array_payload_zeroed_bytes": sum(mask.nbytes for mask in masks.values()),
        "logical_fill_write_bytes": fills,
    }


def measure(manifest, trace_path, axis, sign, mode):
    data, trace = load_original(manifest, trace_path)
    calls = trace["results"][0]["projection_calls"]
    if len(calls) != 1 or not calls[0]["accepted"]:
        raise ValueError("one successful original Whole projection group required")
    eye = np.asarray(trace["view"]["eye_ecef"]) - data["anchor_ecef"]
    size = int(np.ceil(4 * trace["view"]["focal_px"] / trace["view"]["error_px"]))
    original, demand, _ = face_rectangles(
        data["summaries"],
        calls[0]["members"],
        eye,
        axis,
        sign,
        size,
        include_fine=False,
    )
    gc.collect()
    before_kernel = kernel_memory()
    tracemalloc.start()
    start = time.perf_counter()
    tiles = occupied_tiles(original, size)
    if mode == "coarse":
        payload = tiles
        work = {
            "selected_rays": sum(
                np.prod(tile_shape(tile, size)).item() for tile in tiles
            ),
            "row_array_allocations": 0,
            "array_payload_zeroed_bytes": 0,
        }
    elif mode == "word":
        payload, work = word_masks(tiles, demand, size)
        if work["budget_exhausted"]:
            raise ValueError("measurement bitmap budget exhausted")
        work["array_payload_zeroed_bytes"] = work["row_array_bytes"]
        work["logical_or_read_bytes"] = work["row_word_ors"] * 8
        work["logical_or_write_bytes"] = work["row_word_ors"] * 8
    else:
        payload, work = dense_tiles(tiles, demand, size)
    elapsed = time.perf_counter() - start
    current, peak = tracemalloc.get_traced_memory()
    snapshot = tracemalloc.take_snapshot()
    tracemalloc.stop()
    after_kernel = kernel_memory()
    if not payload:
        raise ValueError("measured face must actually contain original tiles")
    return {
        "parent": trace["parent"],
        "mode": mode,
        "axis": axis,
        "sign": sign,
        "dimension": size,
        "work": work,
        "python_instrumented_preparation_s": elapsed,
        "python_tracked_current_bytes": current,
        "python_tracked_peak_bytes": peak,
        "python_live_tracked_blocks": sum(
            stat.count for stat in snapshot.statistics("traceback")
        ),
        "kernel_memory_before": before_kernel,
        "kernel_memory_after": after_kernel,
        "original_source_arrays_unchanged": True,
        "inputs_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (manifest, trace_path)
        },
        "scope": "Python-instrumented mask preparation only, after identical source/rectangle acquisition. Coarse is the existing tile admission without any pixel mask; dense/word are additional work and storage. Tracked live blocks are not total allocation calls. Logical payload writes are not measured bus traffic. Kernel VmHWM is since exec and includes imports, input acquisition and measurement/snapshot overhead; before/after HWM is not an isolated mask peak. No native time, memory, copy, GPU or device acceptance; native full sample grids remain unchanged.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--axis", type=int, choices=(0, 1, 2), required=True)
    parser.add_argument("--sign", type=int, choices=(-1, 1), required=True)
    parser.add_argument("--mode", choices=("coarse", "word", "dense"), required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    result = measure(args.input, args.trace, args.axis, args.sign, args.mode)
    result["code_sha256"] = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in (
            Path(__file__).name,
            "cube_capture_mask.py",
            "compare_cube_capture_masks.py",
        )
    }
    target.mkdir(parents=True)
    (target / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "parent",
                    "mode",
                    "python_tracked_peak_bytes",
                    "python_instrumented_preparation_s",
                )
            }
        ),
        flush=True,
    )
