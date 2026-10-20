"""Original neighboring cells consume complete common-owner border profiles."""

import argparse
import hashlib
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured, solve
from compare_water_grid_height import inputs, run_model
from water_cell_height import body_geometry, prepare, project
from water_edge_profile_audit import audit_profile
from water_edge_profiles import prepare_profile_owner, restrict_profile
from water_shared_borders import shared_borders


def endpoint_only_error(owner, polylines, profile):
    maximum = 0.0
    curves = []
    for line_index, line in enumerate(polylines):
        endpoints = owner.sampler.sample(line, max_queries=8192)
        if any(endpoints.reasons):
            raise ValueError("original source endpoint missing")
        for local in range(len(line) - 1):
            selected = [
                i
                for i, a in enumerate(profile.report["source_segment_addresses"])
                if tuple(a) == (line_index, local)
            ]
            ids = np.unique(profile.segments[selected])
            points = profile.points[ids]
            a, b = line[local : local + 2]
            direction = b - a
            t = ((points[:, :2] - a) @ direction) / (direction @ direction)
            up = endpoints.points_enu_m[local : local + 2, 2]
            error = points[:, 2] - (up[0] + t * (up[1] - up[0]))
            maximum = max(maximum, float(abs(error).max()))
            order = np.argsort(t)
            curves.append((float(np.linalg.norm(direction)), t[order], error[order]))
    return maximum, curves


def consume_saved_profile(path, owner, edge, *, reverse=False):
    with np.load(path) as data:
        if data["context"].item() != owner.context:
            raise ValueError("consumer owner context mismatch")
        if tuple(data["edge"].tolist()) != tuple(edge):
            raise ValueError("consumer shared-edge identity mismatch")
        points, segments = data["points"].copy(), data["segments"].copy()
    return points, segments[::-1, ::-1] if reverse else segments


