"""Actual source bays, unchanged defaults and equal-solid assembly alternatives."""

import argparse
import json
import os
import platform
from pathlib import Path

import numpy as np
from evidence import revision
from facade_recess_comparison import equivalent, storage
from facade_recess_geometry import build
from facade_recess_models import export
from facade_recess_plan import RecessRecipe
from replay_facade_margins import measure
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


def replay(output, rounds):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the experiment before qualifying evidence")
    plans = tuple(plan(row, kind) for row, kind in cases())
    groups, costs = [], {}
    for name, recipe in (("baseline", None), ("recess", RecessRecipe())):
        call = lambda recipe=recipe: tuple(
            RoofedProducts(p, terrace_routing="triangulated", facade_recesses=recipe)
            for p in plans
        )
        call()
        owners, costs[name] = measure(call, rounds, output / f"{name}-owners.bin")
        if name == "baseline":
            groups = [{"baseline": owner} for owner in owners]
        else:
            for group, owner in zip(groups, owners, strict=True):
                group[name] = owner
    descriptors = tuple(group["recess"].body.plan.prepared.recesses for group in groups)
    alternatives, assembly_costs = {}, {}
    for method in ("parts", "union"):
        call = lambda method=method: tuple(build(c, method=method) for c in descriptors)
        call()
        values, cost = measure(call, rounds, output / f"{method}-assembly.bin")
        alternatives[method] = values
        assembly_costs[method] = cost | {"outputs": [storage(v.mesh) for v in values]}
    equality = [
        equivalent(a, b)
        for a, b in zip(alternatives["parts"], alternatives["union"], strict=True)
    ]
    arrays, sources = {}, []
    for i, group in enumerate(groups):
        for name, owner in group.items():
            for detail in (False, True):
                for field, array in owner.draw(detail).items():
                    arrays[f"{i}_{name}_{'near' if detail else 'compact'}_{field}"] = (
                        array
                    )
        owner = group["recess"]
        sources.append(
            {
                "id": owner.plan.body.identifier,
                "cells": len(descriptors[i]),
                "work": dict(owner.recess_work),
                "rejections": owner.body.plan.prepared.recess_rejections,
                "compact": storage(owner.draw(False)),
                "near": storage(owner.draw(True)),
            }
        )
    np.savez_compressed(output / "geometry.npz", **arrays)
    export(groups, output, proof, inside=True)
    report = proof | {
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "sources": sources,
        "compact_owner_costs": costs,
        "equal_solid_assembly_costs": assembly_costs,
        "solid_comparisons": equality,
        "saved_fields": len(arrays),
        "total_internal_copied_bytes": None,
        "scope": "Three actual source buildings; optional source-contained bays. Compact owner timings include planning and assembly, exclude physical expansion/export and independent checks. Assembly controls have equal closed-solid unions and exact open rear frames. Scoped Memray allocations separate from CPU samples. NumPy storage excludes Python/GEOS/Manifold ownership. No native/World/controller/LOD/radiance/device acceptance.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"commit": proof["commit"], "sources": len(sources), "fields": len(arrays)}
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=5)
    args = parser.parse_args()
    output = args.output.resolve()
    if (
        output == Path("/tmp")
        or not output.is_relative_to("/tmp")
        or not 1 <= args.rounds <= 11
    ):
        parser.error("A fresh explicit /tmp subdirectory and1..11 rounds are required")
    output.mkdir(parents=True, exist_ok=False)
    replay(output, args.rounds)


if __name__ == "__main__":
    main()
