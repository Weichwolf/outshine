"""Actual hosted-use fronts: mesh plans/sections, fair retained-wall cost and Blender inputs."""

import argparse
import gc
import json
import resource
import time
import tracemalloc
from dataclasses import replace
from functools import partial
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_mesh import slice_triangles
from evidence import revision
from facade_entry_clip import remainder
from facade_entry_geometry import basis_for
from facade_shopfronts import ShopfrontDetails
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from recess_probe import building
from showcase_tiles import view_for
from tile_facade_recess import selection
from tile_shopfronts import retained_flat, shop_cells
from verify_shopfronts import camera, planned, sources


def projected_front(mesh, frame):
    local = (mesh["vertices"][mesh["faces"]] - np.r_[frame["a"], 0.0]) @ basis_for(
        frame
    ).T
    front = abs(local[:, :, 2]).max(axis=1) < 1e-7
    polygons = [shapely.Polygon(t[:, :2]) for t in local[front]]
    return shapely.union_all(polygons), sum(p.area for p in polygons)


def benchmark():
    records = []
    for row in sources():
        mesh, _, p, entry, plan, _ = planned(row)
        frame = plan["frame"]
        triangles = mesh["vertices"][mesh["faces"]]
        normals = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        normals /= np.linalg.norm(normals, axis=1, keepdims=True)
        selected, _, rectangular = selection(triangles, normals, frame, p["minHeight"])
        if not rectangular:
            raise ValueError(
                "Fair rectangular retained-wall benchmark requires an actual rectangular frame"
            )
        columns = tuple(c["column"] for c in plan["cells"])
        variants = {
            "rectangles": partial(retained_flat, frame, entry, columns, 32768),
            "source-triangle-difference": partial(
                clipped_wall, mesh, selected, entry, columns
            ),
        }
        oracle, _ = projected_front(variants["source-triangle-difference"](), frame)
        for name, generate in variants.items():
            result = generate()
            actual, area = projected_front(result, frame)
            difference = actual.symmetric_difference(oracle).area
            if difference > 1e-7 or abs(area - actual.area) > 1e-7:
                raise ValueError("Retained walls differ or overlap")
            samples = []
            for _ in range(21):
                start = time.perf_counter()
                result = generate()
                samples.append((time.perf_counter() - start) * 1000)
            gc.collect()
            tracemalloc.start()
            result = generate()
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            records.append(
                {
                    "source_id": row["id"],
                    "method": name,
                    "median_ms": float(np.median(samples)),
                    "runs_ms": samples,
                    "triangles": len(result["faces"]),
                    "array_bytes": sum(v.nbytes for v in result.values()),
                    "traced_peak_bytes": peak,
                    "surface_difference_m2": difference,
                    "source_triangles": int(selected.sum()),
                    "cut_cells": len(columns) + 1,
                    "known_full_position_buffer_allocations": len(result["vertices"])
                    // 4
                    + 3
                    if name == "rectangles"
                    else None,
                    "known_full_position_materialized_bytes": len(result["vertices"])
                    * 3
                    * 8
                    * 4
                    if name == "rectangles"
                    else None,
                    "known_position_stack_copied_bytes": len(result["vertices"]) * 3 * 8
                    if name == "rectangles"
                    else None,
                    "work": "one quad per retained rectangular range"
                    if name == "rectangles"
                    else "per-source-triangle GEOS intersection/difference and constrained triangulation",
                    "backend_allocations_copies": None,
                }
            )
    return records


def clipped_wall(mesh, selected, entry, columns):
    return remainder(mesh, selected, entry, excluded_columns=columns)[0]


