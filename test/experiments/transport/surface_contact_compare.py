"""Pinned old/current contact decisions and equal-output normal sampling costs."""

import argparse
import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_terrain_experiment import frozen


def reference(output, commit):
    path = output / "baseline-sampling.py"
    path.write_bytes(
        subprocess.check_output(
            ["git", "show", commit + ":lib/src/outshine_ref/geometry/sampling.py"]
        )
    )
    spec = importlib.util.spec_from_file_location(
        "outshine_ref.geometry._contact_baseline", path
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.SurfaceSampler, digest(path)


def cases():
    row = json.loads(
        Path("lib/tests/data/skinny-shared-ground-contact.json").read_text()
    )
    vertices = np.array([[0, 0, 10.0], [1, 0, 10.0], [6, 1e-9, 10.0], [6, -1e-9, 10.0]])
    regular = Mesh(
        np.array([[0, 0, 10.0], [2, 0, 10.2], [2, 2, 10.6], [0, 2, 10.4]]),
        np.array([[0, 1, 2], [0, 2, 3]]),
    )
    return {
        "normal": (regular, np.random.default_rng(589).uniform(0.1, 1.9, (4096, 2))),
        "actual_shared": (
            Mesh(np.array(row["vertices_enu_m"]), np.array(row["triangles"])),
            np.repeat(row["query_xy_m"], 512, axis=0),
        ),
        "finite_extension": (
            Mesh(vertices, np.array([[0, 1, 2], [0, 1, 3]])),
            np.tile([5.0, 0.0], (512, 1)),
        ),
    }


def measured(sampler, xy):
    began = time.perf_counter()
    result = sampler.sample(xy)
    return {
        "seconds": time.perf_counter() - began,
        "valid": result.report["valid"],
        "candidates": result.report["candidates"],
        "queries": len(xy),
        "result_sha256": hashlib.sha256(
            result.points_enu_m.tobytes()
            + result.normals_enu.tobytes()
            + result.face_indices.tobytes()
        ).hexdigest(),
    }


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("fresh explicit /tmp contact comparison required")
    args.output.mkdir(parents=True, exist_ok=False)
    before, before_sha = reference(args.output, args.baseline_commit)
    inputs = cases()
    samplers = {
        name: {
            "before": before(mesh, xy_tolerance_m=1e-7),
            "current": SurfaceSampler(mesh, xy_tolerance_m=1e-7),
        }
        for name, (mesh, _) in inputs.items()
    }
    controls = {
        name: {
            method: measured(sampler, inputs[name][1])
            for method, sampler in methods.items()
        }
        for name, methods in samplers.items()
    }
    if (
        controls["normal"]["before"]["result_sha256"]
        != controls["normal"]["current"]["result_sha256"]
        or controls["actual_shared"]["before"]["valid"] != 0
        or controls["actual_shared"]["current"]["valid"] != 512
        or controls["finite_extension"]["before"]["valid"] != 512
        or controls["finite_extension"]["current"]["valid"] != 0
    ):
        raise ValueError("pinned controls or equal-quality normal outputs changed")
    runs = {"before": [], "current": []}
    for i in range(5):
        for method in ("before", "current") if i % 2 == 0 else ("current", "before"):
            runs[method].append(
                measured(samplers["normal"][method], inputs["normal"][1])
            )
    work = {}
    for method, cls in (("before", before), ("current", SurfaceSampler)):
        visits = []
        original = cls._features

        def counted(self, xy, pairs, original=original, visits=visits):
            visits.append(pairs.shape[1])
            return original(self, xy, pairs)

        with patch.object(cls, "_features", counted):
            measured(samplers["normal"][method], inputs["normal"][1])
        work[method] = {
            "feature_candidate_rows": sum(visits),
            "feature_batches": len(visits),
        }
    if args.profile:
        import memray

        for method in runs:
            with memray.Tracker(
                str(args.output / (method + ".bin")), trace_python_allocators=True
            ):
                measured(samplers["normal"][method], inputs["normal"][1])
    report = {
        "commit": commit,
        "baseline_commit": args.baseline_commit,
        "baseline_sha256": before_sha,
        "controls": controls,
        "normal_paired_runs": runs,
        "normal_feature_work": work,
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Exact-output normal workload costs; actual shared and finite-extension cases deliberately change correctness. Setup/import outside timers/traces, copied internal bytes unknown; no device/world/optimality claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "normal_feature_work": work}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(parser.parse_args())
