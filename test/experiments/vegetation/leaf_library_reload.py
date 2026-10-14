"""Fresh process loads complete unlit leaf assets without pigment/vein/mesh generation."""

import hashlib
import json
import sys
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import LeafMaterialLevel, LeafOpticsTable


def main():
    folder = Path("build/experiments/vegetation/leaf-library")
    descriptor = json.loads((folder / "materials.json").read_text())
    assert descriptor["version"] == "leaf-material-fields-1"
    assert descriptor["colour_space"] == "linear-srgb"
    assert not descriptor["runtime_illumination_baked"]
    table = LeafOpticsTable.from_recipe(json.loads((folder / "table.json").read_text()))
    assert table.values.shape == (33, 33, 6)
    checks = []
    for product in descriptor["products"]:
        path = folder / product["fields"]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == product["sha256"]
        start = time.perf_counter()
        levels = []
        with np.load(path, allow_pickle=False) as saved:
            assert len(saved.files) == len(product["level_sizes"]) * 3
            for level, size in enumerate(product["level_sizes"]):
                item = LeafMaterialLevel(
                    saved[f"optical_{level}"],
                    saved[f"moments_{level}"],
                    saved[f"coverage_{level}"],
                )
                assert item.coverage.shape == (size, size)
                levels.append(item)
        load_s = time.perf_counter() - start
        start = time.perf_counter()
        base_mean = levels[0].optical_premultiplied.mean((0, 1))
        worst_mean = 0.0
        for item in levels:
            resolved = item.resolve()
            assert (
                np.max(resolved["reflection"] + resolved["transmission"]) <= 1 + 1e-12
            )
            assert np.max(abs(np.linalg.norm(resolved["normal"], axis=2) - 1)) < 1e-14
            assert (
                np.min(resolved["roughness"]) >= 0
                and np.max(resolved["roughness"]) <= 1
            )
            error = float(
                np.max(abs(item.optical_premultiplied.mean((0, 1)) - base_mean))
            )
            worst_mean = max(worst_mean, error)
            assert error < 2e-14
        checks.append(
            {
                "organ": product["organ"],
                "levels": len(levels),
                "load_owned_validate_s": load_s,
                "resolve_and_check_s": time.perf_counter() - start,
                "max_mip_mean_rgb_error": worst_mean,
            }
        )
    assert not any(
        name in sys.modules
        for name in (
            "prosail",
            "colour",
            "scipy",
            "bpy",
            "leaf_venation",
            "leaf_pigment_optics",
        )
    )
    report = {
        "products": checks,
        "generation_repeated": False,
        "external_optics_renderer_imported": False,
    }
    (folder / "material-reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
