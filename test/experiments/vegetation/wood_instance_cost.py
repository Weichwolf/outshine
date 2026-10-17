"""Scoped CPU array preparation and unchanged expanded-buffer comparison."""

import argparse
import gc
import hashlib
import importlib.util
import json
import subprocess
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from window_portrait_source import source, wood
from window_wood_instances import groups


def profile(function, graph, axes, arrays):
    times = []
    for _ in range(3):
        start = time.perf_counter()
        result = function(graph, axes)
        times.append(time.perf_counter() - start)
        del result
    gc.collect()
    tracemalloc.start()
    result = function(graph, axes)
    live, peak = tracemalloc.get_traced_memory()
    snapshot = tracemalloc.take_snapshot()
    blocks = sum(s.count for s in snapshot.statistics("traceback"))
    tracemalloc.stop()
    return {
        "three_run_median_s": float(np.median(times)),
        "one_call_traced_live_bytes": live,
        "one_call_traced_peak_bytes": peak,
        "one_call_traced_live_blocks": blocks,
        "returned_array_bytes": sum(a.nbytes for a in arrays(result)),
    }


def compare(product, catalog):
    graph, axes, _, _, evidence = source(product, product / "execution", catalog)
    frozen = subprocess.check_output(
        [
            "git",
            "show",
            "48b14fba8:test/experiments/vegetation/window_portrait_source.py",
        ]
    )
    with tempfile.TemporaryDirectory(
        prefix="outshine-vegetation-old-wood-", dir="/tmp"
    ) as temp:
        path = Path(temp) / "reference.py"
        path.write_bytes(frozen)
        spec = importlib.util.spec_from_file_location("frozen_wood", path)
        previous = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(previous)
    before, after = previous.wood(graph, axes), wood(graph, axes)
    fields = {}
    for name in before:
        np.testing.assert_array_equal(before[name], after[name])
        assert before[name].dtype == after[name].dtype
        fields[name] = {
            "shape": after[name].shape,
            "dtype": after[name].dtype.str,
            "sha256": hashlib.sha256(after[name].tobytes()).hexdigest(),
        }
    del before, after
    return {
        "identity": evidence["identity"],
        "reference_source_sha256": hashlib.sha256(frozen).hexdigest(),
        "unchanged_expanded_arrays": fields,
        "expanded_explicit_blender_write_bytes": fields["vertices"]["shape"][0] * 12
        + fields["faces"]["shape"][0] * 56,
        "expanded": profile(wood, graph, axes, lambda r: r.values()),
        "instanced": profile(
            groups,
            graph,
            axes,
            lambda r: [
                a
                for _, mesh, ids, origins, frames, scales in r
                for a in (*mesh.values(), ids, origins, frames, scales)
            ],
        ),
        "scope": "Same-process sequential CPU preparation under stated host load; no Blender binding/dependency-graph/GPU time. Traced Python/NumPy live/peak/block counts, not total allocation events or cumulative copied bytes. Instance result retains source pose buffers and IDs but never expands per-axis vertices/normals.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("plant-product", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.plant_product, args.catalog)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
