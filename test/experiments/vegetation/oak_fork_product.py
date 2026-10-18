"""Demanded disjoint fork groups replace saved axes; all other cached wood stays exact."""

import argparse
import hashlib
import importlib.metadata
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from collar_union_product import add_collars
from crown_collars import collar_plan
from crown_union import join_sources
from fork_surface import blocked_voxel_mesh
from limb_paths import dominant_paths
from oak_fork_probe import local_sources, saved_limbs
from oak_scaffold import tapered_radii
from oak_skin_experiment import source_crown
from tree_entry import load

UNASSIGNED = np.iinfo(np.uint32).max


def requested_groups(limbs, paths, nodes):
    nodes = tuple(nodes)
    if (
        len(nodes) > 8
        or len(set(nodes)) != len(nodes)
        or any(type(n) is not int or n <= 0 for n in nodes)
    ):
        raise ValueError("at most eight unique explicit non-root fork IDs required")
    groups = []
    for node in sorted(nodes):
        if not any(p[0] == node for p in paths):
            raise ValueError("requested source is not a side fork")
        sources, owners = local_sources(limbs, paths, node, "limbs")
        merged = {s["source_node"]: s for s in sources}
        members = {node}
        overlap = [i for i, g in enumerate(groups) if set(owners).intersection(g[1])]
        for i in reversed(overlap):
            other_nodes, other = groups.pop(i)
            members.update(other_nodes)
            for key, mesh in other.items():
                caps = set(mesh["forbidden_caps"]) | set(
                    merged.get(key, mesh)["forbidden_caps"]
                )
                merged[key] = mesh | {"forbidden_caps": tuple(sorted(caps))}
        groups.append((members, merged))
    return [
        (tuple(sorted(ns)), [ms[k] for k in sorted(ms)], sorted(ms))
        for ns, ms in sorted(groups, key=lambda g: min(g[0]))
    ]


def replace_groups(original, patches):
    owners = [v for _, _, group in patches for v in group]
    if len(set(owners)) != len(owners):
        raise ValueError("disjoint complete source-axis groups required")
    if not patches:
        return original, {"removed_faces": 0, "copied_array_bytes": 0}
    if np.any(original["surface"]) or np.any(original["corner_uv"]):
        raise ValueError(
            "this prepared wood has only the existing uncharted bark material"
        )
    removed = np.isin(original["limb_ids"], owners)
    if any(not np.any(original["limb_ids"] == owner) for owner in owners):
        raise ValueError("all requested source axes must exist in the saved wood")
    kept = original["faces"][~removed]
    vertices, inverse = np.unique(kept, return_inverse=True)
    result = {
        "vertices": [original["vertices"][vertices]],
        "faces": [inverse.reshape(-1, 3)],
        "normals": [original["normals"].reshape(-1, 3, 3)[~removed].reshape(-1, 3)],
        "surface": [original["surface"][~removed]],
        "corner_uv": [original["corner_uv"].reshape(-1, 3, 2)[~removed].reshape(-1, 2)],
        "limb_ids": [original["limb_ids"][~removed]],
        "patch_nodes": [np.full(len(kept), UNASSIGNED, np.uint32)],
    }
    offset = len(vertices)
    for node, mesh, _ in patches:
        nf = len(mesh["faces"])
        result["vertices"].append(mesh["vertices"])
        result["faces"].append(mesh["faces"] + offset)
        result["normals"].append(mesh["normals"])
        result["surface"].append(np.zeros(nf, np.uint8))
        result["corner_uv"].append(np.zeros((3 * nf, 2)))
        result["limb_ids"].append(
            np.where(mesh["source_kind"] == 0, mesh["source_node"], UNASSIGNED).astype(
                np.uint32
            )
        )
        result["patch_nodes"].append(np.full(nf, node, np.uint32))
        offset += len(mesh["vertices"])
    result = {k: np.concatenate(v) for k, v in result.items()}
    result["faces"] = result["faces"].astype(np.int32)
    return result, {
        "removed_faces": int(removed.sum()),
        "copied_array_bytes": sum(v.nbytes for v in result.values()),
        "note": "concatenation payload only; masks, unique remap and kernel traffic are additional",
    }


