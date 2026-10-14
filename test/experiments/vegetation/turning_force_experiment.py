"""Old-curve identity, same-quality new force solvers and complete solver-free products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from growth_force_scenes import guide
from growth_force_scenes import programs as old_programs
from outshine_ref.vegetation import GrowthSpine, SpatialFrondGuide
from spatial_frond_scenes import pattern
from turning_force_scenes import ROOT, programs, scenes


def unchanged():
    recipes = json.loads((ROOT / "before-recipes.json").read_text())
    count = 0
    for name, p in old_programs().items():
        assert json.loads(json.dumps(p.recipe())) == recipes[name]
        for method in ("auto", "RK45", "DOP853"):
            actual = p.prepare(method=method)
            np.testing.assert_array_equal(
                actual.spine.controls, np.load(ROOT / f"before-{name}-{method}.npy")
            )
            count += 1
    return count


def compare(p, name):
    samples = {"RK45": [], "DOP853": []}
    errors = {}
    evaluations = {}
    reference = None
    difference = 0
    for repeat in range(5):
        for method in ("RK45", "DOP853") if repeat % 2 == 0 else ("DOP853", "RK45"):
            began = time.perf_counter()
            result = p.prepare(method=method)
            samples[method].append(time.perf_counter() - began)
            assert result is not None, (name, method)
            controls = np.asarray(result.spine.controls)
            if reference is None:
                reference = controls
            difference = max(difference, float(abs(controls - reference).max()))
            assert difference < 2e-9, difference
            errors[method] = {
                "norm": result.tangent_norm_error,
                "position_m": result.position_error_m,
                "angle_rad": result.tangent_error_rad,
            }
            evaluations[method] = result.evaluations
    loaded = GrowthSpine.from_recipe(json.loads(json.dumps(p.recipe())))
    assert loaded.identity == p.identity
    np.testing.assert_array_equal(
        loaded.prepare().spine.controls, p.prepare().spine.controls
    )
    return {
        "name": name,
        "samples_s": samples,
        "evaluations": evaluations,
        "errors": errors,
        "max_control_difference": difference,
        "recipe_curve_exact": True,
    }


def replay():
    reports = []
    for name, p in programs().items():
        g = guide(p.prepare())
        source = pattern(g)
        products = []
        for n, a in ((48, 4), (12, 1)):
            mesh = g.deform(source.mesh(n, a))
            path = ROOT / f"{name}-{n}.npz"
            np.savez_compressed(path, **mesh)
            products.append((path, mesh))
        with patch.object(
            GrowthSpine, "prepare", side_effect=AssertionError("solver replay")
        ):
            cached = SpatialFrondGuide.from_recipe(json.loads(json.dumps(g.recipe())))
            np.testing.assert_array_equal(cached.spine.controls, g.spine.controls)
            for path, expected in products:
                with np.load(path) as loaded:
                    for k, v in expected.items():
                        np.testing.assert_array_equal(v, loaded[k])
        reports.append(
            {
                "name": name,
                "near_faces": len(products[0][1]["faces"]),
                "coarse_faces": len(products[1][1]["faces"]),
                "solver_replay_calls": 0,
            }
        )
    return reports


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    report = {
        "old_curves_exact": unchanged(),
        "cases": [compare(p, name) for name, p in programs().items()],
        "replay": replay(),
    }
    scenes()
    paths = [Path(__file__), Path(__file__).with_name("turning_force_scenes.py")]
    paths += [
        Path("lib/src/outshine_ref/vegetation") / n
        for n in (
            "turning_fields.py",
            "growth_spine.py",
            "growth_integrate.py",
            "force_fields.py",
            "scalar_forces.py",
        )
    ]
    report |= {
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "original_mechanical_botanical_or_device_equivalence_proven": False,
    }
    (ROOT / "turning-force-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
