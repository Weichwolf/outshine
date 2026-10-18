"""Scoped equal-coverage shade-template and early-selection construction costs."""

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
from evidence import revision
from terraced_facade_prepare import prepare
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from window_shade_mesh import expand, template
from window_shade_plan import plan_shades


def measure(function, repeats=16):
    function()
    durations = []
    for _ in range(repeats):
        start = time.perf_counter()
        result = function()
        durations.append(time.perf_counter() - start)
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


def costs():
    records = []
    for index, row in enumerate(sources()):
        for kind in ("cascade", "paired-wings"):
            start = time.perf_counter()
            prepared = prepare(plan(row, kind))
            source_setup = time.perf_counter() - start
            for programme in ("eyebrow", "returned-hood"):
                blueprint, planning = measure(partial(plan_shades, prepared, programme))
                alternatives = []
                for method, reuse in (
                    ("profile", True),
                    ("bars", True),
                    ("profile", False),
                ):
                    for selection, ids in (
                        ("all", None),
                        ("three", [s.identifier for s in blueprint.slots[:3]]),
                        ("empty", []),
                    ):
                        run = partial(
                            expand, blueprint, ids, method=method, reuse=reuse
                        )
                        mesh, timing = measure(run)
                        with patch(
                            "window_shade_mesh.template", wraps=template
                        ) as calls:
                            run()
                        alternatives.append(
                            timing
                            | {
                                "method": method,
                                "reuse": reuse,
                                "selection": selection,
                                "triangles": len(mesh["faces"]),
                                "template_builds": calls.call_count,
                                "arrays_B": sum(v.nbytes for v in mesh.values()),
                            }
                        )
                records.append(
                    {
                        "source_index": index,
                        "source_id": row["id"],
                        "kind": kind,
                        "programme": programme,
                        "source_setup_s": source_setup,
                        "accepted": len(blueprint.slots),
                        "rejected": len(blueprint.rejected),
                        "blueprint_json_B": len(json.dumps(asdict(blueprint)).encode()),
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
    if not output.is_relative_to(Path("/tmp")) or output == Path("/tmp"):
        parser.error("--output requires an explicit file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the measured shade implementation first")
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
                "scope": "Source setup, planning and expansion separate; single shared-host worker, no body/renderer/native timing. Tracemalloc misses GEOS-native allocation churn; array bytes are retained payload, not total copied bytes. No cache-hit or device proof.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps({"records": len(records), "output": str(output)}), flush=True)


if __name__ == "__main__":
    main()
