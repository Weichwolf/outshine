"""Reload actual growth plans and material assets without procedural reconstruction."""

import hashlib
import json
import time
from pathlib import Path

import numpy as np
from leaf_network import topology
from PIL import Image


def main():
    folder = Path("build/experiments/vegetation/developing-leaves")
    products = []
    for name in ("frozen", "uniform", "developed-0", "developed-1", "developed-2"):
        start = time.perf_counter()
        material = json.loads((folder / f"{name}-material.json").read_text())
        path = folder / material["vein_plan"]
        assert (
            hashlib.sha256(path.read_bytes()).hexdigest()
            == material["vein_plan_sha256"]
        )
        with np.load(path) as saved:
            parents, positions, radii = (
                saved["parents"],
                saved["positions"],
                saved["radii"],
            )
            assert parents[0] == -1
            assert np.all(
                (parents[1:] >= 0) & (parents[1:] < np.arange(1, len(parents)))
            )
            assert np.isfinite(positions).all() and np.all(radii > 0)
            assert np.all(radii[parents[1:]] >= radii[1:])
            edges = np.column_stack((parents[1:], np.arange(1, len(parents))))
            graph = topology(positions, edges)
            assert graph["components"] == 1 and graph["cycles"] == 0
            count = len(positions)
        for filename in material["maps"].values():
            with Image.open(folder / filename) as image:
                assert image.size == (512, 512)
                image.load()
        products.append(
            {
                "name": name,
                "nodes": count,
                "load_and_check_seconds": time.perf_counter() - start,
            }
        )
    replay = json.loads((folder / "replay.json").read_text())
    assert all(c["same_positions_and_uv"] for c in replay["checks"])
    report = {
        "products": products,
        "coarse_products": len(replay["checks"]),
        "generation_repeated": False,
        "implementation_sha256": {
            Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        },
    }
    (folder / "reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
