"""Measure metadata cuts before fine IO; candidate model, not a World gate."""

import argparse
import hashlib
import json
import platform
import subprocess
import time
from pathlib import Path

from experiment_output import create_output
from source_product_cut import SourceProducts


def measure(tree, eye, focal, error):
    began = time.perf_counter()
    products, visited = tree.cut(eye, focal, error)
    milliseconds = (time.perf_counter() - began) * 1000
    tree.validate_cut(products)
    fields = [p for p in products if p.kind == "field"]
    sizes = {kind: sum(tree.nodes[p.node][7] for p in products if p.kind == kind)
             for kind in ("field", "geometry", "unresolved")}
    texels = sum(p.pixels**2 for p in fields)
    return products, {
        "selection_ms": milliseconds,
        "visited_nodes": visited,
        "products": len(products),
        "field_candidates": len(fields),
        "source_counts": sizes,
        "field_candidate_texels": texels,
        "logical_pbr_mip_bytes": 16 * texels - 4 * len(fields),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, required=True)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--base-key", required=True)
    parser.add_argument("--eye", type=float, nargs=3, required=True)
    parser.add_argument("--focal-px", type=float, required=True)
    parser.add_argument("--error-px", type=float, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    create_output(args.output)
    native = subprocess.check_output(
        [str(args.exporter.resolve()), str(args.database.resolve()), args.base_key,
         str(args.output.resolve() / "source")], text=True)
    native = json.loads(native)
    if native["form_read_bytes"] != 0:
        raise ValueError("metadata export touched fine source forms")
    inputs = {suffix: (args.output / ("source." + suffix)).read_bytes()
              for suffix in ("bin", "cells", "supported")}
    tree = SourceProducts(inputs["bin"], inputs["cells"], inputs["supported"],
                          revision=args.base_key)
    cases, baseline = {}, None
    for distance in (0, 1.5, 6, 30, 100, 300, 600):
        eye = [args.eye[0] + distance, *args.eye[1:]]
        products, cost = measure(tree, eye, args.focal_px, args.error_px)
        if baseline is None:
            baseline = {p.node: (p.kind, p.pixels) for p in products}
        current = {p.node for p in products}
        cost["reused_source_groups"] = len(baseline.keys() & current)
        cost["new_source_groups"] = len(current - baseline.keys())
        cost["retired_source_groups"] = len(baseline.keys() - current)
        cost["representation_changes"] = sum(
            p.node in baseline and baseline[p.node] != (p.kind, p.pixels) for p in products)
        cases[str(distance)] = cost
    report = {
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "source_sha256": {
            name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
            for name in ("source_product_cut.py", "source_product_inputs.cpp",
                         "compare_source_product_cut.py", "test_source_product_cut.py")
        },
        "input_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()},
        "platform": platform.platform(),
        "base_key": args.base_key,
        "eye_ecef_m": args.eye,
        "focal_px": args.focal_px,
        "allowed_centred_error_px": args.error_px,
        "native_metadata": native,
        "translations_ecef_x_m": cases,
        "scope": (
            "Complete source partition; centred capture candidates only. Source-group reuse "
            "does not prove view/material asset reuse. No frustum/occlusion, arbitrary-pose/"
            "material validation, GPU/frame/p99/memory-peak proof. Logical PBR payload is "
            "3 RGBA8 images with full power-of-two mips; device alignment, field archives, "
            "metadata and staging excluded. Publication requires independently validated "
            "current parents. Unresolved sources remain explicit and never ready."
        ),
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"sources": len(tree.order), "cases": cases}))


if __name__ == "__main__":
    main()
