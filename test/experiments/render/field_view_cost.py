"""Same-field rectangle enumeration versus held Float32 summed-area lookups."""

import argparse
import gc
import hashlib
import json
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from field_view_inputs import read_view
from field_view_integrals import box_integrals, summed_field
from field_view_sampling import ImageView, reconstruct, source_corners, support


def direct_integrals(source, lower, upper):
    result = np.zeros((len(lower), 4), dtype=float)
    visits = 0
    for i, (low, high) in enumerate(zip(lower, upper, strict=True)):
        start = np.clip(np.floor(low).astype(int), 0, source.shape[1::-1])
        end = np.clip(np.ceil(high).astype(int), 0, source.shape[1::-1])
        if np.any(end <= start):
            continue
        x, y = (np.arange(a, b) for a, b in zip(start, end, strict=True))
        wx = np.maximum(0, np.minimum(x + 1, high[0]) - np.maximum(x, low[0]))
        wy = np.maximum(0, np.minimum(y + 1, high[1]) - np.maximum(y, low[1]))
        result[i] = np.einsum(
            "i,j,ijk->k",
            wy,
            wx,
            source[start[1] : end[1], start[0] : end[0]],
            dtype=float,
        )
        visits += len(x) * len(y)
    return result, visits


def measured(call, output, name):
    import memray

    rounds = []
    for _ in range(7):
        gc.collect()
        start, wall = time.process_time(), time.perf_counter()
        value = call()
        rounds.append((time.process_time() - start, time.perf_counter() - wall))
        del value
    gc.collect()
    trace = output / (name + ".bin")
    with memray.Tracker(str(trace), trace_python_allocators=True, native_traces=True):
        value = call()
    reader = memray.FileReader(str(trace))
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    allocations, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in frees and event.size > 0:
            allocations += event.n_allocations
            requested += event.size
    return value, {
        "process_cpu_median_s": float(np.median([r[0] for r in rounds])),
        "wall_median_s": float(np.median([r[1] for r in rounds])),
        "rounds": rounds,
        "allocation_events": allocations,
        "requested_bytes": requested,
        "scoped_peak_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "trace": trace.name,
        "memray": memray.__version__,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--held-query", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    sources = [
        Path(__file__).with_name(name)
        for name in (
            "field_view_cost.py",
            "field_view_inputs.py",
            "field_view_integrals.py",
            "field_view_sampling.py",
            "field_view_projection.py",
            "compare_vertex_field_words.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, sources)], text=True
    ):
        raise ValueError("Freeze the measured producer before recording costs")
    args.output.mkdir(parents=True, exist_ok=False)
    states = json.loads((args.field / "radiance-result.json").read_text())["cases"]
    state = next(s for s in states if s["query"] == args.held_query)
    query = next(
        q
        for q in json.loads((args.field / "input.json").read_text())["queries"]
        if q["name"] == args.held_query
    )
    atlas = read_view(args.field / "world-view-assets.sqlite", state["field_asset_key"])
    n, grid = atlas["pixels"], atlas["grid"]
    source = np.fromfile(
        args.field / (args.held_query + "-source-field.f32"), dtype="<f4"
    ).reshape(n, n, 4)
    view = ImageView(
        tuple(query["eye"]), tuple(map(tuple, query["basis"])), query["focal_px"]
    )
    low, high = support(grid, view, grid.axes(), 16384)
    y, x = np.mgrid[low[1] : high[1], low[0] : high[0]]
    uv = source_corners(
        grid, view, grid.axes(), np.column_stack((x.ravel(), y.ravel())), (n, n)
    )
    lower, upper = uv.min(1), uv.max(1)
    field, build = measured(lambda: summed_field(source), args.output, "build-prefix")
    direct, enumeration = measured(
        lambda: direct_integrals(source, lower, upper), args.output, "enumerate-boxes"
    )
    lookup, prefix = measured(
        lambda: box_integrals(field, lower, upper), args.output, "prefix-boxes"
    )
    _, complete = measured(
        lambda: reconstruct(field, grid, view), args.output, "held-complete-view"
    )
    error = float(abs(direct[0] - lookup).max())
    if not np.allclose(direct[0], lookup, atol=0.01, rtol=1e-4):
        raise ValueError("The alternative does not integrate the same source boxes")
    record = {
        "commit": revision,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "field_key": atlas["key"],
        "raw_view_sha256": atlas["sha256"],
        "lit_source_sha256": hashlib.sha256(source.tobytes()).hexdigest(),
        "source_texels": n * n,
        "output_pixels": len(lower),
        "profile": [1280, 720],
        "direct_source_cell_visits": direct[1],
        "prefix_evaluations": 4 * len(lower),
        "logical_prefix_rgba_fetches": 16 * len(lower),
        "source_bytes": source.nbytes,
        "raw_view_bytes": atlas["raw_bytes"],
        "prefix_bytes": field.table.nbytes,
        "maximum_integral_delta": error,
        "maximum_pixel_delta": float(
            np.max(abs(direct[0] - lookup) / np.prod(upper - lower, axis=1)[:, None])
        ),
        "known_owned_prefix_copy_bytes": field.table.nbytes,
        "costs": {
            "build": build,
            "enumeration": enumeration,
            "lookup": prefix,
            "held_complete_view": complete,
        },
        "scope": "Same held already-lit source and pixel boxes. CPU times exclude instrumentation; native-allocation traces are separate. Scoped peaks exclude preexisting inputs/retained field. Table built once per changed source/view/material/light/shader; no pose rebuild within qualified domain. Prefix-copy count is a lower bound; NumPy temporaries, original geometry/GPU work, total process/UMA, driver reserves and native device timings remain separate/unmeasured.",
    }
    (args.output / "report.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
