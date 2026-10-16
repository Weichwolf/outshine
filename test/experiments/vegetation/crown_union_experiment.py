"""Finished whole-tree union from saved wood, with all stored organs unchanged."""

import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path

import numpy as np
from crown_union import join_sources, wood_sources
from join_normals import smooth_join_corners
from spatial_crown_cost import restore
from tree_entry import load


def prepared(inputs, plan, representation="cells"):
    comparison = json.loads((inputs / "comparison.json").read_text())
    metadata, products = load(inputs / "spatial")
    crown = restore(plan)
    if metadata["plan_identity"] != crown.identity:
        raise ValueError("canonical saved plan does not own these products")
    with np.load(inputs / "limbs-only.npz") as data:
        body = {key: data[key] for key in data.files}
    paths = json.loads((inputs / "limb-paths.json").read_text())
    sources = wood_sources(
        body,
        paths,
        comparison["along"],
        products["wood.npz"],
        crown.hosts,
        representation,
    )
    if sources is None:
        raise ValueError("complete cached wood exceeds explicit source budget")
    return sources, metadata


def build(inputs, plan, output):
    sources, metadata = prepared(inputs, plan)
    built = join_sources(sources)
    if built is None:
        raise ValueError("complete union exceeds output capacity")
    mesh, work = built
    owner = mesh["source_node"].astype(np.uint64) + (
        mesh["source_kind"].astype(np.uint64) << 32
    )
    lowest = np.full(len(mesh["vertices"]), np.iinfo(np.uint64).max, np.uint64)
    highest = np.zeros(len(mesh["vertices"]), np.uint64)
    np.minimum.at(lowest, mesh["faces"].ravel(), np.repeat(owner, 3))
    np.maximum.at(highest, mesh["faces"].ravel(), np.repeat(owner, 3))
    mesh, work["join_shading"] = smooth_join_corners(mesh, lowest != highest)
    mesh["surface"] = np.zeros(len(mesh["faces"]), np.uint8)
    mesh["corner_uv"] = np.zeros((mesh["faces"].size, 2))
    recipe = {
        "format": "cached-crown-cell-union-1",
        "source": metadata["geometry_identity"],
        "manifold3d": importlib.metadata.version("manifold3d"),
        "join_shading": "angle-45deg-source-contacts",
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    old_faces = len(np.load(inputs / "spatial/wood.npz")["faces"])
    updated = dict(
        metadata,
        geometry_identity=identity,
        instanced_triangles=metadata["instanced_triangles"]
        + len(mesh["faces"])
        - old_faces,
        scope="One connected outer canonical crown/shoot wood body; original leaf products and anchors. Sharp Boolean forks, no natural collar, species/LOD/wind/native admission.",
    )
    output.mkdir(parents=True, exist_ok=False)
    for name, entry in (("recursive", metadata), ("spatial", updated)):
        folder = output / name
        folder.mkdir()
        for source in (inputs / "spatial").iterdir():
            if source.is_file() and source.name != "entry.json":
                (folder / source.name).symlink_to(source.resolve())
        (folder / "entry.json").write_text(json.dumps(entry, indent=2) + "\n")
    (output / "spatial/wood.npz").unlink()
    np.savez_compressed(output / "spatial/wood.npz", **mesh)
    report = {
        "presentation_labels": {
            "recursive": "separate continuous limbs and shoots",
            "spatial": "united complete cached wood",
        },
        "source_identity": metadata["plan_identity"],
        "geometry_identity": identity,
        "recipe": recipe,
        "old_wood_faces": old_faces,
        "new_wood_faces": len(mesh["faces"]),
        "old_whole_triangles": metadata["instanced_triangles"],
        "new_whole_triangles": updated["instanced_triangles"],
        "work": work,
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
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.inputs, args.plan, args.output)
