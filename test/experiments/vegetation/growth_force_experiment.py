"""Same-quality force preparation: analytic direction, DOP853 and RK45 plus full replay."""

import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from growth_force_scenes import ROOT, guide, programs, scenes
from outshine_ref.vegetation import GrowthSpine, Profile, SpatialFrondGuide
from spatial_frond_scenes import pattern


def scaled(program, scale):
    return replace(
        program,
        fields=tuple(
            replace(f, rate=Profile(f.rate.table * scale)) for f in program.fields
        ),
    )


def vector_before():
    path = ROOT / "vector-before/__init__.py"
    spec = importlib.util.spec_from_file_location(
        "growth_vector_before", path, submodule_search_locations=[str(path.parent)]
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.GrowthSpine


def callback_costs(before, program, name):
    samples = {"vector": [], "scalar": []}
    plans = {"vector": before.from_recipe(program.recipe()), "scalar": program}
    reference = None
    difference = 0.0
    for repeat in range(5):
        for method in ("vector", "scalar") if repeat % 2 == 0 else ("scalar", "vector"):
            began = time.perf_counter()
            result = plans[method].prepare(method="RK45")
            samples[method].append(time.perf_counter() - began)
            assert result is not None
            controls = np.asarray(result.spine.controls)
            if reference is None:
                reference = controls
            difference = max(difference, float(abs(controls - reference).max()))
            assert difference < 2e-9
    return {"name": name, "samples_s": samples, "max_control_difference": difference}


def compare(program, name, scale):
    methods = ["DOP853", "RK45"]
    if program.constant_vector() is not None:
        methods.append("analytic")
    samples = {m: [] for m in methods}
    evaluations = {}
    expected = None
    max_difference = 0.0
    errors = {}
    for repeat in range(5):
        for method in methods if repeat % 2 == 0 else methods[::-1]:
            began = time.perf_counter()
            result = program.prepare(method=method)
            samples[method].append(time.perf_counter() - began)
            assert result is not None, (name, scale, method)
            evaluations[method] = result.evaluations
            errors[method] = {
                "position_m": result.position_error_m,
                "angle_rad": result.tangent_error_rad,
                "norm": result.tangent_norm_error,
            }
            controls = np.asarray(result.spine.controls)
            if expected is None:
                expected = controls
            difference = float(abs(controls - expected).max())
            max_difference = max(max_difference, difference)
            assert difference < 2e-9, difference
    selected = min(methods, key=lambda m: np.median(samples[m]))
    loaded = GrowthSpine.from_recipe(json.loads(json.dumps(program.recipe())))
    a, b = program.prepare(method=selected), loaded.prepare(method=selected)
    assert a is not None and b is not None and loaded.identity == program.identity
    np.testing.assert_array_equal(a.spine.controls, b.spine.controls)
    return {
        "name": name,
        "scale": scale,
        "identity": program.identity,
        "samples_s": samples,
        "evaluations": evaluations,
        "errors": errors,
        "max_control_difference": max_difference,
        "selected": selected,
        "recipe_curve_exact": True,
    }


def product_replay(methods):
    results = []
    for name, p in programs().items():
        method = "analytic" if name == "neutral" else methods[name]
        r = p.prepare(method=method)
        g = guide(r)
        saved = []
        for n, a in ((48, 4), (12, 1)):
            m = g.deform(pattern(g).mesh(n, a))
            path = ROOT / f"{name}-{n}.npz"
            np.savez_compressed(path, **m)
            saved.append((path, m))
        with patch.object(
            GrowthSpine, "prepare", side_effect=AssertionError("solver replay")
        ):
            for path, expected in saved:
                with np.load(path) as loaded:
                    for key, value in expected.items():
                        np.testing.assert_array_equal(value, loaded[key])
            cached = SpatialFrondGuide.from_recipe(json.loads(json.dumps(g.recipe())))
            np.testing.assert_array_equal(cached.spine.controls, g.spine.controls)
        results.append(
            {
                "name": name,
                "near_faces": len(saved[0][1]["faces"]),
                "coarse_faces": len(saved[1][1]["faces"]),
                "replay_solver_calls": 0,
            }
        )
    return results


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    cases = [
        compare(scaled(p, scale), name, scale)
        for name, p in programs().items()
        if name != "neutral"
        for scale in (0.5, 1, 1.5)
    ]
    methods = {c["name"]: c["selected"] for c in cases if c["scale"] == 1}
    before = vector_before()
    callbacks = [
        callback_costs(before, p, name)
        for name, p in programs().items()
        if name != "neutral"
    ]
    replay = product_replay(methods)
    scenes(methods)
    paths = [Path(__file__), Path(__file__).with_name("growth_force_scenes.py")]
    paths += [
        Path("lib/src/outshine_ref/vegetation") / n
        for n in (
            "force_fields.py",
            "growth_integrate.py",
            "growth_spine.py",
            "scalar_forces.py",
        )
    ]
    report = {
        "cases": cases,
        "callback_cases": callbacks,
        "selected_scene_methods": methods,
        "replay": replay,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "botanical_original_mechanical_or_device_equivalence_proven": False,
    }
    (ROOT / "growth-force-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
