"""Static source bounds versus full projection, with matched IDs and fallback."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
from experiment_output import create_output
from field_view_cost import measured
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from source_hierarchy import prepare, query
from source_raster_candidates import projection, select
from source_triangle_clusters import projected_control


def changed_frame(base, angle, delta):
    """Synthetic word-model inputs, never claimed as native upload captures."""
    radians = np.deg2rad(angle)
    c, s = np.cos(radians), np.sin(radians)
    rotate = np.array([[c, 0, s, 0], [0, 1, 0, 0], [-s, 0, c, 0], [0, 0, 0, 1]])
    matrix = base[:16].view("<f4").reshape(4, 4, order="F").astype(float)
    words = base.copy()
    v = words.view("<f4")
    v[:16] = (matrix @ rotate).astype("<f4").ravel(order="F")
    v[48:51] = (base.view("<f4")[48:51].astype(float) - delta).astype("<f4")
    return words


def equivalent(actual, expected):
    np.testing.assert_array_equal(actual[0], expected[0])
    for key in ("complete_model", "reason", "original_group_retained"):
        if actual[1][key] != expected[1][key]:
            raise ValueError("Original candidate/fallback contract changed: " + key)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    paths = [
        Path(__file__).with_name(name)
        for name in (
            "compare_source_hierarchy.py",
            "source_hierarchy.py",
            "source_cluster_bounds.py",
            "source_triangle_clusters.py",
            "source_raster_candidates.py",
            "field_view_cost.py",
            "native_world_inputs.py",
            "native_source_triangle_materials.py",
            "building_sample_inputs.py",
            "building_sample_originals.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze measured source before recording costs")
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
        "native_packet_sha256": binding["samples_sha256"],
        "platform": platform.platform(),
        "numpy": np.__version__,
        "load_average": list(os.getloadavg()),
        "build": {},
        "queries": {},
        "motion": {},
        "scope": "All source data already loaded and held. Scoped Python allocation/timing comparison, not IO/cache/native/UMA/frame/p99 acceptance. Logical 4096-byte array pages are not OS/cache/disk events. Synthetic motion uses explicit model words, not captured native poses. Existing native IDs score only.",
    }
    prepared = {}
    for name, limit, order in (
        ("source4", 4, "source"),
        ("source8", 8, "source"),
        ("morton128", 128, "morton"),
    ):
        h, cost = measured(
            lambda limit=limit, order=order: prepare(
                source, leaf_triangles=limit, order=order
            ),
            args.output,
            "build-" + name,
        )
        report["build"][name] = {
            "cost": cost,
            "payload_bytes": h.payload_bytes,
            "metadata_bytes": h.groups.payload_bytes
            + h.bounds.nbytes
            + h.witnesses.nbytes,
            "groups": len(h.groups.bounds),
        }
        prepared[name] = h
    geometry = prepared["source4"].source
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as archive:
        placement = archive["placements/words"]
        for pose in ("near-direction", "revealing-side"):
            view = archive[pose + "/view_words"]
            for guard in (0.0, 1 / 256):
                case = pose + "-guard-" + str(guard)
                expected, cost = measured(
                    lambda view=view, guard=guard: select(
                        geometry,
                        view,
                        placement,
                        manifest["raster"],
                        raster_guard_px=guard,
                    ),
                    args.output,
                    case + "-flat",
                )
                rows = {
                    "flat": {
                        "cost": cost,
                        "work": expected[1],
                        "selected": len(expected[0]),
                    }
                }
                xy, radius = projection(
                    geometry["positions"],
                    view,
                    placement,
                    np.asarray(manifest["raster"]),
                )
                for name, h in prepared.items():
                    result, cost = measured(
                        lambda h=h, view=view, guard=guard: query(
                            h,
                            view,
                            placement,
                            manifest["raster"],
                            raster_guard_px=guard,
                        ),
                        args.output,
                        case + "-" + name,
                    )
                    equivalent(result, expected)
                    favorable, kept = projected_control(
                        geometry, h.groups, xy, radius + guard, manifest["raster"]
                    )
                    rows[name] = {
                        "cost": cost,
                        "work": result[1],
                        "selected": len(result[0]),
                        "favorable_fine_dependent_control": {
                            "groups_kept": int(kept.sum()),
                            "triangle_rows": len(favorable),
                            "vertex_rows": len(np.unique(source["indices"][favorable])),
                        },
                    }
                report["queries"][case] = rows
        base = archive["near-direction/view_words"]
        for mode, delta in (
            ("turn", np.zeros(3)),
            ("turn-and-translate", np.array([100, 25, -200])),
            ("translation", np.array([100, 25, -200])),
        ):
            views = [
                changed_frame(
                    base,
                    0 if mode == "translation" else angle,
                    index * delta if mode == "translation" else delta,
                )
                for index, angle in enumerate(range(0, 360, 30))
            ]

            def run_flat(views=views):
                return [
                    select(
                        geometry, v, placement, manifest["raster"], raster_guard_px=0.0
                    )
                    for v in views
                ]

            expected, cost = measured(run_flat, args.output, mode + "-flat")
            report["motion"][mode] = {
                "flat": {
                    "cost": cost,
                    "frames": len(views),
                    "whole_fallbacks": sum(
                        not x[1]["complete_model"] for x in expected
                    ),
                }
            }
            for name in ("source4", "source8", "morton128"):
                h = prepared[name]
                result, cost = measured(
                    lambda h=h, views=views: [
                        query(h, v, placement, manifest["raster"], raster_guard_px=0.0)
                        for v in views
                    ],
                    args.output,
                    mode + "-" + name,
                )
                for actual, original in zip(result, expected, strict=True):
                    equivalent(actual, original)
                report["motion"][mode][name] = {
                    "cost": cost,
                    "frames": len(views),
                    "whole_fallbacks": sum(not x[1]["complete_model"] for x in result),
                    "fine_vertex_rows_touched": sum(
                        x[1]["hierarchy"]["fine_vertex_rows_touched"] for x in result
                    ),
                    "work": [x[1] for x in result],
                }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": report["commit"],
                "cases": len(report["queries"]),
                "motion_sequences": len(report["motion"]),
                "all_candidate_and_fallback_comparisons_equal": True,
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
