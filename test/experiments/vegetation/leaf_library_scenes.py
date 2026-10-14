"""Nine cache-only leaf controls: same row shape/maps, three shader responses."""

import json
import os
import shutil
from pathlib import Path

import numpy as np

MODES = ("reflection-only", "material", "diffuse-sheet")


def material_recipe(source, output, index):
    recipe = json.loads((source / f"developed-{index}-material.json").read_text())
    for key in (
        "vein_plan",
        "minor_network",
        "material_radius_profile",
        "pigment_table",
        "optical_reference",
        "normal_moments",
    ):
        recipe[key] = os.path.relpath(source / recipe[key], output)
    recipe["maps"] = {
        k: os.path.relpath(source / v, output) for k, v in recipe["maps"].items()
    }
    recipe["normal_levels"] = [
        {k: os.path.relpath(source / v, output) for k, v in level.items()}
        for level in recipe["normal_levels"]
    ]
    recipe["normal_level_sha256"] = {
        os.path.relpath(source / name, output): digest
        for name, digest in recipe["normal_level_sha256"].items()
    }
    path = output / f"material-{index}.json"
    path.write_text(json.dumps(recipe, indent=2) + "\n")
    return path.name


def scenes(output):
    source = Path("build/experiments/vegetation/normal-leaves")
    original = json.loads((source / "leaf-material-upper.json").read_text())
    products = []
    for row, old in enumerate(original["products"]):
        recipe = material_recipe(source, output, row)
        for col, mode in enumerate(MODES):
            name = f"leaf-{row}-{col}"
            shutil.copyfile(source / old["mesh"], output / f"{name}-mesh.npz")
            np.savez(
                output / f"{name}-poses.npz",
                origins=np.array([[(col - 1) * 0.14, row * 0.15, 0.1]]),
                frames=np.eye(3)[None],
            )
            products.append(
                old
                | {
                    "mesh": f"{name}-mesh.npz",
                    "placements": f"{name}-poses.npz",
                    "leaf_material": recipe,
                    "leaf_fields": f"material-{row}-fields.npz",
                    "leaf_shader_mode": mode,
                }
            )
    base = original | {
        "products": products,
        "ground_size_m": 0,
        "resolution": [2560, 1440],
        "samples": 64,
        "target": [0, 0.2, 0.1],
        "fov_y": 42,
    }
    views = {
        "leaf-shader-front": {"camera": [0, -0.14, 0.96]},
        "leaf-shader-back": {"camera": [0, -0.14, -0.76]},
        "leaf-shader-detail": {
            "products": products[:3],
            "camera": [0, -0.04, 0.43],
            "target": [0, 0.055, 0.1],
            "fov_y": 46,
        },
    }
    for name, view in views.items():
        (output / f"{name}.json").write_text(
            json.dumps(base | view | {"name": name}, indent=2) + "\n"
        )
    return list(views)
