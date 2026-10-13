"""Compare radius-dependent relief using unchanged cached vein geometry and materials."""

import hashlib
import json
import os
import resource
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from blade_surface import Blade
from leaf_relief_checks import relief_checks
from leaf_texture_filter import filtered_veins
from leaf_vein_field import vein_field
from PIL import Image


def points_for(blade, size):
    u, v = np.meshgrid(
        2 * (np.arange(size) + 0.5) / size - 1, (np.arange(size) + 0.5) / size
    )
    width, _ = blade.width_profile(v)
    return np.column_stack(((u * width).ravel(), (v * blade.length).ravel()))


def build(source, output, index):
    path = source / f"developed-{index}-material.json"
    recipe = json.loads(path.read_text())
    blade = Blade(**recipe["blade"])
    primary = source / recipe["vein_plan"]
    assert (
        hashlib.sha256(primary.read_bytes()).hexdigest() == recipe["vein_plan_sha256"]
    )
    with np.load(primary) as saved:
        plan = SimpleNamespace(**{k: saved[k] for k in saved.files})
    points = points_for(blade, 128)
    times, results = {m: [] for m in ("dense", "indexed")}, {}
    for repeat in range(3):
        for method in ("dense", "indexed") if repeat % 2 == 0 else ("indexed", "dense"):
            start = time.perf_counter()
            results[method], _ = vein_field(plan, points, method, field="height_m")
            times[method].append(time.perf_counter() - start)
        assert np.array_equal(results["dense"], results["indexed"])
    start = time.perf_counter()
    height, work = vein_field(plan, points_for(blade, 512), field="height_m")
    height = height.reshape(512, 512) / plan.radii.max()
    with np.load(source / recipe["minor_network"]) as net:
        edges = net["raw_edges"][int(net["base_edges"]) :]
        a, b = net["raw_positions"][edges[:, 0]], net["raw_positions"][edges[:, 1]]
        minor, _ = filtered_veins(
            blade, a, b, np.full(len(a), recipe["minor_radius_m"]), size=512, order=6
        )
    height = np.maximum(height, recipe["minor_relief_weight"] * minor.reshape(512, 512))
    assert height.min() >= 0 and height.max() <= 1 + 1e-12
    filename = f"developed-{index}-height.png"
    Image.fromarray(np.rint(height[::-1] * 255).astype(np.uint8)).save(
        output / filename
    )
    maps = {k: os.path.relpath(source / v, output) for k, v in recipe["maps"].items()}
    maps["height"] = filename
    recipe.update(maps=maps, height_field="radius-weighted-envelope-1")
    recipe["relief_radius_scale"] = float(plan.radii.max())
    for key in ("vein_plan", "minor_network"):
        recipe[key] = os.path.relpath(source / recipe[key], output)
    (output / path.name).write_text(json.dumps(recipe, indent=2) + "\n")
    return {
        "seconds": times,
        "probe_resolution": [128, 128],
        "same_field": True,
        "final_resolution": [512, 512],
        "final_maps_seconds": time.perf_counter() - start,
        "indexed_pairs": work,
    }


def main():
    source = Path("build/experiments/vegetation/coupled-leaves")
    output = source.with_name("relief-leaves")
    output.mkdir(parents=True, exist_ok=True)
    checks = relief_checks()
    cases = []
    inputs = {}
    for path in source.glob("*.json"):
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        recipe = json.loads(path.read_text())
        if "maps" in recipe:
            names = list(recipe["maps"].values()) + [
                recipe["vein_plan"],
                recipe["minor_network"],
            ]
            for name in names:
                asset = source / name
                inputs[str(asset)] = hashlib.sha256(asset.read_bytes()).hexdigest()
    for index in range(3):
        cases.append(build(source, output, index))
        print(index, cases[-1], flush=True)
    for name in ("leaf-material-upper.json", "leaf-coupled-detail.json"):
        scene = json.loads((source / name).read_text())
        for product in scene["products"]:
            for key in ("mesh", "placements"):
                product[key] = os.path.relpath(source / product[key], output)
        (output / name).write_text(json.dumps(scene, indent=2) + "\n")
    report = {
        "input_sha256": inputs,
        "cases": cases,
        "checks": checks,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("leaf_*.py")
        },
    }
    (output / "relief.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
