"""Selective file bytes, locality and previous-demand reuse at equal candidates."""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import numpy as np
from compare_source_hierarchy import changed_frame, equivalent
from experiment_output import create_output
from field_view_cost import measured
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from source_group_payloads import Reads, build, query, read_original
from source_hierarchy import prepare
from source_hierarchy import query as resident_query
from source_raster_candidates import select


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    names = (
        "compare_source_payloads.py",
        "source_group_payloads.py",
        "source_cluster_bounds.py",
        "source_hierarchy.py",
        "source_triangle_clusters.py",
        "source_raster_candidates.py",
        "field_view_cost.py",
        "native_world_inputs.py",
        "native_source_triangle_materials.py",
        "building_sample_originals.py",
        "compare_source_hierarchy.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze measured sources before recording costs")
    create_output(args.output)
    manifest, _, _, source = load(DATA, attributes=True)
    _, binding = bound_selection(DATA, source)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "platform": platform.platform(),
        "numpy": np.__version__,
        "load_average": list(os.getloadavg()),
        "native_packet_sha256": binding["samples_sha256"],
        "build": {},
        "queries": {},
        "motion": {},
        "scope": "Private generated file experiment; warm OS cache (files just written), no cache eviction or physical SSD/UMA/p99/native admission. Actual pread requests/returned bytes recorded, logical4096-byte file pages separately. Original attributes remain with existing source/native consumer. Per-query traces exclude pre-held metadata and input oracle arrays; held bytes counted separately. Synthetic words, not native motion captures.",
    }
    layouts = {}
    for name in ("source", "morton", "size"):
        count = [0]

        def construct(name=name, count=count):
            folder = args.output / f"layout-{name}-{count[0]}"
            count[0] += 1
            return build(source, folder, order=name)

        layout, cost = measured(construct, args.output, "build-" + name)
        layouts[name] = layout
        report["build"][name] = {
            "cost": cost,
            "metadata_bytes": layout.metadata_bytes,
            "original_file_bytes": layout.original.stat().st_size,
            "additional_corner_file_bytes": layout.corners.stat().st_size,
        }
        for i in range(count[0] - 1):
            shutil.rmtree(args.output / f"layout-{name}-{i}")
    h = prepare(source)
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as a:
        placement = a["placements/words"]
        raster = manifest["raster"]

        def original(view, guard):
            reads = Reads()
            mesh = read_original(layouts["source"], reads)
            ids, cost = select(mesh, view, placement, raster, raster_guard_px=guard)
            return ids, {**cost, "payload": reads.work()}

        for pose in ("near-direction", "revealing-side"):
            view = a[pose + "/view_words"]
            for guard in (0.0, 1 / 256):
                label = pose + "-guard-" + str(guard)
                expected = select(
                    source, view, placement, raster, raster_guard_px=guard
                )
                methods = {
                    "whole-file": lambda view=view, guard=guard: original(view, guard),
                    "resident-hierarchy": lambda view=view, guard=guard: resident_query(
                        h, view, placement, raster, raster_guard_px=guard
                    ),
                }
                for name, layout in layouts.items():
                    methods[name] = lambda layout=layout, view=view, guard=guard: query(
                        layout,
                        view,
                        placement,
                        raster,
                        raster_guard_px=guard,
                        reads=Reads(),
                    )
                rows = {}
                for name, call in methods.items():
                    actual, cost = measured(call, args.output, label + "-" + name)
                    equivalent(actual, expected)
                    rows[name] = {
                        "cost": cost,
                        "work": actual[1],
                        "selected": len(actual[0]),
                        "ids_sha256": hashlib.sha256(actual[0].tobytes()).hexdigest(),
                    }
                report["queries"][label] = rows
        base = a["near-direction/view_words"]
        for mode, count, step in (
            ("turn", 12, np.zeros(3)),
            ("translation", 12, np.array([100, 25, -200])),
            ("frame-step-translation", 60, np.array([5, 0, -2])),
        ):
            views = [
                changed_frame(base, index * 30 if mode == "turn" else 0, index * step)
                for index in range(count)
            ]
            expected = [
                select(source, v, placement, raster, raster_guard_px=0.0) for v in views
            ]

            def run(method, views=views, expected=expected):
                previous = None
                rows = []
                for view, want in zip(views, expected, strict=True):
                    if method == "whole-file":
                        actual = original(view, 0.0)
                    else:
                        reads = Reads(previous if method == "previous-demand" else None)
                        actual = query(
                            layouts["source"],
                            view,
                            placement,
                            raster,
                            raster_guard_px=0.0,
                            reads=reads,
                        )
                        previous = reads.retain(layouts["source"])
                    equivalent(actual, want)
                    rows.append({"selected": len(actual[0]), "work": actual[1]})
                return rows

            report["motion"][mode] = {}
            for name in ("whole-file", "source", "previous-demand"):
                rows, cost = measured(
                    lambda name=name: run(name), args.output, mode + "-" + name
                )
                report["motion"][mode][name] = {
                    "cost": cost,
                    "rows": rows,
                    "total_returned_bytes": sum(
                        r["work"]["payload"]["returned_bytes"] for r in rows
                    ),
                    "total_requests": sum(
                        r["work"]["payload"]["requests"] for r in rows
                    ),
                    "sum_query_logical_pages_4096": sum(
                        r["work"]["payload"]["logical_pages_4096"] for r in rows
                    ),
                    "whole_fallbacks": sum(
                        not r["work"]["complete_model"] for r in rows
                    ),
                }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": report["commit"],
                "all_candidates_and_fallbacks_equal": True,
                "report": str(args.output / "report.json"),
            }
        )
    )


if __name__ == "__main__":
    main()
