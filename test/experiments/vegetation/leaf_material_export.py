"""Versioned procedural leaf maps, vein graph, mesh and fixed inspection views."""

import hashlib
import json

import numpy as np
from organ_experiment import write_product
from PIL import Image


def export_leaf(output, name, blade, plan, maps, x):
    paths = {}
    for key, value in maps.items():
        filename = f"{name}-{key}.png"
        # Array row zero represents UV v=0. PNG stores its upper image row first.
        Image.fromarray(
            np.rint(np.clip(value[::-1], 0, 1) * 255).astype(np.uint8)
        ).save(output / filename)
        paths[key] = filename
    np.savez_compressed(
        output / f"{name}-veins.npz",
        positions=plan.positions,
        parents=plan.parents,
        radii=plan.radii,
    )
    material = {
        "version": "procedural-leaf-material-2",
        "maps": paths,
        "height_m": 0.00006,
        "vein_field": "tapered-envelope-1",
        "transmission": 0.22,
        "colour_space": "linear",
        "uv": "u=(s+1)/2,v=t",
        "blade": blade.__dict__,
        "vein_plan": f"{name}-veins.npz",
        "vein_plan_sha256": hashlib.sha256(
            (output / f"{name}-veins.npz").read_bytes()
        ).hexdigest(),
        "remaining_sources": plan.remaining_sources,
        "stop_reason": plan.stop_reason,
    }
    filename = f"{name}-material.json"
    (output / filename).write_text(json.dumps(material, indent=2) + "\n")
    product = write_product(
        output,
        name,
        blade.mesh(64, across=4),
        np.array([[x, 0, 0.10]]),
        np.eye(3)[None],
        "leaf",
    )
    product["leaf_material"] = filename
    for key in maps:
        decoded = np.asarray(Image.open(output / paths[key]))[::-1] / 255
        assert np.max(np.abs(decoded - maps[key])) <= 0.5 / 255 + 1e-15
    return product
