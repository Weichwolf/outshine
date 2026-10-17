"""Compare bounded local welds on saved mature axes, without whole-tree union."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
from collar_union_product import add_collars
from crown_collar_accuracy import ray_accuracy
from crown_collars import collar_plan
from crown_union import join_sources, ring_cells
from limb_paths import dominant_paths
from oak_scaffold import tapered_radii
from oak_skin_experiment import source_crown
from spatial_crown_replay import check_mesh


def saved_limbs(wood, paths):
    result, vertex, face = {}, 0, 0
    for path in paths:
        stations = 2 * (len(path) - 1) + 1
        nv, nf = 8 * stations + 2, 16 * stations
        selected = slice(face, face + nf)
        np.testing.assert_array_equal(wood["limb_ids"][selected], path[1])
        result[path[1]] = {
            "vertices": wood["vertices"][vertex : vertex + nv],
            "faces": wood["faces"][selected] - vertex,
            "normals": wood["normals"][3 * face : 3 * (face + nf)],
            "source_kind": 0,
            "source_node": path[1],
            "cell": -1,
            "forbidden_caps": (),
        }
        vertex += nv
        face += nf
    return result


def local_sources(limbs, paths, node, representation):
    parent = next(p for p in paths if node in p[1:])
    selected = [parent, *(p for p in paths if p[0] == node)]
    sources = []
    for index, path in enumerate(selected):
        mesh = limbs[path[1]]
        if representation == "limbs":
            sources.append(
                mesh
                | {
                    "forbidden_caps": tuple(
                        range(len(mesh["faces"]) - 16, len(mesh["faces"]) - 8)
                    )
                    if index
                    else ()
                }
            )
        else:
            # A local parent's end caps remain legitimate outside this patch.
            cells = list(
                ring_cells(mesh, 2 * (len(path) - 1) + 1, path[1], root=index == 0)
            )
            sources.extend(cells)
    return sources, [p[1] for p in selected]


def study(inputs, output, node, cells):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    metadata = json.loads((inputs / "comparison.json").read_text())
    crown, hints = source_crown(inputs, metadata)
    radii = tapered_radii(
        crown.parents,
        metadata["recipe"]["root_radius_m"],
        metadata["recipe"]["terminal_radius_m"],
    )
    paths = dominant_paths(
        crown.positions, crown.parents, radii, max_nodes=16384, continuations=hints
    )
    with np.load(inputs / "spatial/wood.npz") as z:
        wood = dict(z)
    limbs = saved_limbs(wood, paths)
    results = []
    for representation in ("limbs", "cells"):
        began = time.perf_counter()
        try:
            sources, owners = local_sources(limbs, paths, node, representation)
            result = join_sources(sources)
            if result is None:
                raise ValueError("local source output exceeds existing capacity")
            mesh, work = result
            checks = check_mesh(mesh)
            np.savez_compressed(output / f"{representation}.npz", **mesh)
            results.append(
                {
                    "representation": representation,
                    "owners": owners,
                    "work": work,
                    "checks": checks,
                    "elapsed_s": time.perf_counter() - began,
                }
            )
        except ValueError as e:
            results.append(
                {
                    "representation": representation,
                    "refused": str(e),
                    "elapsed_s": time.perf_counter() - began,
                }
            )
    from fork_surface import blocked_voxel_mesh, voxel_mesh

    field, plan = collar_plan(crown, radii, paths, node)
    records = []
    accepted = None
    for method, sampler in (("dense", voxel_mesh), ("blocked", blocked_voxel_mesh)):
        began = time.perf_counter()
        collar, work = sampler(field, cells)
        check_mesh(collar)
        if accepted is not None:
            for key in ("vertices", "faces", "normals"):
                np.testing.assert_array_equal(accepted[key], collar[key])
        accepted = collar
        records.append(
            {"method": method, "elapsed_s": time.perf_counter() - began, "work": work}
        )
    np.savez_compressed(output / "collar.npz", **accepted)
    accuracy = ray_accuracy(accepted, plan)
    for row in results:
        if "refused" in row:
            continue
        began = time.perf_counter()
        try:
            with np.load(output / f"{row['representation']}.npz") as z:
                base = dict(z)
            result, work = add_collars(base, [accepted], [node])
            np.savez_compressed(
                output / f"{row['representation']}-collared.npz", **result
            )
            row["collared"] = {"work": work, "elapsed_s": time.perf_counter() - began}
        except ValueError as e:
            row["collared_refusal"] = str(e)
    report = {
        "node": node,
        "cells": cells,
        "alternatives": results,
        "sampling": records,
        "accuracy": accuracy,
        "plan": plan,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "plan"}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--node", type=int, required=True)
    parser.add_argument("--cells", type=int, default=96)
    args = parser.parse_args()
    study(args.inputs, args.output, args.node, args.cells)
