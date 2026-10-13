"""Complete frond-function products and a shared-guide rosette, without species claims."""

import hashlib
import json
import subprocess
from dataclasses import asdict

import numpy as np
from axis_surface import axis_mesh
from blade_surface import Blade
from frond_checks import guide_cases
from frond_pattern import PinnatePattern
from organ_experiment import UPRIGHT, write_product
from scipy.spatial.transform import Rotation


def make_scenes(output):
    guides = guide_cases()
    pattern = PinnatePattern()
    pinnate = pattern.mesh(48, 4)
    ribbon_plan = Blade(
        length=1.3,
        width=0.44,
        widest=0.42,
        lobes=0,
        base_fill=0.04,
        bend=0,
        fold=0,
        twist=0,
        petiole=0,
    )
    ribbon = ribbon_plan.mesh(128, 8)
    products = []
    for index, guide in enumerate(guides):
        origin = np.array([[(index - 1.5) * 0.75, 0, 0]])
        source = ribbon if index < 2 else pinnate
        products.append(
            write_product(
                output,
                f"frond-{index}",
                guide.deform(source),
                origin,
                UPRIGHT[None],
                "leaf",
            )
        )
        rib = axis_mesh(guide.length, 0, 0.004, 0.0007, 96, 8)
        products.append(
            write_product(
                output, f"rib-{index}", guide.deform(rib), origin, UPRIGHT[None], "wood"
            )
        )
    function_plan = {
        "version": "frond-functions-1",
        "pattern": pattern.recipe(),
        "ribbon": asdict(ribbon_plan),
        "guides": [g.recipe() for g in guides],
        "spacing_m": 0.75,
        "root_frame": UPRIGHT.tolist(),
    }
    scenes = [
        ("frond-functions", products, (2.5, -4.8, 2.3), (0, 0, 0.64), 38, function_plan)
    ]
    scenes.append(
        (
            "frond-detail",
            products[4:6],
            (0.88, -1.10, 1.10),
            (0.395, 0.18, 0.78),
            38,
            function_plan,
        )
    )
    azimuth = np.arange(12) * 2 * np.pi / 12
    tilt = 0.30 + 0.60 * ((np.arange(12) * 5) % 12) / 11
    frames = (
        Rotation.from_rotvec(
            np.column_stack((azimuth * 0, azimuth * 0, azimuth))
        ).as_matrix()
        @ Rotation.from_rotvec(np.column_stack((-tilt, tilt * 0, tilt * 0))).as_matrix()
        @ UPRIGHT
    )
    origins = np.zeros((12, 3))
    guide = guides[2]
    rosette = [
        write_product(
            output, "rosette-leaf", guide.deform(pinnate), origins, frames, "leaf"
        ),
        write_product(
            output,
            "rosette-rib",
            guide.deform(axis_mesh(guide.length, 0, 0.004, 0.0007, 96, 8)),
            origins,
            frames,
            "wood",
        ),
    ]
    rosette_plan = {
        "version": "frond-rosette-1",
        "pattern": pattern.recipe(),
        "guide": guide.recipe(),
        "azimuth_rad": azimuth.tolist(),
        "tilt_rad": tilt.tolist(),
        "root_frame": UPRIGHT.tolist(),
    }
    scenes.append(
        ("frond-rosette", rosette, (2.6, -3.6, 2.0), (0, 0, 0.52), 38, rosette_plan)
    )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, products, camera, target, fov, plan in scenes:
        recipe = {
            "name": name,
            "products": products,
            "camera": camera,
            "target": target,
            "fov_y": fov,
            "ground_size_m": 10,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": True,
            "plan": plan,
            "plan_identity": hashlib.sha256(
                json.dumps(plan, sort_keys=True).encode()
            ).hexdigest(),
        }
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
