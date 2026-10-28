"""Actual bound local end surfaces; no complete contact or World admission."""

import argparse
import json
import platform
import subprocess
import time
from functools import partial
from pathlib import Path

import numpy as np
import shapely
from bound_bridge_end_surface import build_end_surface
from bridge_end_surface_costs import traced, triangulation_comparison
from bridge_end_surface_inputs import actual_end_inputs
from bridge_end_surface_plot import plot_end_surfaces


def serial(value):
    if hasattr(value, "items"):
        return dict(value)
    if hasattr(value, "tolist"):
        return value.tolist()
    raise TypeError(type(value).__name__)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("auto", "lp"), default="auto")
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--allocations", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, index, prepared, ground, preparation = actual_end_inputs(args.method)
    original = (
        ground["surface"].vertices.tobytes(),
        ground["surface"].triangles.tobytes(),
    )
    unknown = source.unknown_nodes
    pieces, records = [], []
    for value in ground["terminal_values"]:
        cap = value["source_terminal"]
        group = index.groups_by_node[cap["source_node"]]
        if not any(index.index[1][m.edge[:2]].bridge for m in group.members):
            continue
        invoke = partial(build_end_surface, prepared, ground, index, group.identifier)
        began = time.perf_counter()
        mesh, row = invoke()
        row["whole_end_stage_ms"] = 1000 * (time.perf_counter() - began)
        name = group.identifier.rsplit(":", 1)[-1]
        if args.allocations:
            row["whole_end_stage_allocations"] = traced(
                args.output / (name + "-end-stage.bin"), invoke
            )
        row["triangulation_comparison"] = triangulation_comparison(
            mesh,
            trace_root=args.output / (name + "-triangulation")
            if args.allocations
            else None,
        )
        np.savez_compressed(
            args.output / (name + ".npz"),
            vertices=mesh.vertices,
            faces=mesh.triangles,
            axis_port_enu=row["quality"]["axis_port_enu"],
        )
        pieces.append((name, mesh, row))
        records.append(row)
    assert len(pieces) == 2
    assert unknown == source.unknown_nodes
    assert original == (
        ground["surface"].vertices.tobytes(),
        ground["surface"].triangles.tobytes(),
    )
    np.savez_compressed(
        args.output / "ground.npz",
        vertices=ground["surface"].vertices,
        faces=ground["surface"].triangles,
    )
    if args.plot:
        plot_end_surfaces(args.output / "diagnostic.png", ground["surface"], pieces)
    report = {
        "runtime_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
        "scope": "Two actual Source-bound local metric surfaces from one existing qualified Ground field. No additional DEM, World joins, whole Bridge, terrain/crossing/body clearance or completed endpoint authority. Whole-stage measurements include structured binding equality, guards, transforms and finite quality; explicit member counters do not count metadata equality or numeric/kernel operations. Tracked allocations are neither total copied bytes nor process/device peaks; single shared-host diagnostics.",
        "preparation": preparation,
        "source_unknown_count": len(unknown),
        "ground_unchanged": True,
        "method": args.method,
        "platform": {
            "system": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "shapely": shapely.__version__,
            "geos": shapely.geos_version_string,
        },
        "records": records,
    }
    (args.output / "report.json").write_text(
        json.dumps(report, default=serial, indent=2) + "\n"
    )
    print(
        json.dumps(
            [
                {
                    "group": r["group"],
                    "whole_end_stage_ms": r["whole_end_stage_ms"],
                    "quality": {
                        k: r["quality"][k]
                        for k in (
                            "minimum_width_m",
                            "maximum_normalized_source_grade",
                            "maximum_finite_plane_gradient_error",
                            "coverage_error_m2",
                        )
                    },
                    "triangulation": r["triangulation_comparison"],
                }
                for r in records
            ],
            default=serial,
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
