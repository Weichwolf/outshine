"""Full cached crown with demanded local geometric collars and unchanged organs."""

import argparse
import hashlib
import json
import shutil
import time
from pathlib import Path

import numpy as np
from collar_union_product import add_collars
from crown_collars import demanded_collars
from tree_entry import load


def build(
    inputs, output, nodes=(13,), cells=48, method="blocked", strength=2.0, samples=9
):
    if output.exists():
        raise FileExistsError(output)
    start = time.perf_counter()
    metadata, products = load(inputs / "spatial")
    collars, plans = demanded_collars(inputs, nodes, cells, method, strength, samples)
    built = time.perf_counter()
    wood, work = add_collars(products["wood.npz"], collars, sorted(nodes))
    wood["surface"] = np.zeros(len(wood["faces"]), np.uint8)
    wood["corner_uv"] = np.zeros((wood["faces"].size, 2))
    recipe = {
        "format": "cached-crown-local-collars-1",
        "source": metadata["geometry_identity"],
        "nodes": sorted(nodes),
        "cells": cells,
        "method": method,
        "strength": strength,
        "samples": samples,
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    old_faces = len(products["wood.npz"]["faces"])
    entry = dict(
        metadata,
        geometry_identity=identity,
        instanced_triangles=metadata["instanced_triangles"]
        + len(wood["faces"])
        - old_faces,
        scope="Same full cached crown/foliage, requested local geometric collars; no universal C1/error/LOD/species/native admission.",
    )
    output.mkdir(parents=True)
    for label, info in (("recursive", metadata), ("spatial", entry)):
        folder = output / label
        folder.mkdir()
        for source in (inputs / "spatial").iterdir():
            if source.is_file() and source.name != "entry.json":
                (folder / source.name).symlink_to(source.resolve())
        (folder / "entry.json").write_text(json.dumps(info, indent=2) + "\n")
    (output / "spatial/wood.npz").unlink()
    np.savez_compressed(output / "spatial/wood.npz", **wood)
    for source in inputs.iterdir():
        if source.is_file() and source.name.startswith(
            ("spatial-plan.", "wood-source-nodes.")
        ):
            shutil.copyfile(source, output / source.name)
    for index, mesh in enumerate(collars):
        np.savez_compressed(output / f"collar-{sorted(nodes)[index]}.npz", **mesh)
    (output / "collar-plans.json").write_text(json.dumps(plans, indent=2) + "\n")
    report = {
        "recipe": recipe,
        "source_identity": metadata["plan_identity"],
        "geometry_identity": identity,
        "old_wood_faces": old_faces,
        "new_wood_faces": len(wood["faces"]),
        "collar_s": built - start,
        "work": work,
        "export_s": time.perf_counter() - start,
        "presentation_labels": {
            "recursive": "cached crown without collars",
            "spatial": "requested geometric collars",
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in report.items() if k != "work"}
            | {
                "work": {
                    k: v for k, v in work.items() if k != "discarded_flat_fragments"
                }
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--nodes", nargs="*", type=int, default=[13])
    parser.add_argument("--cells", type=int, default=48)
    parser.add_argument("--method", choices=("dense", "blocked"), default="blocked")
    parser.add_argument("--strength", type=float, default=2.0)
    parser.add_argument("--samples", type=int, default=9)
    args = parser.parse_args()
    build(
        args.inputs,
        args.output,
        args.nodes,
        args.cells,
        args.method,
        args.strength,
        args.samples,
    )