def diagram(row, plan, output):
    parts, _ = shop_cells(plan, "recess", lambda fractions: 32770, 1, lambda: 32769)
    frame, entry = plan["frame"], plan["entry"]
    basis, origin = basis_for(frame), np.r_[frame["a"], 0.0]
    vertices = np.concatenate([p["vertices"][p["faces"]] for p in parts])
    local = (vertices - origin) @ basis.T
    materials = np.concatenate([p["materials"] for p in parts])
    colours = np.array(
        [
            (0.08, 0.20, 0.24)
            if m == 32769
            else (0.25, 0.30, 0.31)
            if m == 10
            else (0.72, 0.73, 0.70)
            for m in materials
        ]
    )
    figure = plt.figure(figsize=(28, 8))
    ax = figure.add_subplot(141)
    xy = np.asarray(row["polygon"].exterior.coords)
    ax.plot(*xy.T, color="black")
    xy = np.asarray(entry["door"]["landing"].exterior.coords)
    ax.fill(*xy.T, color="steelblue", alpha=0.4)
    a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
    ax.plot(*np.array([a, b]).T, color="darkorange", linewidth=3)
    ax.set(
        aspect="equal",
        title="Echter Grundriss · gewählte Eingangsfront",
        xlabel="Ost [m]",
        ylabel="Nord [m]",
    )
    ax = figure.add_subplot(142)
    for t, colour in sorted(
        zip(local, colours, strict=True), key=lambda row: row[0][:, 2].mean()
    ):
        ax.fill(*t[:, :2].T, color=colour)
    ax.set(
        aspect="equal",
        title="Tatsächliche Schaufensterdreiecke · Vorderansicht",
        xlabel="Fassaden-U [m]",
        ylabel="Höhe [m]",
    )
    ax = figure.add_subplot(143)
    section_mesh = {
        "vertices": local.reshape(-1, 3),
        "faces": np.arange(local.size // 3).reshape(-1, 3),
        "materials": materials,
    }
    for line, material in slice_triangles(
        section_mesh, [0, 1, 0], entry["floor_m"] / 2
    ):
        ax.plot(line[:, 0], line[:, 2], color="teal" if material == 32769 else "black")
    ax.set(
        aspect="equal",
        title="Tatsächlicher Mittelschnitt",
        xlabel="Fassaden-U [m]",
        ylabel="Außennormale [m]",
    )
    ax = figure.add_subplot(144, projection="3d")
    ax.add_collection3d(Poly3DCollection(local, facecolors=colours, edgecolors="none"))
    ax.set(
        xlim=(local[:, :, 0].min(), local[:, :, 0].max()),
        ylim=(0, entry["floor_m"]),
        zlim=(-0.24, 0.01),
        title="2,5D · Laibungstiefe vergrößert",
    )
    ax.set_box_aspect((frame["width_m"], entry["floor_m"], 1.5))
    ax.view_init(elev=28, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "shopfront-plan-sections.png", dpi=120)
    plt.close(figure)


def generate(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze shopfront source before comparison")
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    for index, prefix in ((0, "Retail"), (1, "Food")):
        row = sources()[index]
        row.update(
            layer="building",
            placement_base_enu_m=0.0,
            ground_asl_m=None,
            placement_origin="flat no DEM",
        )
        _, _, _, _, plan, _ = planned(row)
        near = camera(plan, 10)
        with patch.object(ShopfrontDetails, "plan", return_value=None):
            cases.append(building(row, near, prefix + "Before", output, proof))
        cases.append(building(row, near, prefix + "After", output, proof))
        if index == 0:
            diagram(row, plan, output)
            cases.append(building(row, camera(plan, 500), "Compact", output, proof))
            overview = replace(view_for([row]), width=1920, height=1080)
            cases.append(building(row, overview, "Overview", output, proof))
    for record in cases:
        data = json.loads((output / (record["name"] + "-geometry.json")).read_text())
        record["planned"] = data["procedures"]["shopfronts"]
        record["emitted"] = data["procedures"]["shopfront_geometry"]
    (output / "shopfront-comparison.json").write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "timings": benchmark(),
                "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                "scope": "Actual local-use host geometry and bounded procedural ground fronts; backend allocations/copies unknown, no native budget or complete architecture family",
            },
            indent=2,
        )
        + "\n"
    )


def run(output):
    with patch("facade_balconies.BalconyDetails.plan", return_value=None):
        return generate(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
