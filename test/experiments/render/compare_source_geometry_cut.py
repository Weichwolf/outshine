"""Compare cached source metadata and intrinsic geometry selection domains."""

import argparse
import hashlib
import json
import math
import platform
import statistics
import subprocess
import time
from pathlib import Path

from source_geometry_cut import Camera, GeometrySources
from panoramic_geometry_cut import PanoramicView
from source_product_cut import SourceProducts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-prefix", type=Path, required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--eye", type=float, nargs=3, required=True)
    parser.add_argument("--target", type=float, nargs=3)
    parser.add_argument("--focal-px", type=float, default=1000)
    parser.add_argument("--orthographic-px-per-m", type=float)
    parser.add_argument("--panoramic-viewport", type=float, nargs=2, metavar=("WIDTH", "HEIGHT"))
    parser.add_argument("--error-px", type=float, default=1)
    parser.add_argument("--lookahead-m", type=float, nargs="+", default=[0, 100, 1000])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-control", type=Path)
    parser.add_argument("--asset-db", type=Path)
    args = parser.parse_args()
    if args.panoramic_viewport and (args.target or args.orthographic_px_per_m is not None):
        parser.error("panoramic selection requires a centred perspective viewport and no target")
    if args.native_control and (
        not args.asset_db
        or args.target is not None
        or args.orthographic_px_per_m is not None
    ):
        parser.error(
            "native comparison requires --asset-db and the default perspective target"
        )
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
        camera = (
            PanoramicView(tuple(args.eye), scale, *args.panoramic_viewport, radius=radius)
            if args.panoramic_viewport
            else Camera.toward(args.eye, target, scale, radius=radius,
                               orthographic=args.orthographic_px_per_m is not None)
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
        native = None
        if args.native_control:
            command = [
                str(args.native_control.resolve()),
                str(args.asset_db.resolve()),
                args.source_revision,
                str(len(tree.order)),
                *map(str, args.eye),
                str(args.focal_px),
                str(args.error_px),
                str(radius),
            ]
            if args.panoramic_viewport:
                command.extend(map(str, args.panoramic_viewport))
            result = subprocess.run(command, capture_output=True, text=True, check=True)
            native = json.loads(result.stdout)
            expected = [[p.node, p.kind] for p in cut]
            if [p[:2] for p in native["products"]] != expected or native[
                "visited"
            ] != visited:
                raise ValueError(
                    "native product partition differs from the Python reference"
                )
            for actual, product in zip(native["products"], cut):
                if not all(
                    math.isclose(a, b, rel_tol=2e-9, abs_tol=1e-9)
                    for a, b in zip(
                        actual[2:], [product.error_m, product.selection_error_px]
                    )
                ):
                    raise ValueError(
                        "native declared error differs from the Python reference"
                    )
            native = {key: value for key, value in native.items() if key != "products"}
            native["command"] = command
        cases.append(
            {
                "lookahead_m": radius,
                "native": native,
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
        "panoramic_geometry_cut.py",
        "test_panoramic_geometry_cut.py",
        "source_product_inputs.cpp",
        "source_geometry_control.cpp",
        "compare_source_geometry_cut.py",
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "native_control_sha256": hashlib.sha256(
            args.native_control.read_bytes()
        ).hexdigest()
        if args.native_control
        else None,
        "source_revision": args.source_revision,
        "eye_ecef_m": args.eye,
        "target_ecef_m": None if args.panoramic_viewport else target,
        "scale": scale,
        "orthographic": args.orthographic_px_per_m is not None,
        "panoramic_viewport_px": args.panoramic_viewport,
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
        "errors and real arithmetic only; no native rendered-product/GPU/rounding/material/coverage "
        "or frame-budget acceptance. Hulls are render proxies, not functional CAD "
        "assets. Shell batching reduces selection/products, not source triangles. "
        "Logical addresses do not certify native material/frame cache identity.",
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"sources": len(tree.order), "cases": cases}))


if __name__ == "__main__":
    main()
