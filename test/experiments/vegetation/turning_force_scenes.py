"""Own moving curl, fixed gnarl, planar and recovery products in declared scenarios."""

import hashlib
import json
import shutil
import subprocess
from pathlib import Path

import numpy as np
from growth_force_scenes import guide
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import (
    CurlField,
    DirectionField,
    GnarlField,
    GrowthSpine,
    PlanarField,
    Profile,
    ReturnField,
)
from outshine_ref.vegetation.axis_surface import axis_mesh
from spatial_frond_scenes import pattern

ROOT = Path("build/experiments/vegetation/turning-force")


def programs():
    return {
        "curl": GrowthSpine(
            1.3, (CurlField((1, 0.5, 0.25), Profile(np.full(4, 0.85))),)
        ),
        "gnarl": GrowthSpine(
            1.3, (GnarlField((1, 1, 0.3), Profile(np.full(4, 1.15))),)
        ),
        "planar": GrowthSpine(
            1.3, (PlanarField((1, 0.6, 0.25), Profile(np.full(4, 2))),)
        ),
        "return": GrowthSpine(
            1.3,
            (
                DirectionField((0, 0, 1), Profile((1.2, 0.8, 0, 0))),
                ReturnField(Profile((0, 0, 2, 2))),
            ),
        ),
    }


def products(g, index, near):
    n, a = (48, 4) if near else (12, 1)
    detail = "near" if near else "coarse"
    origin = np.array([[(index - 1.5) * 0.7, 0, 0]])
    leaf = write_product(
        ROOT,
        f"turn-leaf-{index}-{detail}",
        g.deform(pattern(g).mesh(n, a)),
        origin,
        UPRIGHT[None],
        "leaf",
    )
    wood = write_product(
        ROOT,
        f"turn-rib-{index}-{detail}",
        g.deform(
            axis_mesh(
                g.length,
                0,
                0.004,
                0.0007,
                96 if near else 24,
                8 if near else 6,
                bark=True,
            )
        ),
        origin,
        UPRIGHT[None],
        "wood",
    )
    wood["bark_material"] = "bark-material.json"
    return [leaf, wood]


def scenes():
    shutil.copy2(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json",
        ROOT / "bark-material.json",
    )
    plans = []
    curves = []
    for name, p in programs().items():
        result = p.prepare()
        assert result is not None, name
        g = guide(result)
        curves.append(g)
        plans.append(
            {
                "name": name,
                "program": p.recipe(),
                "identity": p.identity,
                "spine": result.spine.controls,
                "guide": g.recipe(),
                "pattern": pattern(g).recipe(),
            }
        )
    (ROOT / "plans.json").write_text(json.dumps(plans, indent=2) + "\n")
    identity = hashlib.sha256(json.dumps(plans, sort_keys=True).encode()).hexdigest()
    near = [p for i, g in enumerate(curves) for p in products(g, i, True)]
    coarse = products(curves[1], 1, False)
    for name, data, camera, target, fov in (
        ("turning-forces", near, (2.4, -5.1, 2.3), (0, -0.12, 0.6), 35),
        ("turning-force-detail", near[2:4], (0.65, -1.9, 1.6), (-0.43, -0.2, 0.67), 32),
        ("turning-force-coarse", coarse, (0.65, -1.9, 1.6), (-0.43, -0.2, 0.67), 32),
    ):
        scene = {
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
        (ROOT / f"{name}.json").write_text(json.dumps(scene, indent=2) + "\n")
