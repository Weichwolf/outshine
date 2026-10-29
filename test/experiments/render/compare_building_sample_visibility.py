"""Finite original visibility probes with an independent native centre-depth control."""

import argparse
import json
import os
import subprocess
import time
from pathlib import Path

from building_sample_inputs import capture_fields, digest, readbacks
from building_sample_originals import centre_control, read_original
from compare_building_sample_reuse import DATA
from field_view_visibility import exposed
from process_memory import peak_rss_bytes
from triangle_image_reference import prepare_source


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp output required")
    tracked = [
        Path(__file__),
        *(
            Path(__file__).with_name(n)
            for n in (
                "building_sample_originals.py",
                "building_sample_inputs.py",
                "field_view_visibility.py",
                "triangle_image_reference.py",
                "process_memory.py",
            )
        ),
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, tracked)]
    ):
        raise ValueError("Freeze the actual visibility study first")
    began, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    manifest, arrays = readbacks(args.inputs)
    grid, _ = capture_fields(*readbacks(args.inputs / "same-rays"))
    source, audit = read_original(args.inputs / "original-geometry")
    prepared = prepare_source(source)
    setup_wall, setup_cpu = time.perf_counter() - began, time.process_time() - cpu
    rows = []
    for index, case in enumerate(manifest["cases"]):
        for axis in (2, 8) if case["query"]["name"] == "px32-yaw0" else (2,):
            row = exposed(source, prepared, grid, case["query"], samples_axis=axis)
            row["native_centre_control"] = centre_control(
                row, arrays[f"case{index}_original_depth"], manifest
            )
            rows.append(row)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "geometry": audit,
        "rows": rows,
        "input_manifests": {
            str(p): digest(p / "manifest.json")
            for p in (
                args.inputs,
                args.inputs / "same-rays",
                args.inputs / "original-geometry",
            )
        },
        "cost": {
            "verify_decode_index_wall_s": setup_wall,
            "verify_decode_index_cpu_s": setup_cpu,
            "probe_wall_s": time.perf_counter() - began - setup_wall,
            "probe_cpu_s": time.process_time() - cpu - setup_cpu,
            "peak_process_rss_bytes": peak_rss_bytes(),
            "loadavg_before": load,
            "nice": os.getpriority(os.PRIO_PROCESS, 0),
            "scope": "Single diagnostic run; existing bounded256-ray broad phase, index reused. Allocation events/total-copy bytes unmeasured; no native/device runtime claim.",
        },
        "scope": "Finite CPU first-hit original geometry probes;1mm capture-hidden depth separation, all native centre disagreements reported. No converged visibility-area bound, shading reconstruction or all-error attribution.",
    }
    args.output.mkdir(parents=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"commit": report["commit"], "rows": len(rows), "output": str(args.output)}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
