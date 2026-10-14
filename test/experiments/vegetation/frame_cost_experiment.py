"""Same canonical plants before/after fixed 3D frame work, with unprofiled CPU costs."""

import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from architecture_experiment import broadleaf
from outshine_ref.vegetation import PlantPlan
from plant_experiment import sapling
from plant_library_experiment import compare_detail

ROOT = Path("build/experiments/vegetation/frame-cost")


def before_module():
    path = ROOT / "before/__init__.py"
    name = "outshine_ref.frame_before"
    spec = importlib.util.spec_from_file_location(
        name, path, submodule_search_locations=[str(path.parent)]
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def case(factory, seed, before):
    template = factory(seed)
    recipe = template.recipe()
    summary = template.summary()
    counts = (summary["axes"], summary["leaves"])
    providers = {"before": before.PlantPlan, "after": PlantPlan}
    warm = {name: provider.from_recipe(recipe) for name, provider in providers.items()}
    expected = warm["before"].expand(*counts)
    actual = warm["after"].expand(*counts)
    compare_detail(expected, actual)
    tables = warm["before"].tables(*counts)
    for key, value in warm["after"].tables(*counts).items():
        np.testing.assert_array_equal(value, tables[key])
    target = tuple(len(range(level.branches)) - 1 for level in template.levels[1:])
    samples = {
        phase: {name: [] for name in providers}
        for phase in ("cold_full", "cold_partial", "warm_partial", "refused")
    }
    for repeat in range(5):
        for name in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            for phase in samples:
                plan = (
                    warm[name]
                    if phase == "warm_partial"
                    else providers[name].from_recipe(recipe)
                )
                began = time.perf_counter()
                if phase == "cold_full":
                    output = plan.expand(*counts)
                elif phase == "refused":
                    output = plan.expand(counts[0] - 1, counts[1])
                else:
                    output = plan.leaves(target, counts[1])
                samples[phase][name].append(time.perf_counter() - began)
                if phase == "cold_full":
                    compare_detail(output, expected)
                elif phase == "refused":
                    assert output is None and not plan._axes and not plan._leaves
                else:
                    saved = warm["before"].leaves(target, counts[1])
                    for key in output:
                        np.testing.assert_array_equal(output[key], saved[key])
    return {
        "name": factory.__name__,
        "seed": seed,
        "identity": template.identity,
        "axes": counts[0],
        "leaves": counts[1],
        "samples_s": samples,
        "all_canonical_tables_exact": True,
        "all_full_and_partial_records_exact": True,
    }


def main():
    before = before_module()
    cases = [
        case(factory, seed, before)
        for factory in (sapling, broadleaf)
        for seed in (17, 0)
    ]
    paths = [
        Path(__file__),
        *[
            Path("lib/src/outshine_ref/vegetation") / f"{name}.py"
            for name in ("vector3", "curve_frame", "shoot_plan")
        ],
    ]
    report = {
        "cases": cases,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "scope": "unprofiled host CPU; recipe construction before timing; same complete canonical output; no meshes/textures/GPU",
        "original_or_device_equivalence_proven": False,
    }
    (ROOT / "frame-cost.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
