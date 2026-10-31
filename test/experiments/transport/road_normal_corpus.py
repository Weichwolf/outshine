"""Screen captured original roads for position-only normal error.

Finite parameter-chart samples locate counterexamples; they do not establish
the continuous normal, physical profile, PBR, World or device contracts.
"""

import argparse
import hashlib
import json
import lzma
import subprocess
import time
from collections import Counter
from pathlib import Path

import numpy as np
from experiment_output import create_output
from road_normal_study import DATA, diagnostic, generate


def read_corpus():
    manifest = json.loads((DATA / "manifest.json").read_text())
    path = DATA / manifest["archive"]
    if (
        path.stat().st_size != manifest["archive_bytes"]
        or hashlib.sha256(path.read_bytes()).hexdigest() != manifest["archive_sha256"]
    ):
        raise ValueError("Exact captured recipe archive required")
    rows, raw_hash, raw_bytes = [], hashlib.sha256(), 0
    with lzma.open(path, "rb") as stream:
        for raw in stream:
            rows.append(json.loads(raw))
            raw_hash.update(raw)
            raw_bytes += len(raw)
    if (
        len(rows) != manifest["records"]
        or raw_bytes != manifest["uncompressed_bytes"]
        or len({r["ordinal"] for r in rows}) != len(rows)
    ):
        raise ValueError("Complete unique captured recipe corpus required")
    return rows, manifest, raw_hash.hexdigest()


def screen(row):
    result = {
        "ordinal": row["ordinal"],
        "form": row["form"],
        "length_m": row["range_m"][1] - row["range_m"][0],
        "source_spans": len(row["intervals"]),
    }
    try:
        plan, curve = generate(row, "position-only", 0.25, np.deg2rad(4), 16384, 65536)
        _, diagnostic_result = diagnostic(plan, curve)
        error = float(
            np.rad2deg(diagnostic_result["observed_maximum_normal_error_rad"])
        )
        lambert = float(diagnostic_result["observed_lambert_maximum_absolute_error"])
        if not np.isfinite([error, lambert]).all():
            raise ValueError("Nonfinite position-only diagnostic")
        result.update(
            accepted=True,
            triangles=len(plan["indices"]),
            sampled_maximum_normal_degrees=error,
            sampled_maximum_lambert_error=lambert,
        )
    except (ValueError, ArithmeticError) as exc:
        result.update(accepted=False, reason=f"{type(exc).__name__}: {exc}")
    return result


def run(args):
    if args.sample_count < 0:
        raise ValueError("Nonnegative sample count required; zero selects whole corpus")
    create_output(args.output)
    rows, manifest, raw_sha = read_corpus()
    if args.sample_count:
        positions = np.unique(
            np.linspace(0, len(rows) - 1, min(args.sample_count, len(rows)), dtype=int)
        )
    else:
        positions = np.arange(len(rows))
    start_cpu, start_wall = time.process_time(), time.perf_counter()
    results = []
    for position in positions:
        results.append(screen(rows[int(position)]))
        if len(results) % 100 == 0:
            print(f"screened {len(results)}/{len(positions)}", flush=True)
    accepted = [r for r in results if r["accepted"]]
    rejected = [r for r in results if not r["accepted"]]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "archive_sha256": manifest["archive_sha256"],
        "selected_raw_jsonl_sha256_observed": raw_sha,
        "selection": "even archive positions"
        if args.sample_count
        else "whole captured corpus",
        "captured_records": len(rows),
        "screened": len(results),
        "accepted": len(accepted),
        "above_four_degrees": sum(
            r["sampled_maximum_normal_degrees"] > 4 for r in accepted
        ),
        "rejection_reasons": dict(Counter(r["reason"] for r in rejected)),
        "cpu_s": time.process_time() - start_cpu,
        "wall_s": time.perf_counter() - start_wall,
        "results": results,
        "scope": __doc__,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "results"}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-count", type=int, default=0)
    run(parser.parse_args())
