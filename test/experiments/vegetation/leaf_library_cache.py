"""Explicit unlit R/T, coverage and moment products for the native material consumer."""

import hashlib
import json
import os
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import leaf_material_mips


def export_materials(output):
    source = Path("build/experiments/vegetation/normal-leaves")
    products, costs, inputs = [], [], {}
    for index in range(3):
        descriptor = source / f"developed-{index}-material.json"
        recipe = json.loads(descriptor.read_text())
        optics = source / recipe["optical_reference"]
        moments = source / recipe["normal_moments"]
        start = time.perf_counter()
        with np.load(optics) as saved:
            optical = saved["optical"].reshape(512, 512, 6)
        with np.load(moments) as saved:
            base_moments = saved["level_0"].astype(float)
        levels = leaf_material_mips(optical, np.ones((512, 512)), base_moments)
        prepare_s = time.perf_counter() - start
        payload = {}
        for level, product in enumerate(levels):
            payload[f"optical_{level}"] = product.optical_premultiplied
            payload[f"moments_{level}"] = product.moments_premultiplied
            payload[f"coverage_{level}"] = product.coverage
        path = output / f"material-{index}-fields.npz"
        start = time.perf_counter()
        np.savez_compressed(path, **payload)
        save_s = time.perf_counter() - start
        for original in (descriptor, optics, moments):
            inputs[str(original)] = hashlib.sha256(original.read_bytes()).hexdigest()
        products.append(
            {
                "organ": f"developed-{index}",
                "material_recipe": f"material-{index}.json",
                "fields": path.name,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "level_sizes": [len(p.coverage) for p in levels],
                "coverage_source": "full-geometry",
                "uv": recipe["uv"],
                "local_frame": "X across, Y along, Z normal; right-handed metres",
                "optical_model": recipe["optical_model"],
                "optical_fixed": recipe["optical_fixed"],
                "pigment_state_ug_cm2": recipe["pigment_state_ug_cm2"],
                "source_optical_recipe": os.path.relpath(descriptor, output),
            }
        )
        costs.append(
            {
                "prepare_with_decode_s": prepare_s,
                "compressed_save_s": save_s,
                "payload_bytes": sum(a.nbytes for a in payload.values()),
                "file_bytes": path.stat().st_size,
            }
        )
    schema = {
        "version": "leaf-material-fields-1",
        "colour_space": "linear-srgb",
        "scalar": "float64",
        "optical_channels": ["R.r", "R.g", "R.b", "T.r", "T.g", "T.b"],
        "moment_channels": [
            "slope_x",
            "slope_y",
            "slope_x_squared",
            "slope_x_times_y",
            "slope_y_squared",
            "perceptual_roughness_fourth_power",
        ],
        "filter": "joint-coverage-weighted-box-2x2",
        "sampling": "filter premultiplied optical/moments with coverage, then divide by positive coverage; zero coverage resolves to zero optical",
        "symmetric_optics": True,
        "runtime_illumination_baked": False,
        "products": products,
    }
    (output / "materials.json").write_text(json.dumps(schema, indent=2) + "\n")
    return {"products": costs, "input_sha256": inputs}
