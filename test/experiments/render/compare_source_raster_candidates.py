"""Bounded original-source candidates versus the fixed native raster opponent."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
import triangle_columns as columns
from compare_native_world_fields import capture_region, pixels
from experiment_output import create_output
from field_view_cost import measured
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import clip_from_original, load
from source_raster_candidates import projection, select
from source_triangle_materials import triangle_parts

GUARDS = (0.0, 1 / 256, 1 / 16, 0.5)


def payload(source, ids):
    if not len(ids):
        return {"vertices": 0, "index_bytes": 0, "array_bytes": 0}
    parts = triangle_parts(source, ids, referenced_vertices=True)
    return {
        "vertices": sum(p[0]["vertices"] for p in parts),
        "index_bytes": len(ids) * 12,
        "array_bytes": sum(a.nbytes for _, values in parts for a in values.values()),
    }


def score(ids, reference):
    reference = np.unique(reference)
    return {
        "selected": len(ids),
        "native_reference": len(reference),
        "missing_native_ordinals": np.setdiff1d(reference, ids).tolist(),
        "extra_ordinals": len(np.setdiff1d(ids, reference)),
    }


def diagnostic(
    output,
    pose,
    source,
    view,
    placement,
    raster,
    reference_xy,
    reference_id,
    first,
    outer,
):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import LineCollection

    center, _ = projection(source["positions"], view, placement, np.asarray(raster))
    center[:, 1] = raster[1] - center[:, 1]
    figure, axes = plt.subplots(1, 2, figsize=(10, 5))
    for ax, ids, name in zip(
        axes,
        (first, outer),
        ("CPU first hit", "Outer support, guard=0 (conditional)"),
        strict=True,
    ):
        vertices = center[source["indices"][ids]]
        lines = np.concatenate([vertices, vertices[:, :1]], axis=1)
        ax.add_collection(LineCollection(lines, colors="0.6", linewidths=0.35))
        kept = np.isin(reference_id, ids)
        for mask, color in ((kept, "tab:green"), (~kept, "tab:red")):
            ax.scatter(*(reference_xy[mask].astype(float) + 0.5).T, s=10, color=color)
        ax.set_title(
            f"{name}\n{len(ids)} triangles, {int((~kept).sum())} reference owners absent"
        )
        ax.set_xlim(reference_xy[:, 0].min() - 2, reference_xy[:, 0].max() + 3)
        ax.set_ylim(reference_xy[:, 1].max() + 3, reference_xy[:, 1].min() - 2)
        ax.set_aspect("equal")
    figure.suptitle(
        pose
        + ": original projected edges and native sample owners (not a native render)"
    )
    figure.tight_layout()
    figure.savefig(output / (pose + "-ordinals.png"), dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    names = (
        "compare_source_raster_candidates.py",
        "source_raster_candidates.py",
        "source_triangle_materials.py",
        "native_source_triangle_materials.py",
        "native_world_inputs.py",
        "triangle_columns.py",
        "triangle_query_support.py",
        "triangle_pair_batches.py",
        "compare_native_world_fields.py",
        "field_view_cost.py",
        "building_sample_inputs.py",
        "building_sample_originals.py",
        "field_depth_reprojection.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze all measured source before recording costs")
    create_output(args.output)
    loaded, load_cost = measured(
        lambda: load(DATA, attributes=True), args.output, "load-fixed-source"
    )
    manifest, _, cases, source = loaded
    _, binding = bound_selection(DATA, source)
    capture = clip_from_original(cases["capture", "near-direction"])
    field, prepare_cost = measured(
        lambda: columns.prepare(source, capture, 64), args.output, "prepare-columns"
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "native_packet_sha256": binding["samples_sha256"],
        "platform": platform.platform(),
        "processor": platform.processor(),
        "logical_cpus": os.cpu_count(),
        "load_average": list(os.getloadavg()),
        "numpy": np.__version__,
        "load_fixed_fixture": load_cost,
        "prepare_columns": prepare_cost,
        "columns_payload_bytes": field.payload_bytes,
        "original": payload(source, np.arange(len(source["indices"]))),
        "original_triangles": len(source["indices"]),
        "original_vertices": len(source["positions"]),
        "poses": {},
        "cohorts": {},
        "scope": "Local loaded-geometry Python comparison, no GPU IDs in demand, no native render or raster qualification. Explicit raster guards are hypotheses. No IO/residency/UMA/frame/p99 acceptance. Each envelope query repeats vertex projection/validation; columns preparation is separate. Numeric output bytes are not measured physical copies; Memray requests are not copied bytes.",
    }
    cohorts = {str(g): [] for g in GUARDS}
    all_references = []
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as archive:
        placement = archive["placements/words"]
        for pose in ("near-direction", "revealing-side"):
            view = archive[pose + "/view_words"]
            world = clip_from_original(cases["original", pose])
            points = pixels(capture_region(field.capture, world, manifest["raster"]))
            first, first_cost = measured(
                lambda world=world, points=points: columns.query(
                    field, world, points, manifest["raster"]
                ),
                args.output,
                pose + "-first-hit",
            )
            if not first[2].all():
                raise ValueError("Incomplete geometric control")
            first_ids = np.unique(first[1][first[1] >= 0])
            reference = archive[pose + "/triangle_ordinal"]
            all_references.extend(reference.tolist())
            row = {
                "first_hit": {
                    **score(first_ids, reference),
                    "cost": first_cost,
                    "work": first[3],
                    "source_payload": payload(source, first_ids),
                },
                "envelopes": {},
            }
            for guard in GUARDS:
                (ids, work), cost = measured(
                    lambda view=view, guard=guard: select(
                        source,
                        view,
                        placement,
                        manifest["raster"],
                        raster_guard_px=guard,
                    ),
                    args.output,
                    pose + "-guard-" + str(guard),
                )
                row["envelopes"][str(guard)] = {
                    **score(ids, reference),
                    "cost": cost,
                    "work": work,
                    "source_payload": payload(source, ids),
                }
                cohorts[str(guard)].extend(ids.tolist())
                np.save(
                    args.output / (pose + "-guard-" + str(guard) + "-ordinals.npy"),
                    ids,
                    allow_pickle=False,
                )
                if guard == 0:
                    diagnostic(
                        args.output,
                        pose,
                        source,
                        view,
                        placement,
                        manifest["raster"],
                        archive[pose + "/pixel_xy"],
                        reference,
                        first_ids,
                        ids,
                    )
            report["poses"][pose] = row
    for guard, cohort in cohorts.items():
        ids = np.unique(cohort)
        report["cohorts"][guard] = {
            **score(ids, all_references),
            "source_payload": payload(source, ids),
        }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": report["commit"], "cohorts": report["cohorts"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
