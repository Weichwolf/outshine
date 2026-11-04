"""Compare cached source metadata and intrinsic geometry selection domains."""

import argparse
import hashlib
import json
import platform
import statistics
import subprocess
import time
from pathlib import Path

from source_geometry_cut import Camera, GeometrySources
from source_product_cut import SourceProducts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-prefix", type=Path, required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--eye", type=float, nargs=3, required=True)
    parser.add_argument("--target", type=float, nargs=3)
    parser.add_argument("--focal-px", type=float, default=1000)
    parser.add_argument("--orthographic-px-per-m", type=float)
    parser.add_argument("--error-px", type=float, default=1)
    parser.add_argument("--lookahead-m", type=float, nargs="+", default=[0, 100, 1000])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = {
        suffix: args.input_prefix.with_suffix(suffix).read_bytes()
        for suffix in [".bin", ".cells", ".supported", ".geometry"]
    }
    tree = SourceProducts(
        inputs[".bin"],
        list(inputs[".cells"]),
        inputs[".supported"],
        revision=args.source_revision,
    )
    geometry = GeometrySources(tree, inputs[".geometry"])
    target = args.target if args.target is not None else tree.centres[0]
    scale = (
        args.focal_px
        if args.orthographic_px_per_m is None
        else args.orthographic_px_per_m
    )
    cases = []
    baseline = None
    for radius in args.lookahead_m:
        camera = Camera.toward(
            args.eye,
            target,
            scale,
            radius=radius,
            orthographic=args.orthographic_px_per_m is not None,
        )
        elapsed = []
        for _ in range(3):
            start = time.perf_counter_ns()
            cut, visited = geometry.cut(camera, args.error_px)
            elapsed.append((time.perf_counter_ns() - start) / 1e6)
        current = {product.address for product in cut}
        baseline = current if baseline is None else baseline
        kinds = {
            kind: sum(p.kind == kind for p in cut)
            for kind in ["hull", "shell", "original", "unresolved"]
        }
        hull_sources = sum(tree.nodes[p.node][7] for p in cut if p.kind == "hull")
        unresolved = sum(tree.nodes[p.node][7] for p in cut if p.kind == "unresolved")
        cases.append(
            {
                "lookahead_m": radius,
                "products": len(cut),
                "visited": visited,
                "kinds": kinds,
                "sources_in_hulls": hull_sources,
                "mesh_source_count": len(tree.order) - hull_sources - unresolved,
                "hull_triangles": kinds["hull"] * 12,
                "largest_product_sources": max(tree.nodes[p.node][7] for p in cut),
                "logical_groups_reused": len(current & baseline),
                "median_python_ms": statistics.median(elapsed),
            }
        )
    sources = [
        "source_geometry_cut.py",
        "test_source_geometry_cut.py",
        "source_product_inputs.cpp",
        "compare_source_geometry_cut.py",
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "source_revision": args.source_revision,
        "eye_ecef_m": args.eye,
        "target_ecef_m": target,
        "scale": scale,
        "orthographic": camera.orthographic,
        "allowed_error_px": args.error_px,
        "sources": len(tree.order),
        "declared_faces": sum(geometry.faces),
        "cases": cases,
        "input_sha256": {k: hashlib.sha256(v).hexdigest() for k, v in inputs.items()},
        "code_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in sources
        },
        "scope": "One native cached parent, not a full Place. Declared source bounds/shell "
        "errors and real arithmetic only; no native/GPU/rounding/material/coverage "
        "or frame-budget acceptance. Hulls are render proxies, not functional CAD "
        "assets. Shell batching reduces selection/products, not source triangles. "
        "Logical addresses do not certify native material/frame cache identity.",
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"sources": len(tree.order), "cases": cases}))


if __name__ == "__main__":
    main()
