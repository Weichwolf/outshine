"""Reload complete near/coarse graph products without rebuilding organs or materials."""

import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from PIL import Image


def main():
    root = Path("build/experiments/vegetation/needle-sectors")
    cases = []
    with (
        patch(
            "needle_surface.Needle.mesh",
            side_effect=AssertionError("unexpected needle build"),
        ),
        patch(
            "generator_graph.GeneratorGraph.instances",
            side_effect=AssertionError("unexpected pose build"),
        ),
        patch(
            "needle_material.write_needle_material",
            side_effect=AssertionError("unexpected material build"),
        ),
    ):
        for index in range(3):
            start = time.perf_counter()
            poses = {}
            bytes_loaded = 0
            counts = {}
            for detail in ("near", "coarse"):
                folder = root / f"case-{index}-{detail}"
                recipe = json.loads((folder / "plan.json").read_text())
                poses[detail] = {}
                for product in recipe["products"]:
                    with np.load(folder / product["mesh"]) as mesh:
                        assert len(mesh["faces"]) == product["triangles_per_instance"]
                        assert (
                            np.isfinite(mesh["vertices"]).all()
                            and np.isfinite(mesh["normals"]).all()
                        )
                        bytes_loaded += sum(mesh[k].nbytes for k in mesh.files)
                    with np.load(folder / product["placements"]) as saved:
                        arrays = {k: saved[k] for k in saved.files}
                        assert len(arrays["origins"]) == product["instances"]
                        poses[detail][tuple(product["generator_ids"])] = arrays
                        bytes_loaded += sum(a.nbytes for a in arrays.values())
                    if "needle_material" in product:
                        material = json.loads(
                            (folder / product["needle_material"]).read_text()
                        )
                        for name in material["maps"].values():
                            with Image.open(folder / name) as image:
                                assert image.size == (64, 256)
                                image.load()
                                bytes_loaded += np.asarray(image).nbytes
                counts[detail] = sum(p["instances"] for p in recipe["products"])
            assert poses["near"].keys() == poses["coarse"].keys()
            for group in poses["near"]:
                for key in poses["near"][group]:
                    assert np.array_equal(
                        poses["near"][group][key], poses["coarse"][group][key]
                    )
            cases.append(
                {
                    "index": index,
                    "instances": counts,
                    "decoded_array_bytes": bytes_loaded,
                    "load_and_check_seconds": time.perf_counter() - start,
                }
            )
    report = {"products": cases, "organs_poses_and_materials_regenerated": False}
    (root / "reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
