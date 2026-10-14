"""Fresh workers, common feasible sources and identical chord-error/grade requirements."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from outshine_ref.transport import plan_vertical_transition, sample_vertical_transition

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/vertical-profiles"
INPUT = OUTPUT / "comparison.json"


def worker(method):
    report = json.loads(INPUT.read_text())
    rows = [
        row
        for row in report["cases"]
        if all(row["methods"][m]["valid"] for m in ("parabolic", "cubic"))
    ]
    cases = [
        (row["target_heights_m"], row["methods"][method]["length_m"], row["grade"])
        for row in rows
    ]
    warm = time.perf_counter_ns()
    plan_vertical_transition(0.0, 1.0, 100.0, method=method)
    warm = time.perf_counter_ns() - warm
    timings, meshes = [], []
    for _ in range(7):
        prepared, sampled = [], []
        began = time.perf_counter_ns()
        for z, length, grade in cases:
            profile, _note = plan_vertical_transition(
                *z, length, grade=grade, method=method
            )
            if profile is None:
                raise ValueError("common feasible source unexpectedly failed")
            prepared.append(profile)
        plan_ns = time.perf_counter_ns() - began
        began = time.perf_counter_ns()
        for profile in prepared:
            table, _note = sample_vertical_transition(profile, height_error_m=0.01)
            if table is None:
                raise ValueError("common sampling budget unexpectedly failed")
            sampled.append(table)
        sample_ns = time.perf_counter_ns() - began
        timings.append({"plan_batch_ns": plan_ns, "sample_batch_ns": sample_ns})
        meshes.append(
            hashlib.sha256(b"".join(t.tobytes() for t in sampled)).hexdigest()
        )
    if len(set(meshes)) != 1:
        raise ValueError("repeated outputs changed")
    memory = [
        line
        for line in Path("/proc/self/status").read_text().splitlines()
        if line.startswith("VmHWM:")
    ]
    return {
        "method": method,
        "common_cases": len(cases),
        "source_ids": [r["source_id"] for r in rows],
        "samples": timings,
        "first_profile_import_and_prepare_ns": warm,
        "sampled_vertices": sum(len(t) for t in sampled),
        "profile_digest": meshes[0],
        "current_exec_peak_rss_bytes": int(memory[0].split()[1]) * 1024,
        "inherited_ru_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("parabolic", "cubic"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    rows = []
    for method in ("parabolic", "cubic"):
        workers = []
        for _ in range(3):
            result = subprocess.run(
                [
                    sys.executable,
                    "-W",
                    "error",
                    str(Path(__file__)),
                    "--worker",
                    method,
                ],
                capture_output=True,
                text=True,
                timeout=30,
                check=True,
            )
            workers.append(json.loads(result.stdout))
        if len({w["profile_digest"] for w in workers}) != 1:
            raise ValueError("fresh worker outputs changed")
        counts = workers[0]["common_cases"]
        row = {
            "method": method,
            "workers": workers,
            "common_cases": counts,
            "p50_plan_us_per_case": float(
                np.median([s["plan_batch_ns"] for w in workers for s in w["samples"]])
            )
            / counts
            / 1000,
            "p50_sample_us_per_case": float(
                np.median([s["sample_batch_ns"] for w in workers for s in w["samples"]])
            )
            / counts
            / 1000,
            "current_exec_peak_rss_bytes": max(
                w["current_exec_peak_rss_bytes"] for w in workers
            ),
        }
        rows.append(row)
        print(json.dumps({k: v for k, v in row.items() if k != "workers"}), flush=True)
    if rows[0]["workers"][0]["source_ids"] != rows[1]["workers"][0]["source_ids"]:
        raise ValueError("cost comparisons used different source cases")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "input_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "methods": rows,
        "scope": "Linux host; 3 fresh workers/method and 7 common-source batches. First lazy SciPy import/profile preparation separate. Plan includes certificate validation; sampling includes owned output. Peak whole current exec includes imports, source JSON and retained profiles; no isolated algorithm/device/frame cost claim.",
    }
    (OUTPUT / "costs.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
