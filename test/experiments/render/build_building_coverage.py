"""Compile coverage-only sidecars from cached BSC3 forms and existing coarse nodes."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_coverage_field import build_field, validate_field
from building_tree_load import read_tree
from building_tree_products import prepare_sources
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes

NODE_KEYS = (
    "positions",
    "indices",
    "triangle_groups",
    "group_sources",
    "group_offsets",
    "tree_sha256",
)


def read_node(path):
    with np.load(path, allow_pickle=False) as archive:
        return {k: archive["g__" + k] for k in NODE_KEYS}


def compile_node(product, masses):
    fields, reports = [], []
    for group, (begin, end) in enumerate(
        zip(product["group_offsets"][:-1], product["group_offsets"][1:], strict=True)
    ):
        ids = product["group_sources"][begin:end]
        field, work = build_field(
            [masses[int(i)] for i in ids],
            product["positions"],
            product["indices"][product["triangle_groups"] == group],
        )
        fields.append(field)
        reports.append({"sources": len(ids), "projections": work})
    result = {k: np.stack([f[k] for f in fields]) for k in ("values", "bounds")}
    validate_field(result, len(fields))
    return result, reports


def compile_case(parent, root, output, cell):
    output.mkdir()
    case = json.loads((root / "report.json").read_text())
    tree = read_tree(root)
    nodes = {name: read_node(root / name) for name in case["node_files"].values()}
    for node in nodes.values():
        if not np.array_equal(node["tree_sha256"], tree["tree_sha256"]):
            raise ValueError("node source hierarchy differs")
    requested = {int(i) for node in nodes.values() for i in node["group_sources"]}
    masses, _, work = prepare_sources(parent, {cell}, requested, tree)
    if requested != masses.keys():
        raise ValueError("all existing parent sources must remain describable")
    reports = []
    for name, product in nodes.items():
        fields, measures = compile_node(product, masses)
        raw = (root / name).read_bytes()
        fields["source_sha256"] = np.frombuffer(
            hashlib.sha256(raw).digest(), np.uint8
        ).copy()
        np.savez(output / name, **fields)
        reports.append(
            {
                "file": name,
                "groups": measures,
                "decoded_node_array_bytes": sum(v.nbytes for v in product.values()),
                "hashed_node_container_bytes": len(raw),
                "coverage_array_bytes": sum(v.nbytes for v in fields.values()),
                "coverage_container_bytes": (output / name).stat().st_size,
            }
        )
    return {"cell": cell, "source_work": work, "nodes": reports}


def read_field(path, source, groups):
    with np.load(path, allow_pickle=False) as archive:
        field = {k: archive[k] for k in ("values", "bounds", "source_sha256")}
    raw = source.read_bytes()
    if field.pop("source_sha256").tobytes() != hashlib.sha256(raw).digest():
        raise ValueError("coverage field source node differs")
    validate_field(field, groups)
    return field, len(raw)


def field_cut(root, field_root, case, query, groups):
    fields, hashed = [], 0
    for node in query["nodes"]:
        name = case["node_files"].get(str(node))
        if name is None:
            continue
        with np.load(root / name, allow_pickle=False) as source:
            count = len(source["g__group_coverage"])
        field, cost = read_field(field_root / name, root / name, count)
        fields.append(field)
        hashed += cost
    if not fields:
        raise ValueError("this bounded comparison requires nonempty parent products")
    result = {k: np.concatenate([f[k] for f in fields]) for k in ("values", "bounds")}
    validate_field(result, groups)
    return result, {
        "decoded_field_bytes": sum(v.nbytes for v in result.values()),
        "hashed_node_container_bytes": hashed,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    source_report = json.loads((args.input / "report.json").read_text())
    with open_parent(args.source) as parent:
        if source_report["source_archive_sha256"] != parent.archive_sha256:
            raise ValueError("source parent archive differs")
        reports = [
            compile_case(
                parent, args.input / f"cell-{cell}", args.output / f"cell-{cell}", cell
            )
            for cell in args.cells
        ]
        digest = parent.archive_sha256
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_archive_sha256": digest,
        "cells": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Fixed8x8 XY/XZ/YZ conditional area maps from BSC3 macroprisms, not fine meshes, physical visibility or shadow certificates. Side projections retain component-extent envelopes. Original coarse geometry/materials/fallback remain unchanged. Sidecars add bytes; no native cache/binding change.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
