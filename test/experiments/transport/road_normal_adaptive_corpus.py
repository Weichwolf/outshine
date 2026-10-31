"""Bound whole-source adaptive road refinement on a captured recipe corpus.

Each accepted plan has the same continuous position/normal limits. Budget or
unsupported sources retain their originals; finite diagnostic samples are not
an independent whole-cell proof, native image, World or device admission.
"""

import argparse
import hashlib
import json
import subprocess
import time
from collections import Counter
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from experiment_output import create_output
from road_normal_corpus import read_corpus
from road_normal_lod import NormalBudget
from road_normal_study import diagnostic, generate


def evaluate(task):
    row, baseline, max_queries, max_vertices, first_valid_split = task
    result = {
        "ordinal": row["ordinal"],
        "form": row["form"],
        "length_m": row["range_m"][1] - row["range_m"][0],
        "baseline_sampled_maximum_degrees": baseline.get(
            "sampled_maximum_normal_degrees"
        ),
    }
    started = time.process_time()
    try:
        plan, curve = generate(
            row,
            "adaptive",
            0.25,
            np.deg2rad(4),
            max_queries,
            max_vertices,
            first_valid_split=first_valid_split,
        )
    except NormalBudget as exc:
        return result | {
            "status": "budget_fallback",
            "reason": str(exc),
            "work": exc.work,
            "cpu_s": time.process_time() - started,
        }
    except (ValueError, ArithmeticError) as exc:
        return result | {
            "status": "unsupported_fallback",
            "reason": f"{type(exc).__name__}: {exc}",
            "cpu_s": time.process_time() - started,
        }
    generation_cpu = time.process_time() - started
    _, observed = diagnostic(plan, curve)
    normal_error = float(np.rad2deg(observed["observed_maximum_normal_error_rad"]))
    position_bound = float(plan["maximum_stored_position_bound_m"])
    finite_arrays = all(
        np.isfinite(value).all()
        for value in plan.values()
        if isinstance(value, np.ndarray)
    )
    valid = bool(
        finite_arrays
        and np.isfinite([normal_error, position_bound]).all()
        and normal_error <= 4
        and position_bound <= 0.25
    )
    return result | {
        "status": "accepted" if valid else "invalid",
        "triangles": len(plan["indices"]),
        "vertices": len(plan["positions_enh_m"]),
        "normal_queries": plan["whole_normal_queries"],
        "normal_requests": plan["whole_normal_requests"],
        "skipped_lateral_requests": plan["skipped_lateral_requests"],
        "sampled_maximum_normal_degrees": normal_error,
        "sampled_maximum_lambert_error": observed[
            "observed_lambert_maximum_absolute_error"
        ],
        "position_bound_m": position_bound,
        "held_plan_array_bytes": sum(
            value.nbytes for value in plan.values() if isinstance(value, np.ndarray)
        ),
        "generation_cpu_s": generation_cpu,
        "total_cpu_s": time.process_time() - started,
    }


def run(args):
    if (
        not 1 <= args.max_queries <= 16384
        or not 1 <= args.workers <= 4
        or args.sample_count < 0
    ):
        raise ValueError("Bounded query, worker and sample limits required")
    files = [
        Path(__file__).with_name(name)
        for name in (
            "road_normal_adaptive_corpus.py",
            "road_normal_corpus.py",
            "road_normal_study.py",
            "road_normal_lod.py",
            "road_normal_cone.py",
            "road_normal_math.py",
            "road_normal_hulls.py",
            "road_surface_model.py",
            "road_surface_lod.py",
            "road_surface_frames.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured producer sources first")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    hashes = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files
    }
    rows, manifest, raw_sha = read_corpus()
    baseline_raw = args.baseline.read_bytes()
    baseline = json.loads(baseline_raw)
    if (
        baseline["selection"] != "whole captured corpus"
        or baseline["archive_sha256"] != manifest["archive_sha256"]
        or baseline["selected_raw_jsonl_sha256_observed"] != raw_sha
    ):
        raise ValueError("Matching complete position-only captured corpus required")
    old = {r["ordinal"]: r for r in baseline["results"]}
    if len(old) != len(rows) or set(old) != {r["ordinal"] for r in rows}:
        raise ValueError("Every original recipe requires its baseline result")
    selected = (
        np.unique(
            np.linspace(0, len(rows) - 1, min(args.sample_count, len(rows)), dtype=int)
        )
        if args.sample_count
        else np.arange(len(rows))
    )
    tasks = [
        (
            rows[int(i)],
            old[rows[int(i)]["ordinal"]],
            args.max_queries,
            65536,
            args.first_valid_split,
        )
        for i in selected
    ]
    create_output(args.output)
    started = time.perf_counter()
    results = []
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        for result in pool.map(evaluate, tasks, chunksize=1):
            results.append(result)
            if len(results) % 50 == 0:
                print(f"evaluated {len(results)}/{len(tasks)}", flush=True)
    if hashes != {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files
    }:
        raise ValueError("Producer sources changed during corpus evaluation")
    results.sort(key=lambda x: x["ordinal"])
    summary = Counter(row["status"] for row in results)
    report = {
        "commit": commit,
        "source_sha256": hashes,
        "archive_sha256": manifest["archive_sha256"],
        "selected_raw_jsonl_sha256_observed": raw_sha,
        "baseline_sha256": hashlib.sha256(baseline_raw).hexdigest(),
        "selection": "even archive positions"
        if args.sample_count
        else "whole captured corpus",
        "workers": args.workers,
        "position_limit_m": 0.25,
        "normal_limit_degrees": 4,
        "first_valid_split": args.first_valid_split,
        "max_queries_per_source": args.max_queries,
        "max_vertices_per_source": 65536,
        "screened": len(results),
        "status_counts": dict(summary),
        "fallback_reasons": dict(
            Counter(r["reason"] for r in results if "reason" in r)
        ),
        "wall_s": time.perf_counter() - started,
        "results": results,
        "scope": __doc__,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in report.items() if k not in ("results", "source_sha256")}
        ),
        flush=True,
    )
    if summary.get("invalid", 0):
        raise ValueError("At least one accepted plan violates finite controls")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-count", type=int, default=0)
    parser.add_argument("--max-queries", type=int, default=512)
    parser.add_argument("--first-valid-split", action="store_true")
    parser.add_argument("--workers", type=int, default=2)
    run(parser.parse_args())
