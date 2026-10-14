"""Generate replayable pigment-driven reflection and transmission material maps."""

import hashlib
import json
import os
import resource
import time
from pathlib import Path

import numpy as np
from blade_surface import Blade
from leaf_pigment_fields import pigment_coordinates
from leaf_pigment_optics import FIXED, gamut_map, lookup
from leaf_relief_experiment import points_for
from outshine_ref.vegetation.leaf_optics import optical_mixture
from PIL import Image

STATES = ((45.0, 0.2), (8.0, 0.2), (10.0, 8.0))


def build(source, output, table, index):
    path = source / f"developed-{index}-material.json"
    recipe = json.loads(path.read_text())
    height_path = source / recipe["maps"]["height"]
    start = time.perf_counter()
    with Image.open(height_path) as image:
        height = np.asarray(image)[::-1] / 255
    points = points_for(Blade(**recipe["blade"]), 512)
    pigments = pigment_coordinates(points, height.ravel(), 121 + index, *STATES[index])
    raw = lookup(table, pigments)
    optical, scale = gamut_map(raw)
    colour, transmitted, weight = optical_mixture(optical)
    maps = {
        "colour": colour.reshape(512, 512, 3),
        "transmittance": transmitted.reshape(512, 512, 3),
        "transmission_weight": weight.reshape(512, 512),
    }
    files, decoded = {}, {}
    for key, values in maps.items():
        assert values.min() >= -1e-14 and values.max() <= 1 + 1e-14
        filename = f"developed-{index}-{key}.png"
        Image.fromarray(
            np.rint(np.clip(values[::-1], 0, 1) * 255).astype(np.uint8)
        ).save(output / filename)
        with Image.open(output / filename) as image:
            decoded[key] = np.asarray(image)[::-1] / 255
        files[key] = filename
    w = decoded["transmission_weight"].ravel()[:, None]
    reconstructed = np.column_stack(
        (
            decoded["colour"].reshape(-1, 3) * (1 - w),
            decoded["transmittance"].reshape(-1, 3) * w,
        )
    )
    quantization_error = float(np.max(abs(reconstructed - optical)))
    assert quantization_error <= 1 / 255 + 1e-14
    map_seconds = time.perf_counter() - start
    diagnostic_start = time.perf_counter()
    target = output / f"developed-{index}-optics.npz"
    np.savez_compressed(target, pigments=pigments, optical=optical)
    diagnostic_seconds = time.perf_counter() - diagnostic_start
    for key in ("height", "roughness"):
        files[key] = os.path.relpath(source / recipe["maps"][key], output)
    for key in ("vein_plan", "minor_network", "material_radius_profile"):
        recipe[key] = os.path.relpath(source / recipe[key], output)
    recipe.update(
        version="procedural-leaf-material-3",
        maps=files,
        symmetric_optics=True,
        optical_model="PROSPECT-D-RGB-log-table-1",
        optical_fixed=FIXED,
        pigment_state_ug_cm2=STATES[index],
        pigment_seed=121 + index,
        pigment_table="pigment-table.npz",
        pigment_table_sha256=hashlib.sha256(
            (output / "pigment-table.npz").read_bytes()
        ).hexdigest(),
        optical_reference=target.name,
        optical_reference_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
    )
    (output / path.name).write_text(json.dumps(recipe, indent=2) + "\n")
    inputs = {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (path, height_path)
    }
    return {
        "state": STATES[index],
        "map_and_validation_seconds": map_seconds,
        "diagnostic_export_seconds": diagnostic_seconds,
        "minimum_gamut_scale": float(scale.min()),
        "gamut_rgb_change": float(np.max(abs(raw - optical))),
        "quantization_max_error": quantization_error,
        "pigment_min": pigments.min(axis=0).tolist(),
        "pigment_max": pigments.max(axis=0).tolist(),
    }, inputs


def main():
    source = Path("build/experiments/vegetation/terminal-leaves")
    output = source.with_name("pigment-leaves")
    with np.load(output / "pigment-table.npz") as saved:
        table = saved["values"]
    cases, inputs = [], {}
    for index in range(3):
        case, files = build(source, output, table, index)
        cases.append(case)
        inputs.update(files)
        print(index, case, flush=True)
    for name in ("leaf-material-upper.json", "leaf-coupled-detail.json"):
        scene = json.loads((source / name).read_text())
        for product in scene["products"]:
            for key in ("mesh", "placements"):
                product[key] = os.path.relpath(source / product[key], output)
        (output / name).write_text(json.dumps(scene, indent=2) + "\n")
    lower = json.loads(
        (source.with_name("leaf-materials") / "leaf-material-lower.json").read_text()
    )
    upper = json.loads((output / "leaf-material-upper.json").read_text())
    lower["products"] = upper["products"]
    (output / "leaf-material-lower.json").write_text(json.dumps(lower, indent=2) + "\n")
    report = {
        "cases": cases,
        "input_sha256": inputs,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "table_sha256": hashlib.sha256(
            (output / "pigment-table.npz").read_bytes()
        ).hexdigest(),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("leaf_pigment*.py")
        },
    }
    (output / "pigments.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
