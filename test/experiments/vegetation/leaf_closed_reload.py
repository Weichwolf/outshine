"""Fresh-process checks of actual cached graph junctions, maps and LOD coordinates."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from leaf_network import topology
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--folder",
        type=Path,
        default=Path("build/experiments/vegetation/closed-leaves"),
    )
    parser.add_argument("--near", default="closed-leaf-upper.json")
    parser.add_argument("--coarse", default="closed-leaf-coarse.json")
    args = parser.parse_args()
    folder = args.folder
    near = json.loads((folder / args.near).read_text())
    coarse = json.loads((folder / args.coarse).read_text())
    assert len(near["products"]) == len(coarse["products"])
    reports = []
    for first, last in zip(near["products"], coarse["products"]):
        start = time.perf_counter()
        assert first["leaf_material"] == last["leaf_material"]
        path = folder / first["leaf_material"]
        material = json.loads(path.read_text())
        graph_path = folder / material["minor_network"]
        assert (
            hashlib.sha256(graph_path.read_bytes()).hexdigest()
            == material["minor_network_sha256"]
        )
        primary_path = folder / material["vein_plan"]
        assert (
            hashlib.sha256(primary_path.read_bytes()).hexdigest()
            == material["vein_plan_sha256"]
        )
        with np.load(primary_path) as primary, np.load(graph_path) as graph:
            count = len(primary["positions"])
            primary_edges = np.column_stack(
                (primary["parents"][1:], np.arange(1, count))
            )
            assert np.array_equal(graph["raw_positions"][:count], primary["positions"])
            assert np.array_equal(
                graph["raw_edges"][: len(primary_edges)], primary_edges
            )
            assert int(graph["base_edges"]) == len(primary_edges)
            positions, edges, hubs = graph["positions"], graph["edges"], graph["hubs"]
            completed = graph["completed"] >= 0
            assert np.all(hubs[completed] >= 0)
            assert np.array_equal(
                positions[hubs[completed]], graph["sources"][completed]
            )
            summary = topology(positions, edges)
            assert summary["components"] == 1 and summary["cycles"] > 0
            assert (
                int(np.count_nonzero(~completed)) == material["minor_remaining_sources"]
            )
        for filename in material["maps"].values():
            with Image.open(folder / filename) as image:
                assert image.size == (512, 512)
                image.load()
        with np.load(folder / first["mesh"]) as a, np.load(folder / last["mesh"]) as b:
            for uv, point in zip(b["parameters"], b["vertices"]):
                rows = np.flatnonzero(np.all(a["parameters"] == uv, axis=1))
                assert len(rows) and np.array_equal(a["vertices"][rows[0]], point)
            count = len(b["vertices"])
        reports.append(
            summary
            | {
                "common_vertices": count,
                "load_and_check_s": time.perf_counter() - start,
            }
        )
    report = {
        "products": reports,
        "growth_or_material_generation_repeated": False,
        "implementation_sha256": {
            Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        },
    }
    (folder / "reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
