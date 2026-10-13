"""Cache a procedural minor-vein layer over an existing primary leaf material."""

import hashlib
import json
import os

import numpy as np
from leaf_texture_filter import filtered_veins
from PIL import Image


def add_minor_material(output, source_folder, name, blade, recipe, network):
    size, radius, strength = 512, min(blade.width, blade.length) / 800, 0.25
    edges = network.edges[network.base_edges :]
    a, b = network.positions[edges[:, 0]], network.positions[edges[:, 1]]
    radii = np.full(len(edges), radius)
    field, work = filtered_veins(blade, a, b, radii, size=size, order=6)
    field = field.reshape(size, size)
    maps, inputs, decoded = {}, {}, {}
    for key, filename in recipe["maps"].items():
        path = source_folder / filename
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        with Image.open(path) as image:
            decoded[key] = np.asarray(image)[::-1] / 255
    decoded["height"] = np.maximum(decoded["height"], strength * field)
    decoded["colour"] = (
        decoded["colour"] * (1 - 0.08 * field[..., None])
        + np.array([0.13, 0.24, 0.035]) * 0.08 * field[..., None]
    )
    decoded["roughness"] = np.clip(decoded["roughness"] - 0.01 * field, 0, 1)
    for key, value in decoded.items():
        path = output / f"{name}-{key}.png"
        Image.fromarray(np.rint(value[::-1] * 255).astype(np.uint8)).save(path)
        with Image.open(path) as image:
            reloaded = np.asarray(image)[::-1] / 255
        assert np.max(np.abs(reloaded - value)) <= 0.5 / 255 + 1e-15
        maps[key] = path.name
    material = recipe | {
        "maps": maps,
        "vein_plan": os.path.relpath(source_folder / recipe["vein_plan"], output),
        "minor_network": f"{name}-network.npz",
        "minor_network_sha256": hashlib.sha256(
            (output / f"{name}-network.npz").read_bytes()
        ).hexdigest(),
        "minor_radius_m": radius,
        "minor_relief_weight": strength,
        "minor_filter": "uv-box-gauss6-1",
        "minor_remaining_sources": network.remaining_sources,
        "minor_stop_reason": network.stop_reason,
    }
    path = output / f"{name}-material.json"
    path.write_text(json.dumps(material, indent=2) + "\n")
    return path.name, inputs, {"field_pairs": work, "quadrature_order": 6}
