"""Complete own frond products from directed growth, profile recovery and attraction."""

import hashlib
import json
import shutil
import subprocess
from pathlib import Path

import numpy as np
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import (
    AttractionField,
    DirectionField,
    GrowthSpine,
    Profile,
    RadialFalloff,
    SpatialFrondGuide,
)
from outshine_ref.vegetation.axis_surface import axis_mesh
from spatial_frond_scenes import pattern

ROOT = Path("build/experiments/vegetation/growth-force")


def programs():
    return {
        "neutral": GrowthSpine(1.3),
        "direction": GrowthSpine(
            1.3, (DirectionField((0, -0.25, -1), Profile(np.full(4, 0.8))),)
        ),
        "profile": GrowthSpine(
            1.3,
            (
                DirectionField((0, -0.25, -1), Profile((0.8, 0.9, 0.1, -0.7))),
                DirectionField((1, 0.2, 0), Profile((0, 0.15, 0.3, 0.15))),
            ),
        ),
        "attraction": GrowthSpine(
            1.3,
            (
                AttractionField(
                    (0.35, 1.1, -0.30),
                    Profile(np.full(4, 1.2)),
                    falloff=RadialFalloff((0.1, 0.75, -0.1), 0.25, 1.5, "squared"),
                ),
                DirectionField((0, -0.25, -1), Profile(np.full(4, 0.15))),
            ),
        ),
    }


def guide(result):
    return SpatialFrondGuide(
        result.spine,
        width=Profile(np.linspace(1, 0.85, 4)),
        height=Profile(np.linspace(0.9, 1.1, 4)),
        roll=Profile(np.linspace(0, 0.35, 4)),
        fold_per_m=0.18,
    )


def products(g, index, near):
    n, across = (48, 4) if near else (12, 1)
    detail = "near" if near else "coarse"
    origin = np.array([[(index - 1.5) * 0.70, 0, 0]])
    leaf = write_product(
        ROOT,
        f"force-leaf-{index}-{detail}",
        g.deform(pattern(g).mesh(n, across)),
        origin,
        UPRIGHT[None],
        "leaf",
    )
    rib = g.deform(
        axis_mesh(
            g.length, 0, 0.004, 0.0007, 96 if near else 24, 8 if near else 6, bark=True
        )
    )
    wood = write_product(
        ROOT, f"force-rib-{index}-{detail}", rib, origin, UPRIGHT[None], "wood"
    )
    wood["bark_material"] = "bark-material.json"
    return [leaf, wood]


def scenes(methods):
    shutil.copy2(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json",
        ROOT / "bark-material.json",
    )
    curves = []
    plans = []
    for name, p in programs().items():
        method = "analytic" if name == "neutral" else methods[name]
        result = p.prepare(method=method)
        assert result is not None
        g = guide(result)
        curves.append(g)
        plans.append(
            {
                "program": p.recipe(),
                "identity": p.identity,
                "method": method,
                "stations": 33,
                "spine": result.spine.controls,
                "guide": g.recipe(),
                "pattern": pattern(g).recipe(),
            }
        )
    (ROOT / "plans.json").write_text(json.dumps(plans, indent=2) + "\n")
    near = [p for i, g in enumerate(curves) for p in products(g, i, True)]
    coarse = products(curves[3], 3, False)
    for name, data, camera, target, fov in (
        ("growth-forces", near, (2.4, -5.1, 2.3), (0, 0.15, 0.6), 35),
        ("growth-force-detail", near[6:8], (2.1, -1.7, 1.4), (1.18, 0.10, 0.67), 32),
        ("growth-force-coarse", coarse, (2.1, -1.7, 1.4), (1.18, 0.10, 0.67), 32),
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
            "growth_plan_identities": [p["identity"] for p in plans],
            "plan_identity": hashlib.sha256(
                json.dumps(plans, sort_keys=True).encode()
            ).hexdigest(),
        }
        (ROOT / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
