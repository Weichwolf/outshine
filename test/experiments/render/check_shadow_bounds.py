"""Check existing native spheres against complete referenced Float32 positions.

Legacy building caches are an additional input, not the accepted-PieceMesh capture
requested under #819. No cache/native resource changes or GPU admission.
"""

import argparse
import collections
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from facing_inputs import buildings
from shadow_bounds import cluster_sphere, outside, transformed_sphere, world_sphere
from shadow_bounds_oracle import ball_violations, containment, shader_points
from shadow_capture import capture


def check_model(model):
    points, triangles = model["vertices"], model["indices"]
    spheres = model["spheres"]
    ranges = [(first * 3, count * 3) for first, count in model["ranges"]]
    start = time.perf_counter()
    bound = cluster_sphere(spheres, ranges, triangles.size)
    build_s = time.perf_counter() - start
    results = []
    for (first, count), sphere in zip(model["ranges"], spheres, strict=True):
        results.append(
            containment(points[triangles[first : first + count]].reshape(-1, 3), sphere)
        )
    referenced = points[triangles].reshape(-1, 3)
    whole = containment(referenced, bound) if bound is not None else None
    return {
        "key": model["key"],
        "group": model["group"],
        "provenance": model["provenance"],
        "vertices": len(points),
        "indices": triangles.size,
        "clusters": len(spheres),
        "bound": bound.tolist() if bound is not None else None,
        "cluster_outside_references": sum(r["outside"] for r in results),
        "cluster_exact_boundary_queries": sum(
            r["exact_boundary_queries"] for r in results
        ),
        "cluster_balls_outside_whole": ball_violations(spheres, bound)
        if bound is not None
        else None,
        "whole_containment": whole,
        "python_bound_s": build_s,
        "logical_cluster_operand_bytes": 40 * len(spheres),
        "aabb_scan_position_operand_bytes": 12 * len(points),
    }


def placements(model):
    """Perturb native points; these rows are controls, not captured instances."""
    results = []
    local = cluster_sphere(
        model["spheres"],
        [(f * 3, n * 3) for f, n in model["ranges"]],
        model["indices"].size,
    )
    if local is None:
        return results
    points = model["vertices"][np.unique(model["indices"])]
    for name, linear, shift in (
        ("shear", [[2, 3, 0], [0, 0.25, 1], [0, 0, -1]], [6.4e6, -3.2e6, 1e6]),
        ("singular", [[0, 0, 0], [0, 1, 0], [0, 0, 0]], [0, 0, 0]),
    ):
        matrix = np.eye(4)
        matrix[:3, :3], matrix[:3, 3] = linear, shift
        bound = transformed_sphere(local, matrix)
        if bound is None:
            raise ValueError("unexpected unknown affine control")
        result = containment(shader_points(points, matrix)[:, :3], bound)
        results.append(dict(name=name, **result))
    return results


def actual_instances(model, bound):
    points = model["vertices"][np.unique(model["indices"])]
    rows = model["instances"]
    aggregate = world_sphere(bound, rows)
    results = []
    for ordinal, matrix in enumerate(rows):
        single = transformed_sphere(bound, matrix)
        if single is None or aggregate is None:
            results.append({"instance": ordinal, "unknown": True})
            continue
        world = shader_points(points, matrix)[:, :3]
        results.append(
            {
                "instance": ordinal,
                "unknown": False,
                "single": containment(world, single),
                "aggregate": containment(world, aggregate),
                "controlled_light_sides": light_sides(points, matrix, single),
            }
        )
    return results


def light_sides(points, matrix, sphere):
    """Actual input points/instances; controlled lights, not captured uniforms."""
    rejected, failures, visible = 0, 0, 0
    light = np.eye(4, dtype=np.float32)
    light[0, 0] = light[1, 1] = 1 / (2 * float(sphere[3]))
    for dx, dy in ((0, 0), (4, 0), (-4, 0), (0, 4), (0, -4)):
        shift = -sphere[:3].astype(float)
        shift[:2] += np.array([dx, dy]) * float(sphere[3])
        clip = shader_points(shader_points(points, matrix, shift)[:, :3], light)
        sides = np.column_stack((clip[:, 3:] + clip[:, :2], clip[:, 3:] - clip[:, :2]))
        reject = outside(sphere, light, shift)
        # Stronger than checking only visible vertices: every rejected piece
        # must put ALL vertices strictly beyond ONE common clipping side.
        common_reject = (sides < 0).all(0).any()
        rejected += int(reject)
        visible += int((sides >= 0).all(1).any())
        failures += int(reject and not common_reject)
    return {
        "queries": 5,
        "rejected": rejected,
        "visible_point_queries": visible,
        "false_rejects": failures,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--buildings", type=Path)
    inputs.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("explicit fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    rows, placed, counts = [], [], collections.Counter()
    groups = set()
    models = buildings(args.buildings) if args.buildings else capture(args.capture)
    for model in models:
        row = check_model(model)
        if "instances" in model:
            row["actual_instances"] = actual_instances(model, row["bound"])
            counts.update(
                instances=len(model["instances"]),
                uncovered_index_references=model["provenance"][
                    "uncovered_index_references"
                ],
                unknown_instances=sum(r["unknown"] for r in row["actual_instances"]),
                instance_outside=sum(
                    r["single"]["outside"] + r["aggregate"]["outside"]
                    for r in row["actual_instances"]
                    if not r["unknown"]
                ),
                controlled_light_false_rejects=sum(
                    r["controlled_light_sides"]["false_rejects"]
                    for r in row["actual_instances"]
                    if not r["unknown"]
                ),
            )
        rows.append(row)
        if model["group"] not in groups:
            cases = placements(model)
            placed.append({"key": model["key"], "cases": cases})
            groups.add(model["group"])
        counts.update(
            pieces=1,
            vertices=row["vertices"],
            indices=row["indices"],
            clusters=row["clusters"],
            invalid_bounds=int(row["bound"] is None),
            cluster_outside=row["cluster_outside_references"],
            whole_outside=row["whole_containment"]["outside"]
            if row["whole_containment"]
            else 0,
        )
    input_path = args.buildings / "manifest.json" if args.buildings else args.capture
    report = {
        "scope": "Native capture or separate legacy building-cache control, as explicitly identified by input. Python source model, no native execution or GPU/PBR/world/device admission. Exact dyadic containment decisions; recorded instances and independent affine controls are separate.",
        "native_reference": "0eb2e739d009cf12df5855f9f688dd0169a6de1c",
        "input": str(input_path.resolve()),
        "input_sha256": hashlib.sha256(input_path.read_bytes()).hexdigest(),
        "counts": counts,
        "models": rows,
        "affine_controls": placed,
        "python_seconds_including_oracles": time.perf_counter() - start,
        "process_peak_rss_linux_kib": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "code_sha256": {
            name: hashlib.sha256(
                (Path(__file__).parent / name).read_bytes()
            ).hexdigest()
            for name in [
                Path(__file__).name,
                "shadow_bounds.py",
                "shadow_bounds_oracle.py",
                "facing_intervals.py",
                "facing_inputs.py",
                "cached_building_streams.py",
                "shadow_capture.py",
            ]
        },
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(counts), flush=True)
    if (
        any(r["cluster_balls_outside_whole"] for r in rows)
        or counts["cluster_outside"]
        or counts["whole_outside"]
        or counts["instance_outside"]
        or counts["controlled_light_false_rejects"]
        or counts["uncovered_index_references"]
        or any(c["outside"] for p in placed for c in p["cases"])
    ):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
