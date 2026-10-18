"""Equal-output direct-front comparison, early cell selection and scoped memory."""

import argparse
import json
import resource
import time
import tracemalloc
from dataclasses import asdict
from functools import partial
from pathlib import Path
from unittest.mock import patch

import numpy as np
from bay_facade_mesh import expand
from bay_facade_plan import PROGRAMMES, plan_bays
from clad_facade_mesh import surface
from evidence import revision
from terraced_crown_plan import plan
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terraces


def front_from_outer(layer, mode):
    if mode != "front":
        raise ValueError("The filter comparison requires the front product")
    full = surface(layer, "outer")
    keep = np.all(full["vertices"][full["faces"], 2] == layer.depth_m, axis=1)
    rows = full["faces"][keep].ravel()
    return {
        key: np.arange(len(rows), dtype=np.uint32).reshape(-1, 3)
        if key == "faces"
        else value[keep]
        if key == "materials"
        else value[rows]
        for key, value in full.items()
    }


def measure(function, repeats=16):
    function()
    durations = []
    for _ in range(repeats):
        began = time.perf_counter()
        result = function()
        durations.append(time.perf_counter() - began)
        del result
    tracemalloc.start()
    result = function()
    retained, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return result, {
        "calls": repeats,
        "median_s": float(np.median(durations)),
        "p95_s": float(np.percentile(durations, 95)),
        "traced_retained_B": retained,
        "traced_peak_B": peak,
    }


def expansion_cost(planned, ids, mode, method):
    builder = surface if method == "direct" else front_from_outer
    calls, source_triangles = 0, 0

    def counted(layer, selected_mode):
        nonlocal calls, source_triangles
        calls += 1
        source_triangles += layer.triangles(
            "outer" if method == "filter" else selected_mode
        )
        return builder(layer, selected_mode)

    with patch("bay_facade_mesh.surface", new=builder):
        (mesh, owners), timing = measure(partial(expand, planned, ids, mode=mode))
    with patch("bay_facade_mesh.surface", new=counted):
        expand(planned, ids, mode=mode)
    return mesh, timing | {
        "mode": mode,
        "method": method,
        "template_builds": calls,
        "template_triangles": source_triangles,
        "output_triangles": len(mesh["faces"]),
        "retained_arrays_B": sum(v.nbytes for v in mesh.values()) + owners.nbytes,
    }


def costs():
    records = []
    for index, kind in ((0, "cascade"), (3, "paired-wings")):
        row = sources()[index]
        start = time.perf_counter()
        terrace = terraces(plan(row, kind).body)
        source_setup = time.perf_counter() - start
        for programme in PROGRAMMES:
            prepared, planning = measure(
                partial(plan_bays, terrace.prepared, programme, portals=terrace.portals)
            )
            alternatives = []
            for selection, ids in (
                ("all", None),
                ("three", [c.identifier for c in prepared.cells[:3]]),
                ("empty", []),
            ):
                direct, result = expansion_cost(prepared, ids, "front", "direct")
                alternatives.append(result | {"selection": selection})
                filtered, result = expansion_cost(prepared, ids, "front", "filter")
                for key in direct:
                    if (
                        direct[key].dtype != filtered[key].dtype
                        or direct[key].shape != filtered[key].shape
                        or direct[key].tobytes() != filtered[key].tobytes()
                    ):
                        raise ValueError(
                            "Direct and filtered fronts differ in their complete numeric output"
                        )
                alternatives.append(
                    result
                    | {"selection": selection, "complete_output_byte_exact": True}
                )
                for mode in ("outer", "closed"):
                    _mesh, result = expansion_cost(prepared, ids, mode, "direct")
                    alternatives.append(result | {"selection": selection})
            records.append(
                {
                    "source_id": row["id"],
                    "programme": programme,
                    "source_setup_s": source_setup,
                    "cells": len(prepared.cells),
                    "rejected": len(prepared.rejected),
                    "diagnostic_plan_json_B": len(
                        json.dumps(asdict(prepared), default=lambda v: v.hex()).encode()
                    ),
                    "planning": planning,
                    "alternatives": alternatives,
                }
            )
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the measured facade implementation first")
    records = costs()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            proof
            | {
                "records": records,
                "worker_peak_rss_KiB": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Planning and expansion separate; original body/renderer excluded. Shared-host 16-call reference timings, no native/device/cache-hit proof. Tracemalloc misses native heaps and total churn; array payload is not total copied bytes. Diagnostic plans repeat protected regions, not a compact runtime-cache format.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps({"records": len(records), "output": str(output)}), flush=True)


if __name__ == "__main__":
    main()
