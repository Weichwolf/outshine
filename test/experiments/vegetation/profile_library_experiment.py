"""Compare installed profiles with a frozen pre-migration source and geometry."""

import hashlib
import importlib.util
import json
import resource
import sys
import time
from pathlib import Path

import numpy as np
from axis_surface import profiled_axis_mesh
from profile_curve import BezierCurve, Profile
from profile_experiment import CURVES


def before_module(path):
    spec = importlib.util.spec_from_file_location("profile_before", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def costs(before, after, queries):
    samples = {"before": [], "after": []}
    for repeat in range(5):
        for name in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            start = time.perf_counter()
            (before if name == "before" else after).evaluate(queries)
            samples[name].append(time.perf_counter() - start)
    return samples


def main():
    root = Path("build/experiments/vegetation/profile-library")
    baseline = root / "profile_curve.before.py"
    previous = before_module(baseline)
    queries = np.random.default_rng(19).uniform(-3, 4, 2048)
    cases = []
    for name, controls in CURVES.items():
        before = previous.Profile.from_curve(previous.BezierCurve(controls))
        after = Profile.from_curve(BezierCurve(controls))
        mesh = profiled_axis_mesh(1.8, 0.45, 0.13, after, segments=128, sides=32)
        errors, exact = {}, []
        with np.load(root / f"{name}-before.npz") as saved:
            for key, actual in {"table": after.table, **mesh}.items():
                if np.array_equal(actual, saved[key]):
                    exact.append(key)
                else:
                    np.testing.assert_allclose(actual, saved[key], atol=1e-10, rtol=0)
                errors[key] = float(np.max(abs(actual - saved[key])))
                if key in ("vertices", "normals"):
                    np.testing.assert_array_equal(
                        actual.astype(np.float32), saved[key].astype(np.float32)
                    )
        for tiled in (False, True):
            for clamp in (False, True):
                np.testing.assert_allclose(
                    after.evaluate(queries, tiled=tiled, clamp=clamp),
                    before.evaluate(queries, tiled=tiled, clamp=clamp),
                    atol=1e-11,
                    rtol=0,
                )
        cases.append(
            {
                "name": name,
                "max_absolute_errors": errors,
                "exact_arrays": exact,
                "lookup_seconds": costs(before, after, queries),
                "queries": len(queries),
            }
        )
    report = {
        "cases": cases,
        "baseline_sha256": hashlib.sha256(baseline.read_bytes()).hexdigest(),
        "float32_meshes_identical": True,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    (root / "migration.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
