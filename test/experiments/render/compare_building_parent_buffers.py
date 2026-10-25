"""Isolate parent-image changes; required original remainders are not image controls."""

import argparse
import json
from pathlib import Path

import numpy as np
from building_mass_materials import policy
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_transfer import transfers
from compare_building_source_models import contact
from compare_mass_appearance import errors, read
from triangle_image_reference import projected_bounds


def compare(scalar, components, output):
    output.mkdir(parents=True, exist_ok=False)
    at = policy()
    roots = {"scalar": scalar, "components": components}
    reports = {k: json.loads((p / "report.json").read_text()) for k, p in roots.items()}
    trees = {k: read_tree(p) for k, p in roots.items()}
    for key in trees["scalar"]:
        if key != "tree_sha256":
            a, b = trees["scalar"][key], trees["components"][key]
            if a.dtype != b.dtype or a.shape != b.shape or a.tobytes() != b.tobytes():
                raise ValueError("identical source hierarchy and frame required")
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    empty_original = {"binding": np.empty((0, 2), np.uint32)}
    panels, comparisons = [], []
    for query in reports["scalar"]["queries"]:
        camera = view(query)
        products = {
            k: load_cut(p, reports[k], trees[k], query, at)[:2]
            for k, p in roots.items()
        }
        a, b = (products[k][0]["needs_sources"] for k in roots)
        if a.dtype != b.dtype or a.tobytes() != b.tobytes():
            raise ValueError("identical required original remainder needed")
        bounds = projected_bounds(
            np.vstack([g["positions"] for g, _ in products.values()]), camera
        )
        box = np.array(
            [
                np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
                np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
            ],
            int,
        )
        area = int(np.prod(box[1] - box[0]))
        rate = 16
        while area * rate**2 > 1048576 and rate > 1:
            rate //= 2
        images, work = {}, {}
        for name, (geometry, material) in products.items():
            images[name], work[name] = transfers(
                geometry,
                empty_original,
                {},
                geometry,
                material,
                camera,
                box,
                tables,
                rate,
            )
        np.savez(
            output / (query["name"] + ".npz"),
            box=box,
            **{k + "__" + f: v for k, img in images.items() for f, v in img.items()},
        )
        comparisons.append(
            {
                "query": query["name"],
                "sample_rate": rate,
                "required_originals_not_rendered": a.tolist(),
                "parent_only_difference": errors(
                    images["components"], images["scalar"]
                ),
                "work": work,
            }
        )
        panels.append(({query["name"] + "-" + k: v for k, v in images.items()}, {}))
    contact(panels, output)
    report = {
        "comparisons": comparisons,
        "scope": "Parent-only finite-quadrature change diagnostic. Required original remainders are verified identical but NOT rendered. No complete/source-error/native/world admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("scalar", "components", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    compare(args.scalar, args.components, args.output)


if __name__ == "__main__":
    main()
