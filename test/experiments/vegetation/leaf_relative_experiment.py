"""Compare exact relative-neighbor searches on the same cached leaf vein graphs."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from blade_surface import Blade
from leaf_relative_checks import check_neighbors
from leaf_relative_neighbors import relative_neighbors
from leaf_venation import leaf_sources


def main():
    folder = Path("build/experiments/vegetation/leaf-materials")
    cases, inputs = [], {}
    for index in range(3):
        recipe_path = folder / f"leaf-{index}-material.json"
        recipe = json.loads(recipe_path.read_text())
        plan_path = folder / recipe["vein_plan"]
        assert (
            hashlib.sha256(plan_path.read_bytes()).hexdigest()
            == recipe["vein_plan_sha256"]
        )
        with np.load(plan_path) as saved:
            nodes = saved["positions"]
        sources = leaf_sources(Blade(**recipe["blade"]), 918 + index, 128)
        times, results, work = {m: [] for m in ("dense", "sectors")}, {}, {}
        for repeat in range(5):
            methods = ("dense", "sectors") if repeat % 2 == 0 else ("sectors", "dense")
            for method in methods:
                start = time.perf_counter()
                results[method], work[method] = relative_neighbors(
                    nodes, sources, method
                )
                times[method].append(time.perf_counter() - start)
            assert all(
                np.array_equal(a, b)
                for a, b in zip(results["dense"], results["sectors"])
            )
        cases.append(
            {
                "nodes": len(nodes),
                "sources": len(sources),
                "seconds": times,
                "work": work,
                "same_neighbors": True,
                "influences": sum(map(len, results["sectors"])),
            }
        )
        for path in (recipe_path, plan_path):
            inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    report = {
        "cases": cases,
        "checks": check_neighbors(),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "input_sha256": inputs,
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "leaf_relative_neighbors.py",
                "leaf_relative_checks.py",
                "leaf_relative_experiment.py",
                "leaf_venation.py",
                "blade_surface.py",
            )
        },
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "closed_vein_growth_implemented": False,
    }
    (folder / "relative-neighbors.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
