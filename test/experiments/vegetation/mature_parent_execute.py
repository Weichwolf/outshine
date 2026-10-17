"""Execute bounded wood poses and physical needle ledgers without complete organs."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from mature_density import (
    MAX_AXES,
    MAX_RELATIVE_AREA_ERROR,
    MAX_WINDOWS,
    WINDOW_SETTINGS,
    needle_areas,
)
from mature_parent_graph import mature_graph
from outshine_ref.vegetation import AxisState, NeedledPlant
from outshine_ref.vegetation.needle_windows import capacities
from parent_growth_area_probe import expected_area, measure
from parent_profile_graph import ParentProfileGraph


def provenance():
    names = (
        "mature_parent_execute.py",
        "mature_parent_graph.py",
        "parent_profile_graph.py",
        "parent_growth_graph.py",
        "parent_growth_area_probe.py",
        "mature_density.py",
        "generator_graph.py",
        "generator_specs.py",
        "graph_frames.py",
    )
    here = Path(__file__).parent
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    *[str(here / name) for name in names],
                ],
                text=True,
            ).strip()
        ),
        "implementation_sha256": {
            name: hashlib.sha256((here / name).read_bytes()).hexdigest()
            for name in names
        },
    }


def store_axes(output, poses, needle_counts):
    lengths = np.array([len(pose.path) for pose in poses], dtype=np.uint64)
    assert len({pose.path for pose in poses}) == len(poses)
    np.savez_compressed(
        output / "axes.npz",
        origins=np.array([p.origin for p in poses]),
        rotations=np.array([p.frame for p in poses]),
        uniform_scales=np.array([p.scale for p in poses]),
        lengths=np.array([p.length for p in poses]),
        bends=np.array([p.bend for p in poses]),
        path_offsets=np.r_[np.uint64(0), np.cumsum(lengths)],
        path_pairs=np.array(
            [pair for pose in poses for pair in pose.path], dtype=np.uint32
        ),
        needle_counts=np.asarray(needle_counts, dtype=np.uint32),
        first_needle_ids=np.r_[
            np.uint64(0), np.cumsum(needle_counts, dtype=np.uint64)[:-1]
        ],
    )


def check_replay(descriptor, poses, plant, needle_counts, output):
    replay = ParentProfileGraph.from_recipe(descriptor)
    occupied = np.flatnonzero(needle_counts)
    first = np.r_[np.uint64(0), np.cumsum(needle_counts, dtype=np.uint64)[:-1]]
    near = output / "near"
    near.mkdir(exist_ok=True)
    for index in occupied[[0, len(occupied) // 2, -1]]:
        original, regenerated = poses[index], replay.instance(poses[index].path)
        np.testing.assert_array_equal(original.origin, regenerated.origin)
        np.testing.assert_array_equal(original.frame, regenerated.frame)
        assert original.length == regenerated.length
        order = len(original.path) - 2
        radii = (
            replay.by_id[original.path[-1][0]].shape.radius.evaluate(np.array((0, 1)))
            * original.scale
        )

        def detail(pose):
            axis = AxisState(
                tuple(slot for _, slot in pose.path[2:]),
                int(index),
                pose.length,
                pose.bend,
                pose.origin,
                pose.frame,
                0,
                0,
                path_capacity=256,
            )
            return plant.patterns[order].detail(
                axis, radii, plant.wood.seed, int(needle_counts[index])
            )

        a, b = detail(original), detail(regenerated)
        assert a is not None and b is not None
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
        values = dict(a) | {"ids": a["ids"] + first[index]}
        np.testing.assert_array_equal(
            values["ids"],
            np.arange(
                first[index], first[index] + needle_counts[index], dtype=np.uint64
            ),
        )
        values["variant_ids"] = (values["ids"] + np.uint64(plant.wood.seed)) % len(
            plant.needles
        )
        np.savez_compressed(near / f"axis-{index}.npz", **values)
    assert len(replay._poses) <= 15


def execute(recipe, output, target_area, needle_source):
    began = time.perf_counter()
    plant = NeedledPlant.from_recipe(json.loads(recipe.read_text()))
    graph = mature_graph(plant)
    if not 0 < target_area < float("inf"):
        raise ValueError("finite positive target area required")
    areas, inputs = needle_areas("picea-abies", needle_source)
    proposal = measure(plant, areas, "relative-distance")
    if sum(proposal["axis_counts"]) > MAX_AXES or proposal["windows"] > MAX_WINDOWS:
        raise ValueError("proposed plan exceeds the mature budget before pose work")
    groups = capacities(plant, **WINDOW_SETTINGS)
    poses, counts, per_axis_counts, windows, reports = [], [], [], 0, []
    for order, (pattern, capacity) in enumerate(
        zip(plant.patterns, groups, strict=True)
    ):
        current = graph.instances(order + 1, MAX_AXES)
        if current is None or len(poses) + len(current) > MAX_AXES:
            raise ValueError("executed wood exceeds the existing mature axis budget")
        rotations = np.asarray([pose.frame for pose in current])
        if not np.isfinite(rotations).all():
            raise ValueError("finite executed frames required")
        residual = np.max(
            abs(np.einsum("nji,njk->nik", rotations, rotations) - np.eye(3))
        )
        if residual > 2e-14 or np.max(abs(np.linalg.det(rotations) - 1)) > 2e-14:
            raise ValueError("executed axes require right-handed orthonormal frames")
        local = [pattern.count(pose.length) if pattern else 0 for pose in current]
        counts.append(sum(local))
        per_axis_counts.extend(local)
        if pattern:
            size = capacity * pattern.group_size
            windows += sum((count + size - 1) // size for count in local)
        if windows > MAX_WINDOWS:
            raise ValueError("executed windows exceed the existing mature budget")
        before = len(graph._poses)
        warm = graph.instances(order + 1, 0)
        assert all(a is b for a, b in zip(current, warm, strict=True))
        assert len(graph._poses) == before
        reports.append(
            {
                "order": order,
                "axes": len(current),
                "needles": sum(local),
                "frame_residual_max": float(residual),
                "length_min_median_max_m": np.quantile(
                    [p.length for p in current], (0, 0.5, 1)
                ).tolist(),
            }
        )
        poses.extend(current)
    area = expected_area(counts, plant.patterns, plant.wood.seed, areas)
    area_error = abs(area - target_area) / target_area
    if area_error > MAX_RELATIVE_AREA_ERROR:
        raise ValueError("executed physical area misses the original target tolerance")
    assert [r["axes"] for r in reports] == proposal["axis_counts"]
    output.mkdir(parents=True, exist_ok=True)
    descriptor = graph.recipe()
    (output / "graph.json").write_text(json.dumps(descriptor, indent=2) + "\n")
    store_axes(output, poses, per_axis_counts)
    check_replay(descriptor, poses, plant, per_axis_counts, output)
    result = {
        "graph_identity": graph.identity,
        "source_recipe_sha256": hashlib.sha256(recipe.read_bytes()).hexdigest(),
        "source_recipe": str(recipe),
        "orders": reports,
        "axes": len(poses),
        "windows": windows,
        "needle_counts": counts,
        "expected_area_m2": area,
        "target_area_m2": target_area,
        "relative_area_error": area_error,
        "proposal_area_m2": proposal["expected_area_m2"],
        "proposal_needle_count_delta": sum(counts) - sum(proposal["needle_counts"]),
        "elapsed_s": time.perf_counter() - began,
        **provenance(),
        "physical_area_inputs_sha256": inputs,
        "scope": "executed experimental wood, uniform source scaling, fixed bend shapes and physical needle counts; no complete needle placement/mesh, atlas, botanical, material, LOD or native approval; source attachment jitter is not reproduced",
    }
    (output / "execution.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "axes",
                    "windows",
                    "needle_counts",
                    "expected_area_m2",
                    "proposal_needle_count_delta",
                    "elapsed_s",
                )
            }
        ),
        flush=True,
    )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target-area", type=float, required=True)
    parser.add_argument("--needle-source", type=Path, required=True)
    args = parser.parse_args()
    execute(args.recipe, args.output, args.target_area, args.needle_source)
