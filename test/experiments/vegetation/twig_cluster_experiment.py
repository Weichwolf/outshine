"""Whole matched-leaf crown, clustered short shoots versus crowded single shoots."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from crown_pair_export import export_pair
from outshine_ref.vegetation import TwigClusters
from outshine_ref.vegetation.shoot_plan import normal_frame
from posture_replay import read_pose
from spatial_crown_cost import restore
from tree_entry import load
from twig_cluster_mesh import add_twigs


def prepare(source, posture):
    crown = restore(source)
    header, pose = read_pose(posture, crown.identity)
    hosts = crown.hosts
    frame = normal_frame(
        crown.positions[hosts] - crown.positions[crown.parents[hosts]], (0, 0, 1)
    )
    return TwigClusters(
        hosts,
        crown.counts,
        crown.variants,
        pose["positions"][hosts],
        pose["rotations"][hosts] @ frame,
        header["identity"],
        crown.seed,
        carrier_length=crown.length_m,
        carrier_bend=crown.bend_rad,
        petiole=crown.petiole_m,
    )


def save_plan(plan, output):
    fields = ("host_ids", "host_counts", "variants", "origins", "frames")
    np.savez_compressed(
        output / "cluster-plan.npz", **{k: getattr(plan, k) for k in fields}
    )
    (output / "cluster-plan.json").write_text(
        json.dumps(plan.recipe(), indent=2) + "\n"
    )


def read_plan(inputs):
    recipe = json.loads((inputs / "cluster-plan.json").read_text())
    if (
        recipe.pop("format") != "twig-clusters-1"
        or recipe.pop("numpy") != np.__version__
    ):
        raise ValueError("same version clustered-shoot recipe required")
    with np.load(inputs / "cluster-plan.npz", allow_pickle=False) as d:
        return TwigClusters(**{k: d[k] for k in d.files}, **recipe)


def build(source, posture, output):
    start = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    plan = prepare(source, posture)
    metadata, products = load(posture / "spatial")
    loaded = time.perf_counter()
    summary = plan.summary()
    assert not plan._prototypes and not plan._groups
    rows = plan.tables(100000)
    placed = time.perf_counter()
    old_rows = products["tables.npz"]
    for k in ("ids", "variants"):
        np.testing.assert_array_equal(rows[k], old_rows["leaf_" + k])
    wood = add_twigs(plan, products["wood.npz"], 16384)
    if wood is None:
        raise ValueError("complete wood exceeds twig capacity")
    meshed = time.perf_counter()
    tables = {"leaf_" + k: rows[k] for k in ("ids", "variants", "origins", "frames")}
    extra_faces = len(wood["faces"]) - len(products["wood.npz"]["faces"])
    save_plan(plan, output)
    export_pair(
        posture,
        output,
        products["wood.npz"],
        wood,
        (old_rows, tables),
        plan.identity,
        added_wood_faces=extra_faces,
        scope="Same source skeleton/pose and physical leaves; extra curved short shoots, no botanical/welded-skin/LOD/native admission.",
    )
    exported = time.perf_counter()
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "identity": plan.identity,
        "summary": summary,
        "prototypes_generated": len(plan._prototypes),
        "source_triangles": metadata["instanced_triangles"],
        "candidate_triangles": metadata["instanced_triangles"] + extra_faces,
        "extra_wood_faces": extra_faces,
        "stages_s": {
            "source_and_plan": loaded - start,
            "leaf_groups": placed - loaded,
            "extra_wood": meshed - placed,
            "export": exported - meshed,
        },
        "known_array_bytes": {
            "source": sum(a.nbytes for d in products.values() for a in d.values()),
            "candidate_wood": sum(a.nbytes for a in wood.values()),
            "candidate_leaf_rows": sum(a.nbytes for a in rows.values()),
            "prototype_rows": sum(
                a.nbytes for d in plan._prototypes.values() for a in d.values()
            ),
        },
        "presentation_labels": {
            "recursive": "crowded single end shoots",
            "spatial": "partitioned short-shoot groups",
        },
        "scope": "Same saved source nodes/pose/leaf IDs/meshes/materials. Own generic additional architecture, no accepted species/default/world/wind/LOD/device scope.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--posture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.source, args.posture, args.output)
