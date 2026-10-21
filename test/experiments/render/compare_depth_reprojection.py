"""Frozen native depth-point reprojection errors; no native culling change."""

import argparse
import hashlib
import json
import os
import resource
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from depth_reprojection import depth_errors, forward_points, transfer_matrix
from native_depth_pair import digest, load_pair
from PIL import Image


def images(output, current, candidate):
    for name, depth in [("current", current), ("point", candidate)]:
        display = np.log1p(50000 * depth) / np.log1p(50000)
        Image.fromarray(np.rint(255 * display).astype(np.uint8)).save(
            output / f"{name}.png"
        )
    colour = np.zeros((*current.shape, 3), dtype=np.uint8)
    colour[(current > 0) & (candidate == 0)] = [40, 100, 255]
    colour[candidate - current > 1e-6] = [255, 50, 20]
    colour[(candidate > 0) & (current == 0)] = [255, 255, 0]
    Image.fromarray(colour).save(output / "errors.png")


def run(args):
    here = Path(__file__).resolve().parent
    names = ["depth_reprojection.py", "native_depth_pair.py", Path(__file__).name]
    sources = [here / name for name in names]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, sources),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze the measured point-control sources first")
    args.output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    (old, current), hashes = load_pair(args.input)
    read_seconds = time.perf_counter() - began
    if old["source_anchor"] != current["source_anchor"]:
        raise ValueError(
            "This finite control does not infer changed source-anchor semantics"
        )
    transform = transfer_matrix(
        old["clip"], current["clip"], current["pre_view"] - old["pre_view"]
    )
    if args.trace:
        tracemalloc.start()
    began = time.perf_counter()
    candidate, work = forward_points(old["depth"], transform)
    seconds = time.perf_counter() - began
    peak = tracemalloc.get_traced_memory()[1] if args.trace else None
    if args.trace:
        tracemalloc.stop()
    errors = depth_errors(candidate, current["depth"])
    np.savez_compressed(
        args.output / "depths.npz",
        old=old["depth"],
        current=current["depth"],
        point=candidate,
    )
    images(args.output, current["depth"], candidate)
    report = {
        "producer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "implementation_sha256": {p.name: digest(p) for p in sources},
        "input_sha256": hashes,
        "frames": [old["frame"], current["frame"]],
        "shape": list(candidate.shape),
        "unchanged_source_generations": old["generation"] == current["generation"],
        "relative_pre_view_change_m": (current["pre_view"] - old["pre_view"]).tolist(),
        "clip_transfer": transform.tolist(),
        "read_validation_seconds": read_seconds,
        "point_transfer_seconds": seconds,
        "traced_peak_bytes": peak,
        "trace": args.trace,
        "host_load_average": os.getloadavg(),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "work": work,
        "errors": errors,
        "candidate_sha256": hashlib.sha256(candidate.tobytes()).hexdigest(),
        "scope": "Unsafe point-to-target-cell control, not conservative area coverage. Native source matrices/D32 values retained; transfer/interpolation is Double CPU, not shader replay. Current full depth used only after candidate generation for diagnosis, never a repair. No target work saving, GPU/device/p99/whole-world qualification. Unchanged generation does not certify dynamic sources or sample coverage.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "input": str(args.input),
                "errors": errors,
                "producer": report["producer_commit"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    run(parser.parse_args())