def export(inputs, output, study, entry, wood, patches, plans, nodes, cells):
    recipe = {
        "format": "oak-local-axis-forks-1",
        "source_product_identity": entry["product_identity"],
        "source_wood_sha256": hashlib.sha256(
            (inputs / "spatial/wood.npz").read_bytes()
        ).hexdigest(),
        "nodes": sorted(nodes),
        "cells": cells,
        "strength": 2.0,
        "curve_tolerance_m": 0.001,
        "manifold_version": importlib.metadata.version("manifold3d"),
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    output.mkdir(parents=True, exist_ok=False)
    for label in ("recursive", "spatial"):
        folder = output / label
        folder.mkdir()
        for source in (inputs / "spatial").iterdir():
            if source.is_file():
                (folder / source.name).symlink_to(source.resolve())
    new = entry | {
        "product_identity": identity,
        "product_recipe": recipe,
        "wood_triangles": len(wood["faces"]),
        "scope": "Same complete saved skeleton/foliage/materials; requested fork groups joined locally. Unrequested limb/shoot bodies remain separate; no species/LOD/native/device admission.",
        "instanced_triangles": entry["instanced_triangles"]
        + len(wood["faces"])
        - entry["wood_triangles"],
    }
    target = output / "spatial"
    for name in ("entry.json", "wood.npz"):
        (target / name).unlink()
    np.savez_compressed(target / "wood.npz", **wood)
    (target / "entry.json").write_text(json.dumps(new, indent=2) + "\n")
    for node, mesh, _ in patches:
        np.savez_compressed(output / f"patch-{node}.npz", **mesh)
    (output / "fork-plans.json").write_text(json.dumps(plans, indent=2) + "\n")
    pair = dict(study)
    pair["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    pair["source_build_commit"] = study["commit"]
    pair["scope"] = new["scope"]
    pair["products"] = {
        name: dict(study["products"]["spatial"]) for name in ("recursive", "spatial")
    }
    pair["products"]["spatial"].update(
        product_identity=identity,
        wood_triangles=len(wood["faces"]),
        instanced_triangles=new["instanced_triangles"],
        wood_array_bytes=sum(v.nbytes for v in wood.values()),
    )
    pair["presentation_labels"] = {
        "recursive": "separate side forks",
        "spatial": "demanded local fork groups",
    }
    (output / "comparison.json").write_text(json.dumps(pair, indent=2) + "\n")
    return identity


def build(inputs, output, nodes, cells=32):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    if output.exists():
        raise FileExistsError(output)
    if not nodes:
        raise ValueError("explicit nonempty fork demand required by this experiment")
    if type(cells) is not int or not 16 <= cells <= 192:
        raise ValueError("existing bounded collar resolution required")
    began = time.perf_counter()
    study = json.loads((inputs / "comparison.json").read_text())
    entry, products = load(inputs / "spatial")
    if entry["product_recipe"]["format"] != "oak-continuous-wood-1":
        raise ValueError("prepared continuous mature-oak wood format required")
    original = products["wood.npz"]
    crown, hints = source_crown(inputs, study)
    radii = tapered_radii(
        crown.parents,
        study["recipe"]["root_radius_m"],
        study["recipe"]["terminal_radius_m"],
    )
    paths = dominant_paths(
        crown.positions, crown.parents, radii, max_nodes=16384, continuations=hints
    )
    groups = requested_groups(saved_limbs(original, paths), paths, nodes)
    prepared = time.perf_counter()
    patches, plans = [], []
    for group_nodes, sources, owners in groups:
        collars, fork_plans = [], []
        for node in group_nodes:
            field, plan = collar_plan(crown, radii, paths, node)
            collar, sampling = blocked_voxel_mesh(field, cells)
            collars.append(collar)
            fork_plans.append(plan | {"sampling": sampling})
        result = join_sources(sources)
        if result is None:
            raise ValueError("local axes exceed unchanged union capacity")
        base, union = result
        mesh, final = add_collars(base, collars, group_nodes)
        patches.append((group_nodes[0], mesh, owners))
        plans.append(
            {
                "nodes": group_nodes,
                "forks": fork_plans,
                "owners": owners,
                "union": union,
                "collared": final,
            }
        )
    built = time.perf_counter()
    wood, work = replace_groups(original, patches)
    # Source entry does not require a wood count; this is local report metadata.
    entry = entry | {"wood_triangles": len(original["faces"])}
    identity = export(inputs, output, study, entry, wood, patches, plans, nodes, cells)
    report = {
        "product_identity": identity,
        "nodes": sorted(nodes),
        "source_decode_plan_s": prepared - began,
        "local_build_s": built - prepared,
        "aggregation_export_s": time.perf_counter() - built,
        "old_wood_faces": len(original["faces"]),
        "new_wood_faces": len(wood["faces"]),
        "work": work,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
    }
    (output / "fork-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--nodes", nargs="+", type=int, required=True)
    parser.add_argument("--cells", type=int, default=32)
    args = parser.parse_args()
    build(args.inputs, args.output, args.nodes, args.cells)
