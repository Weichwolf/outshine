"""Build an experimental bounded surface hull from existing root source domains."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_appearance import policy_digest
from building_mass_materials import policy
from building_surface_samples import emit_samples, sample_surfaces, validate
from building_tree_load import read_tree
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_material_patches import shape_material_patches


def compile_root(parent, root, cell, at, *, all_depths=False):
    case = json.loads((root / "report.json").read_text())
    tree = read_tree(root)
    node_path = root / case["node_files"]["0"]
    with np.load(node_path, allow_pickle=False) as archive:
        wanted = set(map(int, archive["g__group_sources"]))
        needs = archive["g__needs_sources"]
    patches, seen = [], set()
    before_bytes, before_reads = parent.form_bytes, parent.form_reads
    for _, identities, forms in parent.iter_forms({cell}):
        for identity, surface in zip(identities, forms, strict=True):
            identity = int(identity)
            if identity not in wanted:
                continue
            for part, shape in enumerate(surface["shapes"]):
                patches.extend(
                    shape_material_patches(
                        surface, shape, identity, part, tree["basis"], tree["anchor"]
                    )
                )
            seen.add(identity)
    if seen != wanted:
        raise ValueError("all root source domains must remain available")
    sampled, spacing, sampling = sample_surfaces(
        patches, tree["node_bounds"][0], all_depths=all_depths
    )
    result, emission = emit_samples(patches, sampled, spacing, at)
    selected_sources = {patches[int(i)].geometry.source for i in sampled["patches"]}
    report = {
        "cell": cell,
        "source_archive_sha256": parent.archive_sha256,
        "decoded_form_bytes": parent.form_bytes - before_bytes,
        "form_reads": parent.form_reads - before_reads,
        "qualified_sources": len(wanted),
        "sources_without_surface_sample": [
            {
                "source": i,
                "reason": "not sampled by fixed orthographic rays; approximation, not proven hidden",
            }
            for i in sorted(wanted - selected_sources)
        ],
        "sampling": sampling,
        "emission": emission,
    }
    result.update(
        {
            "basis": tree["basis"],
            "anchor": tree["anchor"],
            "needs_sources": needs,
            "group_sources": np.array(sorted(wanted), np.uint32),
            "node_sha256": np.frombuffer(
                hashlib.sha256(node_path.read_bytes()).digest(), np.uint8
            ).copy(),
            "policy_sha256": np.frombuffer(policy_digest(at), np.uint8).copy(),
        }
    )
    return result, report


def read_surfels(path, node, tree, at):
    with np.load(path, allow_pickle=False) as data:
        arrays = {
            k: data[k]
            for k in (
                "positions",
                "normal",
                "albedo",
                "roughness_weights",
                "roughness_palette",
                "spacing",
                "basis",
                "anchor",
                "needs_sources",
                "node_sha256",
                "policy_sha256",
            )
        }
    if (
        arrays["node_sha256"].tobytes() != hashlib.sha256(node.read_bytes()).digest()
        or arrays["policy_sha256"].tobytes() != policy_digest(at)
        or not np.array_equal(arrays["basis"], tree["basis"])
        or not np.array_equal(arrays["anchor"], tree["anchor"])
        or not np.array_equal(arrays["roughness_palette"], at["palette"])
    ):
        raise ValueError("surface hull source, material or frame differs")
    validate(arrays)
    return arrays


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--all-depths", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    receipt = json.loads((args.input / "report.json").read_text())
    reports = []
    with open_parent(args.source) as parent:
        if parent.archive_sha256 != receipt["source_archive_sha256"]:
            raise ValueError("source root archive differs")
        for cell in args.cells:
            arrays, work = compile_root(
                parent,
                args.input / f"cell-{cell}",
                cell,
                policy(),
                all_depths=args.all_depths,
            )
            path = args.output / f"cell-{cell}.npz"
            np.savez(path, **arrays)
            work.update(
                {
                    "all_array_bytes": sum(v.nbytes for v in arrays.values()),
                    "container_bytes": path.stat().st_size,
                }
            )
            reports.append(work)
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": reports,
        "all_depths": args.all_depths,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Fixed16x16per axis, source domains before triangulation. Hull or all-depth control explicitly selected; at most65536all-depth intermediates and1536emitted samples, overflow fails without publishing that product. Neither six first views nor this coarse direct LDC sampling certifies arbitrary-view visibility. Missing interior/thin surfaces reported, original logical/near sources retained. No native cache/bindings or default replacement.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
