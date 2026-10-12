"""Verify and time complete bark coordinates, then prepare unchanged plant products."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
from bark_checks import check_charts, check_selection
from bark_recipe import bark_recipe
from plant_experiment import sapling


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/bark")
    )
    args = parser.parse_args()
    source = Path("build/experiments/vegetation/collars")
    args.output.mkdir(parents=True, exist_ok=True)
    with np.load(source / "united-wood-mesh.npz") as loaded:
        original = dict(loaded)
    fields = check_selection(original["vertices"])
    checks = check_charts()
    samples = {method: [] for method in ("dense", "indexed")}
    expected = {
        key: hashlib.sha256(value.tobytes()).hexdigest()
        for key, value in fields.items()
    }
    for _ in range(3):
        for method, runs in samples.items():
            sample = json.loads(
                subprocess.check_output(
                    [
                        "/bin/sh",
                        "-c",
                        '"$@"; bark_exit=$?; exit "$bark_exit"',
                        "bark-cost",
                        sys.executable,
                        str(Path(__file__).with_name("bark_cost.py")),
                        "--mesh",
                        str(source / "united-wood-mesh.npz"),
                        "--method",
                        method,
                    ],
                    text=True,
                )
            )
            assert sample["sha256"] == expected
            runs.append(sample)
    np.savez_compressed(args.output / "united-wood-mesh.npz", **original, **fields)
    with np.load(args.output / "united-wood-mesh.npz") as written:
        for key, value in original.items():
            np.testing.assert_array_equal(value, written[key])
    (args.output / "bark-material.json").write_text(
        json.dumps(bark_recipe(), indent=2) + "\n"
    )
    for name in ("united-sapling", "united-attachment"):
        recipe = json.loads((source / f"{name}.json").read_text())
        assert recipe["plan_identity"] == sapling().identity
        recipe["source_modified"] = True
        for product in recipe["products"]:
            if product["surface"] == "wood":
                product["bark_material"] = "bark-material.json"
            for key in ("mesh", "placements"):
                path = args.output / product[key]
                if product["surface"] != "wood" or key == "placements":
                    shutil.copy2(source / product[key], path)
        (args.output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
    implementation = [
        Path(__file__).with_name(name)
        for name in (
            "bark_coordinates.py",
            "bark_checks.py",
            "bark_cost.py",
            "bark_recipe.py",
            "bark_experiment.py",
        )
    ]
    report = {
        "checks": checks,
        "vertices": len(original["vertices"]),
        "axes": sum(sapling().axis_counts),
        "times_s": {
            method: [s["elapsed_s"] for s in runs] for method, runs in samples.items()
        },
        "work": {method: runs[0]["work"] for method, runs in samples.items()},
        "samples": samples,
        "memory_scope": "Linux worker peak RSS including imports/input/scratch/output; forked by small shell after exec to exclude inherited experiment-parent RSS",
        "attribute_bytes": sum(value.nbytes for value in fields.values()),
        "input_sha256": hashlib.sha256(
            (source / "united-wood-mesh.npz").read_bytes()
        ).hexdigest(),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in implementation
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "geometry_unchanged": True,
        "leaves_expanded": 0,
    }
    (args.output / "bark.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
