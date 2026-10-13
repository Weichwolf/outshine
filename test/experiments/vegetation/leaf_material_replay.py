"""Request coarse geometry from cached leaf recipes; reuse the stored vein/material assets."""

import hashlib
import json
from pathlib import Path

import numpy as np
from blade_surface import Blade
from organ_experiment import write_product
from PIL import Image


def main():
    folder = Path("build/experiments/vegetation/leaf-materials")
    source = folder / "leaf-material-upper.json"
    recipe = json.loads(source.read_text())
    products, checks = [], []
    for product in recipe["products"]:
        path = folder / product["leaf_material"]
        material = json.loads(path.read_text())
        blade = Blade(**material["blade"])
        mesh = blade.mesh(16, across=1)
        with np.load(folder / product["mesh"]) as data:
            near_uv, near_positions = data["parameters"], data["vertices"]
        for uv, position in zip(mesh["parameters"], mesh["vertices"]):
            rows = np.flatnonzero(np.all(near_uv == uv, axis=1))
            assert len(rows) and np.array_equal(near_positions[rows[0]], position)
        for name in material["maps"].values():
            with Image.open(folder / name) as image:
                assert image.size == (512, 512)
                image.verify()
        with np.load(folder / product["placements"]) as poses:
            emitted = write_product(
                folder,
                Path(product["mesh"]).stem + "-coarse",
                mesh,
                poses["origins"],
                poses["frames"],
                "leaf",
            )
        emitted["leaf_material"] = product["leaf_material"]
        products.append(emitted)
        checks.append(
            {
                "common_vertices": len(mesh["vertices"]),
                "same_positions_and_uv": True,
                "material_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        )
    scenario = recipe | {"name": "leaf-material-coarse", "products": products}
    (folder / "leaf-material-coarse.json").write_text(
        json.dumps(scenario, indent=2) + "\n"
    )
    proof = {
        "checks": checks,
        "material_generation_repeated": False,
        "implementation_sha256": {
            Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        },
    }
    (folder / "replay.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof))


if __name__ == "__main__":
    main()
