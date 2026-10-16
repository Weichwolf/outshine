"""Whole matched-foliage comparison: recursive tiers versus existing spatial growth."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from broadleaf_crowns import architecture, match_area
from colonization import GrowthBudget, grow, pipe_radii
from crown_attractors import attractors
from crown_shoots import CrownShoots
from fern_maps import write_maps
from outshine_ref.vegetation.axis_surface import axis_mesh
from tree_entry import wood_mesh
from wood_mesh import branch_tubes


def crown_wood(crown, radius=0.14):
    v, f, n = branch_tubes(
        crown.positions, crown.parents, pipe_radii(crown.parents, radius), 8
    )
    vertices, faces, normals = [v], [f], [n]
    offset = len(v)
    local = axis_mesh(crown.length_m, crown.bend_rad, 0.0005, 0.00012, 3, 4)
    for node in crown.hosts:
        origin, frame = crown.stem(int(node))
        vertices.append(local["vertices"] @ frame.T + origin)
        faces.append(local["faces"] + offset)
        normals.append(local["normals"] @ frame.T)
        offset += len(local["vertices"])
    f = np.concatenate(faces).astype(np.int32)
    return {
        "vertices": np.concatenate(vertices),
        "faces": f,
        "normals": np.concatenate(normals),
        "corner_uv": np.zeros((f.size, 2)),
        "surface": np.zeros(len(f), np.uint8),
    }


def save(folder, rows, wood, blades, identity):
    folder.mkdir()
    np.savez_compressed(folder / "tables.npz", **rows)
    np.savez_compressed(folder / "wood.npz", **wood)
    triangles = len(wood["faces"])
    for index, blade in enumerate(blades):
        mesh = blade.mesh(8, 1)
        mesh["corner_uv"] = np.c_[
            (mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0]
        ][mesh["faces"].ravel()]
        mesh["surface"] = np.full(len(mesh["faces"]), 6, np.uint8)
        np.savez_compressed(folder / f"blade-{index}.npz", **mesh)
        triangles += int((rows["leaf_variants"] == index).sum()) * len(mesh["faces"])
    write_maps(folder)
    metadata = {
        "format": "spatial-crown-products-1",
        "plan_identity": identity,
        "tables": "tables.npz",
        "products": ["wood.npz", *[f"blade-{i}.npz" for i in range(8)]],
        "instanced_triangles": triangles,
        "scope": "Same blade variants/counts and material; changed whole spatial skeleton. Closed individual branch tubes, not welded forks; generic authored crown, not botanical/LOD/native admission.",
    }
    (folder / "entry.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return triangles


def build(output, seed=718, points=2048, distribution="lobed", area=55):
    start = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    base = match_area(architecture("oak", seed, True), area)
    before = base.tables(2000, 100000)
    if before is None:
        raise ValueError("complete source crown exceeds explicit placement capacity")
    baseline_time = time.perf_counter()
    points_xyz = attractors(points, seed, distribution)
    plan = grow(points_xyz, 8.0, 0.23, 0.16, 1.75, 0.22, budget=GrowthBudget(4096, 128))
    crown = CrownShoots(
        plan["positions"],
        plan["parents"],
        before["leaf_variants"],
        seed,
        length_m=0.16,
        foliage_depth_m=0.48,
    )
    assert not crown._groups
    summary = crown.summary()
    skeleton_time = time.perf_counter()
    after = crown.tables(len(crown.variants))
    np.testing.assert_array_equal(after["variants"], before["leaf_variants"])
    rows = {
        "leaf_" + k: after[v]
        for k, v in (
            ("ids", "ids"),
            ("variants", "variants"),
            ("origins", "origins"),
            ("frames", "frames"),
        )
    }
    np.savez_compressed(
        output / "spatial-plan.npz",
        positions=crown.positions,
        parents=crown.parents,
        variants=crown.variants,
        attractors=points_xyz,
        remaining=plan["remaining"],
    )
    (output / "spatial-plan.json").write_text(
        json.dumps(crown.recipe(), indent=2) + "\n"
    )
    (output / "recursive-plan.json").write_text(
        json.dumps(base.recipe(), indent=2) + "\n"
    )
    before_tri = save(
        output / "recursive",
        before,
        wood_mesh(before, {"axis_segments": 6, "axis_sides": 6}),
        base.blades,
        base.identity,
    )
    after_tri = save(
        output / "spatial", rows, crown_wood(crown), base.blades, crown.identity
    )
    report = {
        "seed": seed,
        "distribution": distribution,
        "requested_leaf_area_m2": area,
        "source_plan": base.identity,
        "spatial_plan": crown.identity,
        "leaf_count": len(crown.variants),
        "variant_counts": summary["variant_counts"],
        "analytic_leaf_area_m2": float(base.summary()["leaf_area_m2"]),
        "recursive_axes": len(before["axis_ids"]),
        "spatial_nodes": len(crown.parents),
        "spatial_shoots": len(crown.hosts),
        "recursive_triangles": before_tri,
        "spatial_triangles": after_tri,
        "baseline_plan_s": baseline_time - start,
        "growth_and_summary_s": skeleton_time - baseline_time,
        "total_s": time.perf_counter() - start,
        "growth": {
            k: v
            for k, v in plan.items()
            if k not in ("positions", "parents", "remaining")
        },
        "unreached_attractors": len(plan["remaining"]),
        "scope": "All leaf variants/counts identical, hence exact shared physical leaf area/discretization. Different skeletons/envelope, not same-size allometry or ecological competition. Cold full product build is offline; gross demand needs saved skeleton only.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=718)
    parser.add_argument("--points", type=int, default=2048)
    parser.add_argument(
        "--distribution", choices=("ellipsoid", "lobed"), default="lobed"
    )
    parser.add_argument("--area", type=float, default=55)
    args = parser.parse_args()
    build(args.output, args.seed, args.points, args.distribution, args.area)
