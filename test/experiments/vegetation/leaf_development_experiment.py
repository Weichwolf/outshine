"""Compare growth searches, export mature products, and isolate morphological controls."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from blade_surface import Blade
from leaf_development import develop_veins
from leaf_development_checks import growth_checks
from leaf_domain import contains_segment
from leaf_material_checks import check_graph
from leaf_material_export import export_leaf
from leaf_material_maps import material_maps
from leaf_network import node_network
from leaf_venation import grow_veins, leaf_sources


def compare(blade, candidates):
    times, plans, records = {m: [] for m in ("dense", "tree")}, {}, {}
    for repeat in range(3):
        for method in ("dense", "tree") if repeat % 2 == 0 else ("tree", "dense"):
            start = time.perf_counter()
            plans[method], records[method] = develop_veins(
                blade, candidates, method, shear=2, birth_fraction=0.08
            )
            times[method].append(time.perf_counter() - start)
        check_graph(blade, plans["dense"], plans["tree"])
        assert records["dense"] == records["tree"]
    winner = min(times, key=lambda m: np.median(times[m]))
    plan, record = plans[winner], records[winner]
    edges = np.column_stack((plan.parents[1:], np.arange(1, len(plan.parents))))
    assert all(
        contains_segment(blade, plan.positions[a], plan.positions[b]) for a, b in edges
    )
    _, _, graph = node_network(plan.positions, edges)
    assert graph["components"] == 1 and graph["cycles"] == 0
    assert (
        record["max_growth_chord_error_m"]
        <= min(blade.width, blade.length) / 2000 + 1e-15
    )
    return plan, record | {
        "seconds": times,
        "winner": winner,
        "graph": graph,
        "remaining": plan.remaining_sources,
        "stop_reason": plan.stop_reason,
    }


def emit(folder, name, blade, plan, x):
    start = time.perf_counter()
    maps, work = material_maps(blade, plan)
    product = export_leaf(folder, name, blade, plan, maps, x)
    return product, {
        "material_seconds": time.perf_counter() - start,
        "field_pairs": work,
    }


def main():
    folder = Path("build/experiments/vegetation/developing-leaves")
    folder.mkdir(parents=True, exist_ok=True)
    shapes = (
        Blade(lobes=0, bend=0.22, twist=0.12),
        Blade(lobes=4, lobe_depth=0.25, width=0.065, bend=0.35, twist=-0.16),
        Blade(lobes=0, widest=0.70, width=0.035, bend=0.12, twist=0.25),
    )
    cases, products = [], []
    for index, (blade, seed) in enumerate(zip(shapes, (17, 23, 47))):
        candidates = leaf_sources(blade, seed, 3840).reshape(160, 24, 2)
        plan, report = compare(blade, candidates)
        product, cost = emit(folder, f"developed-{index}", blade, plan, 0.1)
        report |= cost | {
            "seed": seed,
            "blade": blade.__dict__,
            "nodes": len(plan.positions),
            "plan": f"developed-{index}-veins.npz",
        }
        (folder / f"developed-{index}-growth.json").write_text(
            json.dumps(report, indent=2) + "\n"
        )
        cases.append(report)
        if index == 0:
            frozen = grow_veins(blade, leaf_sources(blade, seed, 700))
            uniform, _ = develop_veins(blade, candidates, shear=0, birth_fraction=0.08)
            for name, other, x in (("frozen", frozen, -0.1), ("uniform", uniform, 0)):
                entry, _ = emit(folder, name, blade, other, x)
                products.append(entry)
            products.append(product)
        print(
            index, report["nodes"], report["seconds"], report["remaining"], flush=True
        )
    scene = {
        "products": products,
        "camera": [0, -0.10, 0.61],
        "target": [0, 0.06, 0.10],
        "fov_y": 35,
        "ground_size_m": 10,
        "resolution": [2560, 1440],
        "samples": 32,
        "procedural_material": True,
        "source_modified": True,
        "name": "leaf-development-comparison",
    }
    (folder / "leaf-material-upper.json").write_text(json.dumps(scene, indent=2) + "\n")
    detail = scene | {
        "products": products[-1:],
        "camera": [0.1, 0.024, 0.245],
        "target": [0.1, 0.065, 0.112],
        "name": "leaf-development-detail",
    }
    (folder / "leaf-development-detail.json").write_text(
        json.dumps(detail, indent=2) + "\n"
    )
    names = (
        "leaf_development.py",
        "leaf_growth_map.py",
        "leaf_development_checks.py",
        "leaf_development_experiment.py",
        "leaf_venation.py",
        "leaf_material_maps.py",
        "leaf_material_export.py",
        "leaf_material_checks.py",
        "leaf_vein_field.py",
        "leaf_domain.py",
        "leaf_network.py",
        "blade_surface.py",
    )
    report = {
        "cases": cases,
        "checks": growth_checks(),
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    (folder / "development.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
