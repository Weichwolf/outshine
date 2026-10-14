"""Own spatial frond controls, complete procedural products and fixed render scenarios."""

import hashlib
import json
import shutil
import subprocess
from dataclasses import replace
from pathlib import Path

import numpy as np
from leaflet_shape_experiment import patterns
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import CubicSpine, Profile, SpatialFrondGuide
from outshine_ref.vegetation.axis_surface import axis_mesh

ROOT = Path("build/experiments/vegetation/spatial-frond")


def guides():
    return tuple(
        SpatialFrondGuide(
            CubicSpine((((0, 0, 0), (0, 0.42, 0), middle, tip),)),
            width=Profile(np.linspace(1, 0.85, 4)),
            height=Profile(np.linspace(0.9, 1.1, 4)),
            roll=Profile(np.linspace(0, roll, 4)),
            fold_per_m=0.18,
        )
        for middle, tip, roll in (
            ((0, 0.96, -0.09), (0, 1.3, -0.22), 0.15),
            ((0.09, 0.9, 0.08), (0.18, 1.29, -0.06), 0.60),
            ((0.18, 0.92, 0.06), (-0.05, 1.32, 0.12), -0.45),
        )
    )


def pattern(guide):
    return replace(patterns()[2], length=guide.length, leaflet_length=0.24)


def products(guide, index, near):
    plan = pattern(guide)
    n, across = (48, 4) if near else (12, 1)
    detail = "near" if near else "coarse"
    origin = np.array([[(index - 1) * 0.75, 0, 0]])
    leaf = write_product(
        ROOT,
        f"spatial-leaf-{index}-{detail}",
        guide.deform(plan.mesh(n, across)),
        origin,
        UPRIGHT[None],
        "leaf",
    )
    rib = guide.deform(
        axis_mesh(
            guide.length,
            0,
            0.004,
            0.0007,
            96 if near else 24,
            8 if near else 6,
            bark=True,
        )
    )
    wood = write_product(
        ROOT, f"spatial-rib-{index}-{detail}", rib, origin, UPRIGHT[None], "wood"
    )
    wood["bark_material"] = "bark-material.json"
    return [leaf, wood]


def scenes():
    shutil.copy2(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json",
        ROOT / "bark-material.json",
    )
    curves = guides()
    near = [p for i, g in enumerate(curves) for p in products(g, i, True)]
    coarse = products(curves[2], 2, False)
    recipes = [{"guide": g.recipe(), "pattern": pattern(g).recipe()} for g in curves]
    identity = hashlib.sha256(json.dumps(recipes, sort_keys=True).encode()).hexdigest()
    (ROOT / "plans.json").write_text(json.dumps(recipes, indent=2) + "\n")
    for name, data, camera, target, fov in (
        ("spatial-fronds", near, (2.1, -4.2, 2.2), (0, 0, 0.6), 35),
        ("spatial-frond-detail", near[4:6], (1.8, -1.7, 1.5), (0.77, 0.08, 0.67), 32),
        ("spatial-frond-coarse", coarse, (1.8, -1.7, 1.5), (0.77, 0.08, 0.67), 32),
    ):
        recipe = {
            "name": name,
            "products": data,
            "camera": camera,
            "target": target,
            "fov_y": fov,
            "ground_size_m": 8,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_modified": True,
            "plan_identity": identity,
        }
        (ROOT / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
