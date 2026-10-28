"""Same-bound direct closed road displays from versioned pre-contact recipes."""

import argparse
import hashlib
import json
import lzma
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak
from road_closed_checks import observed_body_error
from road_closed_surface import prepare_closed

ARRAYS = (
    "positions_enh_m",
    "indices",
    "face_roles",
    "upper_source_vertex_ids",
    "stations_m",
)


def original_cases():
    started = time.process_time()
    root = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/road_curve_requests"
    )
    manifest = json.loads((root / "manifest.json").read_text())
    archive = root / manifest["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest["archive_sha256"]:
        raise ValueError("Original road recipe archive changed")
    cases, count = [], 0
    with lzma.open(archive, "rt") as stream:
        for line in stream:
            row = json.loads(line)
            count += 1
            if row["form"] == "closed_shell":
                source = {
                    key: row[key]
                    for key in ("form", "range_m", "section_m", "seams", "intervals")
                }
                cases.append(
                    {
                        "ordinal": row["ordinal"],
                        "recipe": source,
                        "recipe_sha256": hashlib.sha256(
                            json.dumps(source, sort_keys=True).encode()
                        ).hexdigest(),
                        "original_native_stations": len(row["stations_m"]),
                    }
                )
    if count != manifest["records"] or len(cases) != 186:
        raise ValueError("Complete original selected corpus required")
    return cases, {
        "manifest": manifest,
        "metadata_extraction_cpu_s": time.process_time() - started,
        "scope": "Fixture acquisition parses the original diagnostic archive, including its Fine fields; only five original metadata fields reach the producer. This is not a selective asset-DB IO proof.",
    }


def stage(cases, method, payload):
    began, wall = time.process_time(), time.perf_counter()
    results = {}
    for case in cases:
        try:
            body = prepare_closed(
                case["recipe"], 0.25, max_payload_bytes=payload, method=method
            )
        except (ValueError, RuntimeError) as error:
            results[case["ordinal"]] = {
                "state": "UnqualifiedOriginalRetained",
                "reason": str(error),
            }
        else:
            results[case["ordinal"]] = {"state": "BoundedDisplayOnly", "body": body}
    return results, {
        "cpu_s": time.process_time() - began,
        "wall_s": time.perf_counter() - wall,
    }


def fingerprint(results):
    value = hashlib.sha256()
    for ordinal, row in sorted(results.items()):
        value.update(str(ordinal).encode())
        value.update(row["state"].encode())
        if "body" in row:
            for key in ARRAYS:
                value.update(row["body"][key].tobytes())
        else:
            value.update(row["reason"].encode())
    return value.hexdigest()


def export(cases, results, output):
    arrays, records = {}, {}
    for method, values in results.items():
        rows = []
        for case in cases:
            ordinal = case["ordinal"]
            value = values[ordinal]
            record = {key: val for key, val in case.items() if key != "recipe"}
            record.update({key: val for key, val in value.items() if key != "body"})
            if "body" in value:
                body = value["body"]
                observed = observed_body_error(case["recipe"], body)
                if observed > body["maximum_stored_bound_m"] + 1e-7:
                    raise ValueError(
                        "Independent closed body observation exceeds bound"
                    )
                record.update(
                    {key: val for key, val in body.items() if key not in ARRAYS}
                )
                record["observed_body_error_m"] = observed
                record["observed_triangle_points"] = 5 * len(body["indices"])
                record["array_prefix"] = f"{method}_{ordinal}_"
                for key in ARRAYS:
                    arrays[record["array_prefix"] + key] = body[key]
            rows.append(record)
        records[method] = rows
    np.savez_compressed(output / "geometry.npz", **arrays)
    return records, sum(value.nbytes for value in arrays.values())


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cases, inputs = original_cases()
    names = ("adaptive", "uniform")
    results = {name: stage(cases, name, args.max_payload_bytes)[0] for name in names}
    expected = {name: fingerprint(values) for name, values in results.items()}
    runs = {name: [] for name in names}
    for turn in range(args.rounds):
        for name in names if turn % 2 == 0 else reversed(names):
            values, cost = stage(cases, name, args.max_payload_bytes)
            if fingerprint(values) != expected[name]:
                raise ValueError("Timed complete corpus output changed")
            runs[name].append(cost)
    traces = {}
    if args.trace:
        import memray

        for name in names:
            path = args.output / (name + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                values, _ = stage(cases, name, args.max_payload_bytes)
            if fingerprint(values) != expected[name]:
                raise ValueError("Profiled complete corpus output changed")
            traces[name] = allocations(path) | {
                "scope": "Warm complete selected-closed-recipe stage including every explicit refusal; original metadata already acquired. Source qualification, normal-budget admission, subdivision, assembly and full stored-body checks included. Output retained as in timed runs. Validation/export/plot/profiler processing excluded. Allocation bytes are not copied bytes."
            }
    records, stored = export(cases, results, args.output)
    peak, peak_source = process_peak()
    report = {
        "schema": "outshine/direct-closed-road-display-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "inputs": inputs,
        "complete_body_error_m": 0.25,
        "per_body_payload_budget_bytes": args.max_payload_bytes,
        "methods": {
            name: {
                "artifact_sha256": expected[name],
                "runs": runs[name],
                "allocations": traces.get(name),
                "cases": records[name],
            }
            for name in names
        },
        "stored_array_payload_bytes": stored,
        "process_peak_rss_bytes_before_plot": peak,
        "process_peak_source": peak_source,
        "scope": "Pre-contact render geometry only; original functional/Fine products retained. Analytic whole-body model bound plus Float32 reservation; observations sample five points per stored triangle. No universal IEEE/libm, final contacts, materials, physical/native/World/device/frame-budget admission. Total copied bytes unmeasured; output payload is not total working memory.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.plot:
        from road_closed_plot import draw

        draw(cases, results, args.output)
    print(
        json.dumps(
            {
                name: {
                    "qualified": sum("body" in row for row in values.values()),
                    "unqualified": sum("body" not in row for row in values.values()),
                }
                for name, values in results.items()
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--max-payload-bytes", type=int, default=8 * 1024 * 1024)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16 or args.max_payload_bytes <= 0:
        parser.error("Bounded rounds1..16 and positive payload bytes required")
    replay(args)


if __name__ == "__main__":
    main()
