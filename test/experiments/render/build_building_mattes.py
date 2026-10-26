"""Paired source-matte/prism experiment; the original-source remainder is identical."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_hierarchy import measure
from building_mass_appearance import group_materials, policy_digest
from building_mass_materials import policy
from building_matte_fields import (
    build_fields,
    merge_equal_normal_profiles,
    validate_fields,
)
from building_matte_planes import cover_domains
from building_tree_load import read_tree, render_arrays
from building_tree_products import build_cut, prepare_sources
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_material_patches import selected_material_patches

MATTE_KEYS = (
    "plane_origin",
    "plane_axes",
    "plane_low",
    "plane_shape",
    "plane_cell_offsets",
    "alpha",
    "cell_profile_offsets",
    "profile_normal",
    "profile_material",
    "profile_weight",
    "material_albedo",
    "material_roughness_weights",
    "roughness_palette",
    "spacing",
    "needs_sources",
    "basis",
    "anchor",
    "tree_sha256",
    "node_sha256",
    "policy_sha256",
)


def response_signatures():
    return {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in ("material_filter.py", "building_mass_lighting.py")
    }


def subset_cover(cover, patches, represented):
    sources = np.array([p.geometry.source for p in patches])
    assignment = cover["patch_planes"].copy()
    assignment[~np.isin(sources, represented)] = -1
    kept = assignment >= 0
    used, remap = np.unique(assignment[kept], return_inverse=True)
    assignment[kept] = remap
    return cover | {
        "patch_planes": assignment,
        "plane_normal": cover["plane_normal"][used],
        "plane_offset": cover["plane_offset"][used],
    }


def matched_prism(parent, cell, wanted, tree, at):
    masses, forms, source_work = prepare_sources(parent, {cell}, wanted, tree)
    product, geometry = build_cut(tree, [0], masses, tree)
    values, material_work = measure(
        forms, set(map(int, product["group_sources"])), tree, at
    )
    material = group_materials(values, product, at)
    if not material["supported"].all():
        raise ValueError("complete matched prism material measures required")
    return (
        product,
        material,
        {"source": source_work, "geometry": geometry, "material": material_work},
    )


def build_case(parent, root, cell, output, at):
    output.mkdir()
    wall, cpu = time.perf_counter(), time.process_time()
    tree = read_tree(root)
    case = json.loads((root / "report.json").read_text())
    node_path = root / case["node_files"]["0"]
    with np.load(node_path, allow_pickle=False) as node:
        wanted = node["g__group_sources"]
    patches = selected_material_patches(parent, {cell}, wanted, tree)
    cover, covering = cover_domains(patches, tree["node_bounds"][0])
    source_cpu = (time.process_time() - cpu) * 1000
    before = time.process_time()
    product, material, prism_work = matched_prism(
        parent,
        cell,
        set(map(int, np.setdiff1d(wanted, cover["source_fallback"]))),
        tree,
        at,
    )
    control_cpu = (time.process_time() - before) * 1000
    before = time.process_time()
    cover = subset_cover(cover, patches, product["group_sources"])
    fields, field_work = build_fields(patches, cover, tree["node_bounds"][0], at)
    fields.update(
        {
            "needs_sources": product["needs_sources"],
            "basis": tree["basis"],
            "anchor": tree["anchor"],
            "tree_sha256": tree["tree_sha256"],
            "node_sha256": np.frombuffer(
                hashlib.sha256(node_path.read_bytes()).digest(), np.uint8
            ).copy(),
            "policy_sha256": np.frombuffer(policy_digest(at), np.uint8).copy(),
        }
    )
    field_cpu = (time.process_time() - before) * 1000
    before = time.process_time()
    merged, merge_work = merge_equal_normal_profiles(fields)
    merge_cpu = (time.process_time() - before) * 1000
    payload = {
        "prism": {"g__" + k: v for k, v in product.items()}
        | {"m__" + k: v for k, v in material.items()},
        "matte-separate": fields,
        "matte-merged": merged,
    }
    files = {}
    for name, arrays in payload.items():
        path = output / (name + ".npz")
        np.savez(path, **arrays)
        files[name] = {
            "raw_all_array_bytes": sum(v.nbytes for v in arrays.values()),
            "container_bytes": path.stat().st_size,
        }
    report = {
        "cell": cell,
        "source_archive_sha256": parent.archive_sha256,
        "cover": covering,
        "field": field_work,
        "merge": merge_work,
        "prism_control": prism_work,
        "represented_sources": product["group_sources"].tolist(),
        "original_sources": product["needs_sources"].tolist(),
        "files": files,
        "cost": {
            "source_domains_and_cover_cpu_ms": source_cpu,
            "matched_prism_control_cpu_ms": control_cpu,
            "matte_fields_cpu_ms": field_cpu,
            "normal_merge_cpu_ms": merge_cpu,
            "complete_cpu_ms": (time.process_time() - cpu) * 1000,
            "complete_wall_ms": (time.perf_counter() - wall) * 1000,
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def read_matte(path, node_path, tree, at):
    with np.load(path, allow_pickle=False) as archive:
        arrays = {k: archive[k] for k in MATTE_KEYS}
    if (
        arrays["node_sha256"].tobytes()
        != hashlib.sha256(node_path.read_bytes()).digest()
        or arrays["policy_sha256"].tobytes() != policy_digest(at)
        or not all(
            np.array_equal(arrays[k], tree[k])
            for k in ("basis", "anchor", "tree_sha256")
        )
        or not np.array_equal(arrays["roughness_palette"], at["palette"])
    ):
        raise ValueError("matte source, hierarchy, frame or material model differs")
    validate_fields(arrays)
    return arrays


def read_prism(path, tree, at):
    raw = render_arrays(path)
    if not all(
        np.array_equal(raw["g__" + k], tree[k])
        for k in ("basis", "anchor", "tree_sha256")
    ) or raw["m__policy_sha256"].tobytes() != policy_digest(at):
        raise ValueError("paired prism source frame or material model differs")
    return {k[3:]: v for k, v in raw.items() if k.startswith("g__")}, {
        k[3:]: v for k, v in raw.items() if k.startswith("m__")
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    receipt = json.loads((args.input / "report.json").read_text())
    with open_parent(args.source) as parent:
        if receipt["source_archive_sha256"] != parent.archive_sha256:
            raise ValueError("source root archive differs")
        reports = [
            build_case(
                parent, args.input / f"cell-{c}", c, args.output / f"cell-{c}", policy()
            )
            for c in args.cells
        ]
    result = {
        "response_program_sha256": response_signatures(),
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Paired experimental source products before any native/fine image reference. Existing prism control defines common whole-source remainder; its source/morphology/material cost is separate. Finite candidate cover and cell filtering are not native/occluder/world/device admission or a global image bound.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
