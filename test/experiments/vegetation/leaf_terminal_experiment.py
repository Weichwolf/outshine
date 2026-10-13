"""Derive cached terminal material profiles while preserving the complete vein graphs."""

import hashlib
import json
import os
import resource
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from blade_surface import Blade
from leaf_closed_material import compose_minor_maps
from leaf_material_maps import material_maps
from leaf_relief_experiment import points_for
from leaf_terminal_profile import terminal_profile
from leaf_texture_filter import filtered_veins
from leaf_vein_field import vein_field
from PIL import Image


def build(source, output, index):
    path = source / f"developed-{index}-material.json"
    recipe = json.loads(path.read_text())
    inputs = [path, source / recipe["vein_plan"], source / recipe["minor_network"]]
    with np.load(inputs[1]) as saved, np.load(inputs[2]) as net:
        plan = SimpleNamespace(**{k: saved[k] for k in saved.files})
        times = []
        for _ in range(3):
            start = time.perf_counter()
            radii, chains = terminal_profile(
                plan.positions,
                plan.parents,
                plan.radii,
                net["positions"],
                net["edges"],
                recipe["minor_radius_m"],
            )
            times.append(time.perf_counter() - start)
        profile = output / f"developed-{index}-profile.npz"
        np.savez_compressed(profile, radii=radii)
        edges = net["raw_edges"][int(net["base_edges"]) :]
        a, b = net["raw_positions"][edges[:, 0]], net["raw_positions"][edges[:, 1]]
    original_radii = plan.radii
    plan.radii = radii
    start = time.perf_counter()
    blade = Blade(**recipe["blade"])
    maps, _ = material_maps(blade, plan)
    maps = {k: np.rint(v * 255) / 255 for k, v in maps.items()}
    height, _ = vein_field(plan, points_for(blade, 512), field="height_m")
    maps["height"] = height.reshape(512, 512) / original_radii.max()
    minor, _ = filtered_veins(
        blade, a, b, np.full(len(a), recipe["minor_radius_m"]), size=512, order=6
    )
    maps = compose_minor_maps(
        maps, minor.reshape(512, 512), recipe["minor_relief_weight"]
    )
    names = {}
    for key, value in maps.items():
        assert np.isfinite(value).all() and value.min() >= 0 and value.max() <= 1
        name = f"developed-{index}-{key}.png"
        Image.fromarray(np.rint(value[::-1] * 255).astype(np.uint8)).save(output / name)
        names[key] = name
    elapsed = time.perf_counter() - start
    recipe.update(
        maps=names,
        material_radius_profile=profile.name,
        material_radius_profile_sha256=hashlib.sha256(profile.read_bytes()).hexdigest(),
        terminal_profile="complete-network-smoothstep-1",
    )
    for key in ("vein_plan", "minor_network"):
        recipe[key] = os.path.relpath(source / recipe[key], output)
    (output / path.name).write_text(json.dumps(recipe, indent=2) + "\n")
    return {
        "profile_seconds": times,
        "material_seconds": elapsed,
        "chains": chains,
        "protected_tips": int(
            np.count_nonzero(
                np.bincount(plan.parents[1:], minlength=len(plan.parents)) == 0
            )
        )
        - len(chains),
        "free_tips": sum(not c["attached"] for c in chains),
        "attached_tips": sum(c["attached"] for c in chains),
        "changed_radii": int(np.count_nonzero(original_radii != radii)),
    }, {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}


def main():
    source = Path("build/experiments/vegetation/relief-leaves")
    output = source.with_name("terminal-leaves")
    output.mkdir(parents=True, exist_ok=True)
    cases, inputs = [], {}
    for index in range(3):
        case, files = build(source, output, index)
        cases.append(case)
        inputs.update(files)
        print(index, {k: v for k, v in case.items() if k != "chains"}, flush=True)
    for name in ("leaf-material-upper.json", "leaf-coupled-detail.json"):
        scene = json.loads((source / name).read_text())
        inputs[str(source / name)] = hashlib.sha256(
            (source / name).read_bytes()
        ).hexdigest()
        for product in scene["products"]:
            for key in ("mesh", "placements"):
                product[key] = os.path.relpath(source / product[key], output)
        (output / name).write_text(json.dumps(scene, indent=2) + "\n")
    report = {
        "cases": cases,
        "input_sha256": inputs,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("leaf_*.py")
        },
    }
    (output / "terminal.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
