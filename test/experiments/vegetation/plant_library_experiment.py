"""Complete and requested-subtree plant migration with original-input comparisons."""

import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from plant_experiment import check_plant, sapling

ROOT = Path("build/experiments/vegetation/plant-library")


def before_module():
    path = ROOT / "before/__init__.py"
    spec = importlib.util.spec_from_file_location(
        "plant_before", path, submodule_search_locations=[str(path.parent)]
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def compare_detail(left, right):
    assert len(left["axes"]) == len(right["axes"])
    for a, b in zip(left["axes"], right["axes"], strict=True):
        for name in vars(a):
            np.testing.assert_array_equal(getattr(a, name), getattr(b, name))
    assert len(left["groups"]) == len(right["groups"])
    for a, b in zip(left["groups"], right["groups"], strict=True):
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])


def compare_case(seed, before):
    template = sapling(seed)
    recipe = template.recipe()
    providers = {"before": before.PlantPlan, "after": PlantPlan}
    plants = {
        name: provider.from_recipe(recipe) for name, provider in providers.items()
    }
    summary = template.summary()
    counts = (summary["axes"], summary["leaves"])
    expected = plants["before"].expand(*counts)
    actual = plants["after"].expand(*counts)
    compare_detail(expected, actual)
    for p in plants.values():
        assert p.summary() == summary
        assert p.recipe() == recipe and p.identity == template.identity
    samples = {
        phase: {name: [] for name in providers}
        for phase in ("cold_full", "cold_partial", "warm_partial", "refused")
    }
    target = (3, 2)
    for repeat in range(5):
        for name in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            for phase in samples:
                p = (
                    plants[name]
                    if phase == "warm_partial"
                    else providers[name].from_recipe(recipe)
                )
                began = time.perf_counter()
                if phase == "cold_full":
                    output = p.expand(*counts)
                elif phase == "refused":
                    output = p.expand(counts[0] - 1, counts[1])
                else:
                    output = p.leaves(target, counts[1])
                samples[phase][name].append(time.perf_counter() - began)
                if phase == "cold_full":
                    compare_detail(output, expected)
                elif phase == "refused":
                    assert output is None and not p._axes and not p._leaves
                else:
                    for key in output:
                        np.testing.assert_array_equal(
                            output[key], plants["before"].leaves(target, counts[1])[key]
                        )
    for state in actual["axes"]:
        base_radius, tip_radius = template.radii(state.path)
        for n, sides in ((24, 8), (12, 6)):
            old = before.axis_mesh(
                state.length, state.bend, base_radius, tip_radius, n, sides
            )
            new = axis_mesh(state.length, state.bend, base_radius, tip_radius, n, sides)
            for key in old:
                np.testing.assert_array_equal(old[key], new[key])
    return {
        "seed": seed,
        "identity": template.identity,
        "axes": counts[0],
        "leaves": counts[1],
        "samples_s": samples,
        "all_original_arrays_exact": True,
        "original_meshes_exact": 128,
        "checks": check_plant(sapling(seed)),
    }


def main():
    before = before_module()
    cases = [compare_case(seed, before) for seed in (17, 0, 2**64 - 1)]
    paths = [
        Path(__file__),
        *Path("lib/src/outshine_ref/vegetation").glob("plant*.py"),
        Path("lib/src/outshine_ref/vegetation/shoot_plan.py"),
        Path("lib/src/outshine_ref/vegetation/axis_surface.py"),
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
    }
    (ROOT / "plant-library.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
