"""Isolated capacity binding cost after loading input windows and metadata."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from contextlib import nullcontext
from pathlib import Path

import numpy as np
from window_capacity_catalog import rebind


def measure(product, original_catalog, capacity_catalog, output, profile=None):
    code = Path(__file__).resolve()
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            str(code),
            str(code.with_name("window_capacity_catalog.py")),
        ]
    ):
        raise ValueError("freeze cost implementation first")
    inputs = [
        product / "windows.npz",
        original_catalog / "catalog.json",
        capacity_catalog / "catalog.json",
    ]
    began = time.perf_counter()
    with np.load(inputs[0], allow_pickle=False) as f:
        plan = {k: f[k] for k in f.files}
    old, new = [json.loads(p.read_text()) for p in inputs[1:]]
    loaded = time.perf_counter() - began
    tracker = nullcontext()
    if profile is not None:
        import memray

        tracker = memray.Tracker(str(profile), trace_python_allocators=True)
    with tracker:
        began = time.perf_counter()
        result = rebind(plan, old, new)
        elapsed = time.perf_counter() - began
    keys = ("capacities", "families", "weights")
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "numpy": np.__version__,
        "instrumented": profile is not None,
        "loaded_window_bytes": sum(a.nbytes for a in plan.values()),
        "load_s": loaded,
        "binding_s": elapsed,
        "windows": len(plan["origins"]),
        "changed_families": int(
            np.count_nonzero(result["families"] != plan["families"])
        ),
        "retained_new_array_bytes": sum(result[k].nbytes for k in keys),
        "shared_unchanged_array_bytes": sum(
            v.nbytes for k, v in result.items() if k not in keys
        ),
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
        },
        "scope": "One capacity binding on already loaded data; optional Memray tracks this region only. Process peak includes imports/input. No image load, growth, organs, native/device/frame or internal copy claim.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report | {"input_sha256": "recorded in file"}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "original-catalog", "capacity-catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    measure(
        args.product,
        args.original_catalog,
        args.capacity_catalog,
        args.output,
        args.profile,
    )
