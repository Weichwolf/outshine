"""Compare vein procedures on identical leaves and emit reusable material products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from blade_surface import Blade
from leaf_envelope_checks import check_envelopes
from leaf_material_checks import check_cases, check_field, check_graph
from leaf_material_export import export_leaf
from leaf_material_maps import material_maps
from leaf_venation import grow_veins, leaf_sources


def compare(blade, sources):
    timings = {method: [] for method in ("dense", "tree")}
    plans = {}
    for _ in range(3):
        for method, samples in timings.items():
            began = time.perf_counter()
            plans[method] = grow_veins(blade, sources, method)
            samples.append(time.perf_counter() - began)
    checks = check_graph(blade, plans["dense"], plans["tree"])
    return plans["tree"], {
        "seconds": timings,
        "graph": checks,
        "field": check_field(plans["tree"]),
    }


def compare_maps(blade, plan):
    results, times, work = {}, {"dense": [], "indexed": []}, {}
    for _ in range(3):
        for method, samples in times.items():
            began = time.perf_counter()
            results[method], work[method] = material_maps(blade, plan, 96, method)
            samples.append(time.perf_counter() - began)
    errors = {}
    for key in results["dense"]:
        errors[key] = float(
            np.max(np.abs(results["dense"][key] - results["indexed"][key]))
        )
        assert errors[key] < 1e-12
    return {"size": 96, "seconds": times, "evaluations": work, "maximum_errors": errors}


def scenes(output, products, commit):
    base = {
        "products": products,
        "camera": [0, -0.10, 0.61],
        "target": [0, 0.06, 0.10],
        "fov_y": 35,
        "ground_size_m": 10,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": commit,
        "source_modified": True,
    }
    for name, camera, enabled in (
        ("leaf-material-upper", base["camera"], True),
        ("leaf-material-lower", [0, -0.10, -0.41], True),
        ("leaf-material-before", base["camera"], False),
    ):
        recipe = base | {"name": name, "camera": camera, "procedural_material": enabled}
        if name == "leaf-material-lower":
            recipe["ground_size_m"] = 0
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")


def main():
    output = Path("build/experiments/vegetation/leaf-materials")
    output.mkdir(parents=True, exist_ok=True)
    products, cases = [], []
    shapes = (
        Blade(lobes=0, bend=0.22, twist=0.12),
        Blade(lobes=4, lobe_depth=0.25, width=0.065, bend=0.35, twist=-0.16),
        Blade(lobes=0, widest=0.70, width=0.035, bend=0.12, twist=0.25),
    )
    for index, (blade, seed) in enumerate(zip(shapes, (17, 23, 47))):
        sources = leaf_sources(blade, seed, 700)
        plan, report = compare(blade, sources)
        report["material_comparison"] = compare_maps(blade, plan)
        began = time.perf_counter()
        maps, work = material_maps(blade, plan)
        report |= {
            "seed": seed,
            "blade": blade.__dict__,
            "size": 512,
            "material_seconds": time.perf_counter() - began,
            "material_evaluations": work,
            "material_array_bytes": sum(a.nbytes for a in maps.values()),
        }
        products.append(
            export_leaf(output, f"leaf-{index}", blade, plan, maps, (index - 1) * 0.10)
        )
        report["inputs"] = check_cases(blade)
        cases.append(report)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    scenes(output, products, commit)
    names = (
        "leaf_venation.py",
        "leaf_envelope_checks.py",
        "leaf_vein_field.py",
        "leaf_material_maps.py",
        "leaf_material_checks.py",
        "leaf_material_export.py",
        "leaf_material_experiment.py",
        "blade_surface.py",
    )
    report = {
        "cases": cases,
        "envelope": check_envelopes(),
        "commit": commit,
        "source_modified": True,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
    }
    (output / "leaf-materials.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
