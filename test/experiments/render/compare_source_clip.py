"""Cost of explicit whole-source XY clip proof versus original Whole fallback."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
from compare_source_hierarchy import changed_frame, equivalent
from experiment_output import create_output
from field_view_cost import measured
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from prepare_source_clip_controls import export_controls
from source_clip_demand import query
from source_hierarchy import prepare
from source_hierarchy import query as old_query


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    names = (
        "compare_source_clip.py",
        "prepare_source_clip_controls.py",
        "source_clip_demand.py",
        "source_cluster_bounds.py",
        "source_hierarchy.py",
        "source_triangle_clusters.py",
        "source_raster_candidates.py",
        "source_triangle_materials.py",
        "native_world_inputs.py",
        "field_view_cost.py",
        "compare_source_hierarchy.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze measured sources before recording costs")
    create_output(args.output)
    m, _, _, source = load(DATA, attributes=True)
    h = prepare(source)
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
        "held_source_hierarchy_bytes": h.payload_bytes,
        "scope": "Python metadata/view-demand comparison, original source remains held. Seven new Empty results are an explicit contract change, not native/raster/World admission. Old hierarchy already avoids fine projection for those7Whole cases. Zero new view-demand triangles does not discard shadow/simulation needs or prove actual IO/UMA/frame savings.",
        "bound_queries": {},
        "turn": {},
    }
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as a:
        p = a["placements/words"]
        raster = m["raster"]
        for pose in ("near-direction", "revealing-side"):
            for guard in (0.0, 1 / 256):
                v = a[pose + "/view_words"]
                expected = old_query(h, v, p, raster, raster_guard_px=guard)
                rows = {}
                for name, method in (("old", old_query), ("clip", query)):
                    result, cost = measured(
                        lambda method=method, v=v, guard=guard: method(
                            h, v, p, raster, raster_guard_px=guard
                        ),
                        args.output,
                        pose + "-" + str(guard) + "-" + name,
                    )
                    equivalent(result, expected)
                    rows[name] = {
                        "cost": cost,
                        "selected": len(result[0]),
                        "work": result[1],
                    }
                report["bound_queries"][pose + "-" + str(guard)] = rows
        base = a["near-direction/view_words"]
        angles = list(range(0, 360, 30))
        views = [changed_frame(base, angle, np.zeros(3)) for angle in angles]
        expected = [old_query(h, v, p, raster, raster_guard_px=0.0) for v in views]

        def run(method):
            rows = []
            for angle, v, before in zip(angles, views, expected, strict=True):
                result = method(h, v, p, raster, raster_guard_px=0.0)
                if method is query and angle in (90, 120, 150, 180, 210, 240, 270):
                    if (
                        len(result[0])
                        or result[1]["reason"] != "source_outside_xy_clip"
                    ):
                        raise ValueError("Expected strict whole-source clip exclusion")
                else:
                    equivalent(result, before)
                rows.append(
                    {
                        "angle": angle,
                        "selected": len(result[0]),
                        "output_id_bytes": result[0].nbytes,
                        "work": result[1],
                    }
                )
            return rows

        for name, method in (("old", old_query), ("clip", query)):
            rows, cost = measured(
                lambda method=method: run(method), args.output, "turn-" + name
            )
            report["turn"][name] = {
                "cost": cost,
                "rows": rows,
                "output_id_bytes": sum(r["output_id_bytes"] for r in rows),
                "source_view_triangles_requested": sum(r["selected"] for r in rows),
                "whole_fallbacks": sum(not r["work"]["complete_model"] for r in rows),
            }
        export_controls(
            source,
            h,
            a,
            raster,
            args.output / "native-controls",
            m["world_cameras"]["near-direction"],
        )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": report["commit"],
                "report": str(args.output / "report.json"),
                "outside_empty_cases": 7,
            }
        )
    )


if __name__ == "__main__":
    main()