def lake_case(path, output):
    data, records, knots, origin = inputs(path, kind="lake")
    _data, _snapshots, indices = observed_snapshots(path)
    mesh, anchor, rows, bounds, _csr, _meta = prepare(records, data, indices)
    fit = solve(mesh, anchor, rows, bounds)
    _anchor, polygons = body_geometry(records)
    mask = shapely.union_all(polygons)
    digest = hashlib.sha256(
        ("lake-soft-native-source-v1:" + origin["input_sha256"]).encode()
    )
    for array in (
        mesh.vertices,
        mesh.triangles,
        anchor,
        rows,
        np.asarray(bounds),
        fit.mesh.vertices,
    ):
        digest.update(np.ascontiguousarray(array).tobytes())
    owner = prepare_profile_owner(
        fit.mesh,
        anchor=anchor,
        mask=mask,
        owner_version=digest.hexdigest(),
    )
    borders, status = shared_borders(
        records, knots, known_cells=[tuple(s["tile"]) for s in data["sources"]]
    )
    reports = []
    for index, border in enumerate(borders):
        polylines = [
            project(p, border["edge"][0], anchor)[:, :2] for p in border["polylines"]
        ]
        profile = restrict_profile(owner, polylines)
        reports.append(
            {
                "edge": border["edge"],
                "profile": profile.report,
                "audit": audit_profile(fit.mesh, polylines, profile),
            }
        )
        np.savez_compressed(
            output / f"lake-profile-{index}.npz",
            points=profile.points,
            segments=profile.segments,
            parents=profile.parent_faces,
            context=np.asarray(profile.context),
            edge=np.asarray(border["edge"]),
        )
    return {
        "source": origin,
        "fit": dict(fit.report),
        "border_status": status,
        "profiles": reports,
    }


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    (data, records, knots, origin), source_cost = measured(lambda: inputs(path))
    (borders, border_work), border_cost = measured(
        lambda: shared_borders(
            records, knots, known_cells=[tuple(s["tile"]) for s in data["sources"]]
        )
    )
    if not borders:
        raise ValueError("no real shared water border")
    summaries, plots = {}, {}
    for spacing in (1024, 256, 64):
        prepared, fitted, model_report = run_model(
            spacing, data, records, knots, origin, output
        )
        owner, owner_cost = measured(
            lambda fitted=fitted, prepared=prepared, model_report=model_report: (
                prepare_profile_owner(
                    fitted.mesh,
                    anchor=prepared.anchor,
                    mask=prepared.mask,
                    owner_version=model_report["version"],
                )
            )
        )
        if owner.context != model_report["contacts"]["owner_context"]:
            raise ValueError("whole-profile and existing vertex-product owners differ")
        edges = []
        for index, border in enumerate(borders):
            polylines = [
                project(p, border["edge"][0], prepared.anchor)[:, :2]
                for p in border["polylines"]
            ]
            profile, cost = measured(
                lambda polylines=polylines, owner=owner: restrict_profile(
                    owner, polylines
                )
            )
            audit, audit_cost = measured(
                lambda polylines=polylines, profile=profile, fitted=fitted: (
                    audit_profile(fitted.mesh, polylines, profile)
                )
            )
            file = output / f"profile-{spacing}-{index}.npz"
            np.savez_compressed(
                file,
                points=profile.points,
                segments=profile.segments,
                parents=profile.parent_faces,
                context=np.asarray(profile.context),
                edge=np.asarray(border["edge"]),
                cells=np.asarray(border["cells"]),
                original_source_keys=np.asarray(json.dumps(border["source_keys"])),
            )
            left, left_edges = consume_saved_profile(file, owner, border["edge"])
            right, right_edges = consume_saved_profile(
                file, owner, border["edge"], reverse=True
            )
            np.testing.assert_array_equal(
                left[left_edges], right[right_edges[::-1, ::-1]]
            )
            error, curves = endpoint_only_error(owner, polylines, profile)
            plots[(spacing, index)] = curves
            edges.append(
                {
                    "edge": border["edge"],
                    "cells": border["cells"],
                    "source_keys": border["source_keys"],
                    "profile_context": profile.context,
                    "profile": profile.report,
                    "profile_cost": cost,
                    "all_pairs_audit": audit,
                    "audit_cost": audit_cost,
                    "old_vertex_only_max_difference_m": error,
                    "independent_consumer_profile_gap_m": 0.0,
                }
            )
        summaries[spacing] = {
            "owner_prepare_cost": owner_cost,
            "edges": edges,
            "vertices": sum(e["profile"]["profile_vertices"] for e in edges),
            "segments": sum(e["profile"]["profile_segments"] for e in edges),
            "numeric_payload_bytes": sum(
                e["profile"]["numeric_payload_bytes"] for e in edges
            ),
        }
    # Plot the actual edge most affected by the missing root crossings.
    worst = max(
        range(len(borders)),
        key=lambda i: summaries[64]["edges"][i]["old_vertex_only_max_difference_m"],
    )
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    fig, axes = plt.subplots(1, 3, figsize=(14, 4), layout="constrained")
    for axis, spacing in zip(axes, (1024, 256, 64), strict=True):
        offset = 0.0
        for length, t, error in plots[(spacing, worst)]:
            axis.plot(offset + t * length, error * 1000, "o-")
            offset += length
        axis.set(
            xlabel="Along original shared border (m)",
            ylabel="Correction to vertex-only interpolation (mm)",
            title=f"{spacing} m functional model",
        )
        axis.axhline(0, color="0.7", linewidth=0.5)
    fig.suptitle("Actual shared edge " + str(borders[worst]["edge"]))
    fig.savefig(output / "profiles.png", dpi=150)
    plt.close(fig)
    lake, lake_cost = measured(lambda: lake_case(path, output))
    result = {
        "origin": origin,
        "source_cost": source_cost,
        "border_work": border_work,
        "border_cost": border_cost,
        "models": summaries,
        "lake": lake,
        "lake_comparison_cost": lake_cost,
        "process_peak_rss_bytes_including_all_diagnostics": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "total_copied_bytes": None,
        "total_allocations": None,
        "scope": "complete declared common-owner border profiles, including all root facet changes; independent consumer copies use the same owner frame/version. Two unknown outer edges stay open; cross-cohort ownership, fine-job feasibility, bed and native/runtime/device integration remain open.",
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.fixture, args.output)
