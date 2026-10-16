"""Existing source building: flat/near/opposite/far recess products and equal-quality construction costs."""

import argparse
import gc
import hashlib
import json
import resource
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from evidence import revision
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface
from showcase_tiles import view_for
from tile_buildings import construct
from tile_facades import FacadeDetails
from verify_urban_roofs import sources


def benchmark():
    results = []
    for columns, rows in ((4, 3), (12, 10), (40, 30)):
        plan = RecessPlan(
            FacadeRepeat(columns * 3.2, rows * 2.9, columns, rows, 0.48, 0.53)
        )
        expected = recess_surface(plan, method="scalar")
        for method in ("scalar", "batch"):
            values = []
            for _ in range(21):
                start = time.perf_counter()
                result = recess_surface(plan, method=method)
                values.append((time.perf_counter() - start) * 1000)
            for key in expected:
                np.testing.assert_array_equal(expected[key], result[key])
            del result
            gc.collect()
            tracemalloc.start()
            result = recess_surface(plan, method=method)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            results.append(
                {
                    "cells": plan.count(),
                    "method": method,
                    "median_ms": float(np.median(values)),
                    "runs_ms": values,
                    "triangles": len(result["faces"]),
                    "vertices": len(result["vertices"]),
                    "array_bytes": sum(v.nbytes for v in result.values()),
                    "tracemalloc_peak_bytes": peak,
                    "position_array_allocations": 1
                    if method == "batch"
                    else plan.count() + 1,
                    "position_array_materialized_bytes": result["vertices"].nbytes
                    * (1 if method == "batch" else 2),
                }
            )
    return results


def diagram(output):
    plan = RecessPlan(FacadeRepeat(6.4, 2.9, 2, 1, 0.48, 0.53))
    mesh = recess_surface(plan)
    figure = plt.figure(figsize=(24, 8))
    colours = np.array([[0.74, 0.72, 0.67], [0.12, 0.23, 0.30], [0.22, 0.26, 0.29]])
    points = mesh["vertices"][mesh["faces"]]
    front = figure.add_subplot(131)
    front.tripcolor(
        mesh["vertices"][:, 0],
        mesh["vertices"][:, 1],
        mesh["faces"],
        facecolors=mesh["roles"],
        cmap="cividis",
        shading="flat",
    )
    front.set(
        aspect="equal",
        xlabel="Fassaden-U [m]",
        ylabel="Höhe [m]",
        title="Dieselben Fensterachsen · tatsächliche Oberfläche",
    )
    section = figure.add_subplot(132)
    for line, role in slice_triangles(
        {
            "vertices": mesh["vertices"],
            "faces": mesh["faces"],
            "materials": mesh["roles"],
        },
        [0, 1, 0],
        1.45,
    ):
        section.plot(line[:, 0], line[:, 2], color=colours[role], linewidth=2)
    section.set(
        xlabel="Fassaden-U [m]",
        ylabel="Außennormale [m]",
        title="Tatsächlicher Schnitt · Glas bei −0,18 m",
    )
    spatial = figure.add_subplot(133, projection="3d")
    spatial.add_collection3d(
        Poly3DCollection(points, facecolors=colours[mesh["roles"]], edgecolors="none")
    )
    spatial.set(
        xlim=(0, 6.4),
        ylim=(0, 2.9),
        zlim=(-0.3, 0.05),
        title="2,5D-Diagnose · Tiefenachse vergrößert",
    )
    spatial.set_box_aspect((6.4, 2.9, 1.4))
    spatial.view_init(elev=28, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "recess-plan-sections.png", dpi=120)
    plt.close(figure)


def building(
    row,
    view,
    name,
    output,
    provenance,
    *,
    scope=None,
    door_states=None,
    context_rows=(),
):
    details = FacadeDetails([row, *context_rows], view, door_states=door_states)
    mesh, source = construct(
        name,
        prepared=([row], {"invalid": [], "span_m": 100.0}),
        ordinary_builder=details,
    )
    if source["generated"] != 1 or source["omissions"]:
        raise ValueError("Actual source building was not generated completely")
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    record = provenance | {
        "city": name,
        "ground_mode": "flat-no-dem",
        "ground_span_m": 10000.0,
        "source": source,
        "source_id": row["id"],
        "context_source_sha256": row["context"]["source_sha256"],
        "camera": {
            "projection": "perspective",
            "eye": view.eye.tolist(),
            "target": view.target.tolist(),
            "vertical_fov_deg": view.fov_deg,
            "clip_near_m": 0.1,
            "clip_far_m": 10000.0,
            "profile": [view.width, view.height],
        },
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "triangles": len(mesh["faces"]),
        "facade_materials": details.bindings,
        "procedures": details.report(),
        "scope": scope
        or "Same actual source and facade repeat; inward render recesses use existing physical frame component10, whose response differs from the prior flat shader frame. No new room/door programme or native/device/LOD admission.",
    }
    (output / (name + "-geometry.json")).write_text(json.dumps(record, indent=2) + "\n")
    return {
        "name": name,
        "triangles": len(mesh["faces"]),
        "recess_cells": sum(r["cells"] for r in details.recesses),
        "rejected": details.recess_rejected,
    }


def run(output):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Freeze source before generating recess comparisons")
    output.mkdir(parents=True, exist_ok=True)
    diagram(output)
    timings = benchmark()
    row = sources()[1] | {
        "layer": "building",
        "placement_base_enu_m": 0.0,
        "ground_asl_m": None,
        "placement_origin": "flat no DEM",
    }
    near = replace(view_for([row]), width=1920, height=1080)
    opposite = replace(near, eye=near.target + (near.eye - near.target) * [-1, -1, 1])
    far = replace(near, eye=near.target + (near.eye - near.target) * 30)
    with (
        patch("facade_entrances.EntranceDetails.plan", return_value=None),
        patch(
            "tile_facade_bands.BandDetails.__call__",
            side_effect=lambda mesh, *args, **kwargs: mesh,
        ),
    ):
        with patch(
            "tile_facade_recess.attach",
            side_effect=lambda mesh, *args, **kwargs: (mesh, [], []),
        ):
            cases = [building(row, near, "Flat", output, provenance)]
        for name, view in (("Near", near), ("Opposite", opposite), ("Far", far)):
            cases.append(building(row, view, name, output, provenance))
    if (
        not cases[1]["recess_cells"]
        or not cases[2]["recess_cells"]
        or cases[3]["recess_cells"]
    ):
        raise ValueError("Actual near/opposite/far detail demand disagrees")
    (output / "recess-comparison.json").write_text(
        json.dumps(
            provenance
            | {
                "cases": cases,
                "timings": timings,
                "process_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Equal arrays for scalar/batch; position allocation counters cover those arrays only, tracemalloc misses some native allocations. RSS includes sources/plots/verification. Local host, no native frame/device admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
