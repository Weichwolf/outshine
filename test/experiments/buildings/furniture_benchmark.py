"""Equal-programme wall/jitter and greedy/beam comparisons on actual provider rooms."""

import argparse
import hashlib
import json
import platform
import resource
import time
from pathlib import Path

import numpy as np
from evidence import revision
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan


def signature(product):
    digest = hashlib.sha256()
    for room in product["rooms"]:
        digest.update(room["room_id"].encode())
        digest.update(room["kind"].encode())
        for item in room["result"]["items"]:
            digest.update(item["id"].encode())
            digest.update(item["footprint"].wkb)
            digest.update(item["activity"].wkb)
    return digest.hexdigest()


def compare(plan, repeats):
    rows = []
    required = None
    for method in ("wall-anchors", "jittered-anchors"):
        for width in (1, 4):
            previous = programme(plan, method, width)
            expected = signature(previous)
            requests = [
                (r["room_id"], r["kind"], tuple(str(s) for s in r["required"]))
                for r in previous["rooms"]
            ]
            if required is not None and requests != required:
                raise AssertionError(
                    "Layout alternative changed its actual room programme"
                )
            required = requests
            timings = []
            for _ in range(repeats):
                began = time.perf_counter()
                product = programme(plan, method, width)
                timings.append(1000 * (time.perf_counter() - began))
                if signature(product) != expected:
                    raise AssertionError(
                        "Furnishing layout changed under deterministic replay"
                    )
            rows.append(
                {
                    "method": method,
                    "beam_width": width,
                    "status": product["status"],
                    "complete_rooms": sum(
                        r["result"]["status"] == "complete" for r in product["rooms"]
                    ),
                    "requested_rooms": len(product["rooms"]),
                    "instances": sum(
                        len(r["result"]["items"]) for r in product["rooms"]
                    ),
                    "tested_candidates": sum(
                        r["result"]["tested"] for r in product["rooms"]
                    ),
                    "rejected_candidates": sum(
                        len(r["result"]["rejected"]) for r in product["rooms"]
                    ),
                    "p50_ms": float(np.quantile(timings, 0.5)),
                    "p95_ms": float(np.quantile(timings, 0.95)),
                    "samples_ms": timings,
                    "placement_sha256": expected,
                    "unfilled": [
                        {
                            "room_id": r["room_id"],
                            "kind": r["kind"],
                            "missing": r["result"]["missing"],
                            "reason": r["result"].get("reason"),
                        }
                        for r in product["rooms"]
                        if r["result"]["status"] != "complete"
                    ],
                }
            )
    return rows


def run(output, repeats):
    if not 3 <= repeats <= 21:
        raise ValueError(
            "Furnishing benchmark needs 3..21 measured deterministic repeats"
        )
    output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    cases = []
    for site, identifier in CONTROLS:
        started = time.perf_counter()
        row, plan, receipt = source_plan(site, identifier)
        preparation = time.perf_counter() - started
        result = compare(plan, repeats)
        cases.append(
            {
                "id": identifier,
                "preparation_s": preparation,
                "ground_wkb_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
                "source_properties": row["properties"],
                "receipt": receipt,
                "alternatives": result,
            }
        )
        print(
            site,
            [
                (r["method"], r["beam_width"], r["status"], round(r["p50_ms"], 1))
                for r in result
            ],
            flush=True,
        )
    report = revision() | {
        "status": "pass",
        "cases": cases,
        "repeats": repeats,
        "warmups": 1,
        "elapsed_s": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "rss_scope": "Fresh Linux process maximum including imports/source decode/preparation/all alternatives; not a per-layout allocation peak",
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
        },
        "limits": {
            "requests_per_room": 32,
            "candidates_per_request": 64,
            "beam_width": [1, 4],
        },
        "scope": "Same rooms, requests, portal and activity requirements; CPU layout only. No render time, compiled actor verification, native/device/frame claim or global optimum",
    }
    (output / "benchmark.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/room-programme"),
    )
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    run(args.output, args.repeats)
