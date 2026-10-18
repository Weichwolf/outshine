"""Reuse the equal-output front/outer benchmark for the new whole-wall profiles."""

import argparse
import json
import resource
import time
from dataclasses import asdict
from functools import partial
from pathlib import Path

from bay_facade_cost import expansion_cost, measure
from bay_facade_mesh import expand
from evidence import revision
from facade_programme_plan import PROGRAMMES, plan_programme
from terraced_crown_plan import plan
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terraces


def costs():
    records = []
    for index, kind in ((0, "cascade"), (3, "paired-wings")):
        row = sources()[index]
        began = time.perf_counter()
        terrace = terraces(plan(row, kind).body)
        preparation = time.perf_counter() - began
        for style in PROGRAMMES:
            planned, planning = measure(
                partial(
                    plan_programme, terrace.prepared, style, portals=terrace.portals
                )
            )
            selections = {}
            chosen = tuple(c.identifier for c in planned.cells[:3])
            for name, ids in (("all", None), ("three", chosen), ("empty", ())):
                direct, d = expansion_cost(planned, ids, "front", "direct")
                filtered, f = expansion_cost(planned, ids, "front", "filter")
                for key in direct:
                    if (
                        direct[key].dtype != filtered[key].dtype
                        or direct[key].shape != filtered[key].shape
                        or direct[key].tobytes() != filtered[key].tobytes()
                    ):
                        raise ValueError(
                            "Programme front alternatives changed actual draw arrays"
                        )
                mesh, owners = expand(planned, ids, mode="front")
                if len(owners) != len(mesh["faces"]) or (
                    len(owners) and owners.max() >= len(planned.cells)
                ):
                    raise ValueError("Programme front triangle ownership changed")
                selections[name] = {
                    "byte_exact_draw_arrays": True,
                    "direct": d,
                    "outer_then_filter": f,
                }
            records.append(
                {
                    "id": row["id"],
                    "programme": style,
                    "mass_kind": kind,
                    "source_preparation_s": preparation,
                    "plan": planning,
                    "cells": len(planned.cells),
                    "descriptor_json_B": len(
                        json.dumps(asdict(planned), default=lambda v: v.hex()).encode()
                    ),
                    "selections": selections,
                    "maximum_recipe_profile_adjustment_m": max(
                        layer.profile_adjustment_m
                        for cell in planned.cells
                        for layer in cell.layers
                    ),
                }
            )
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the programme cost producer first")
    result = proof | {
        "records": costs(),
        "worker_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "New profile planning and equal-output front expansion only,16 timed warm calls with a separate traced call. Source acquisition/full-body CSG/legacy trim construction excluded. Tracemalloc excludes native heap; no full allocation/copy traffic, runtime/cache/device claim. JSON descriptor is diagnostic, not a compact cache schema.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"records": len(result["records"]), "output": str(args.output)}))


if __name__ == "__main__":
    main()
