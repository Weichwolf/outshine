"""Compare indexed and scanned discovery on the same verified geometric request."""

import argparse
import gc
import hashlib
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from camera_height_inputs import digest
from outshine_ref.transport.frontiers import discover_source_modules
from stair_endpoint_frontier_study import query_context
from stair_frontier_roots import root_seeds
from stair_frontier_sources import records_for
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def meaning(result):
    value = {
        k: v
        for k, v in result.items()
        if k not in ("index_nodes", "index_entries", "proposals")
    }
    value["proposals"] = [
        {k: v for k, v in p.items() if k != "work"} for p in result["proposals"]
    ]
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal priority")
    receipt = json.loads((args.study / "report.json").read_text())
    saved = json.loads((args.study / "selection.json").read_text())
    if (
        not receipt["valid_automatic_module"]
        or receipt["selection_sha256"] != digest(args.study / "selection.json")
        or receipt["query_sha256"] != digest(args.query)
    ):
        raise ValueError(
            "Successful actual automatic study for the same query required"
        )
    state, sources, _, _, manifest = query_context(args)
    records = records_for(state, sources)
    roots, deferred = root_seeds(state, sources, records)
    if (
        roots != saved["index"]["roots"]
        or deferred != saved["index"]["deferred"]
        or manifest["source_ids"] != saved["index"]["before_source_ids"]
    ):
        raise ValueError("Actual original source/profile selection changed")
    expected = {
        name: discover_source_modules(records, roots, method=name)
        for name in ("index", "scan")
    }
    fingerprint = meaning(saved["index"]["plan"])
    if any(meaning(r) != fingerprint for r in expected.values()):
        raise ValueError("Actual parent witnesses or source dispositions changed")
    args.output.mkdir()
    load = os.getloadavg()
    costs = {name: {"rounds": []} for name in expected}
    names = tuple(costs)
    for turn in range(9):
        for name in names[turn % 2 :] + names[: turn % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            result = discover_source_modules(records, roots, method=name)
            costs[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if meaning(result) != fingerprint:
                raise ValueError("Repeated source result changed")
            del result
    for name in names:
        gc.collect()
        result, memory = allocations(
            args.output / (name + ".bin"),
            lambda name=name: discover_source_modules(records, roots, method=name),
        )
        if meaning(result) != fingerprint:
            raise ValueError("Profiled source result changed")
        costs[name].update(memory)
        for clock in ("cpu", "wall"):
            costs[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in costs[name]["rounds"]])
            )
        costs[name]["work"] = [p["work"] for p in result["proposals"]]
        costs[name]["index_nodes"] = result["index_nodes"]
        costs[name]["index_entries"] = result["index_entries"]
    report = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load,
        "load_after": os.getloadavg(),
        "query_sha256": digest(args.query),
        "study_sha256": digest(args.study / "report.json"),
        "result_meaning_sha256": fingerprint,
        "same_original_parent_witnesses_and_dispositions": True,
        "source_count": len(records),
        "input_node_occurrences": expected["index"]["input_node_occurrences"],
        "roots": len(roots),
        "resident_sources": len(manifest["source_ids"]),
        "local_sources": len(state["report"]["candidate_source_ids"]),
        "costs": costs,
        "scope": "Same actual original records, verified old profiles, initial sources and budgets. Discovery scope includes validation/index/heap/output, excludes source/cache IO, eligibility/profile construction, geometry, readers, total copies, process/UMA and native/device/frame time.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({"commit": commit, "same_result": True, "output": str(args.output)}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "query", "study", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())
