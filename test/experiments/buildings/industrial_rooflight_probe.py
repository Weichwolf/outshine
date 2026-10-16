"""Actual before/rooflit hall inputs, geometric sections and equal-volume frame costs."""

import argparse
import gc
import json
import time
import tracemalloc
from contextlib import contextmanager
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_mesh import slice_triangles
from demand import View
from evidence import revision
from industrial_hall_details import products as previous_products
from industrial_hall_plan import plan as previous_plan
from industrial_hall_shell import structure as previous_structure
from industrial_roof_choice import section_for
from industrial_rooflights import components, cuts, plan, templates
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from recess_probe import building
from stepped_source_probe import overview
from tile_roof_envelopes import choose
from verify_industrial_halls import sources
from verify_industrial_rooflights import generate


@contextmanager
def prototype():
    """Use the real builder once; explicit prototype overrides keep recipe v1 intact."""

    def planned(row, parameters, info, *args):
        value = previous_plan(row, parameters, info, *args)
        if value is None:
            return None
        fields = plan(value, section_for(row, choose(row, parameters)))
        return value | {"rooflights": json.loads(json.dumps(fields, default=dict))}

    def shell(value, envelope, material):
        return previous_structure(
            value, envelope, material, extra_cuts=cuts(value["rooflights"], material)
        )

    def detailed(value, material, opened, view):
        prior, detail = previous_products(value, material, opened, view)
        return [*prior, *components(value["rooflights"])], detail

    with (
        patch("tile_industrial_halls.plan", side_effect=planned),
        patch("tile_industrial_halls.structure", side_effect=shell),
        patch("tile_industrial_halls.products", side_effect=detailed),
    ):
        yield


def diagram(mesh, shell, fields, row, output, index):
    origin = np.asarray(row["polygon"].centroid.coords[0])
    figure = plt.figure(figsize=(24, 13.5))
    ax = figure.add_subplot(131)
    ax.plot(*(np.asarray(row["polygon"].exterior.coords) - origin).T, color="#777777")
    for item in fields:
        region = shapely.from_wkb(item["footprint_wkb"])
        ax.fill(*(np.asarray(region.exterior.coords) - origin).T, color="#267d9b")
    ax.set(
        aspect="equal",
        xlabel="East [m]",
        ylabel="North [m]",
        title="Owned facet replacements · source footprint",
    )
    item = fields[len(fields) // 2]
    axes = np.asarray(item["axes"])
    a, low, b, high = item["bounds"]
    centre = np.array([(a + b) / 2, (low + high) / 2]) @ axes + item["origin_xy"]
    ax = figure.add_subplot(132)
    raw = dict(mesh, vertices=mesh["vertices"] - np.r_[centre, 0])
    for line, material in slice_triangles(raw, np.r_[axes[1], 0], 0):
        colours = {3: "#267d9b", 10: "#c08442"}
        ax.plot(
            line[:, :2] @ axes[0],
            line[:, 2],
            color=colours.get(int(material), "#777777"),
            lw=1,
        )
    ax.set(
        xlim=(-(b - a) * 1.5, (b - a) * 1.5),
        ylim=(item["ceiling_m"] - 0.3, item["roof_top_m"] + 0.1),
        xlabel="Across facet [m]",
        ylabel="Height [m]",
        title="Actual section · open shaft / frame / glass",
    )
    ax = figure.add_subplot(133, projection="3d")
    triangles = mesh["vertices"][mesh["faces"]].copy()
    selected = mesh["materials"] == 9
    selected |= mesh["materials"] == 3
    selected |= (mesh["materials"] == 10) & (
        triangles[:, :, 2].min(axis=1) > item["ceiling_m"]
    )
    triangles = triangles[selected]
    triangles[:, :, :2] -= origin
    colour = np.tile([0.64, 0.66, 0.68], (len(triangles), 1))
    colour[mesh["materials"][selected] == 3] = [0.10, 0.41, 0.57]
    colour[mesh["materials"][selected] == 10] = [0.31, 0.33, 0.34]
    ax.add_collection3d(
        Poly3DCollection(triangles, facecolors=colour, edgecolors="none")
    )
    bounds = mesh["vertices"].min(axis=0), mesh["vertices"].max(axis=0)
    ax.set(
        xlim=(bounds[0][0] - origin[0], bounds[1][0] - origin[0]),
        ylim=(bounds[0][1] - origin[1], bounds[1][1] - origin[1]),
        zlim=(0, bounds[1][2]),
        title="2.5D · actual exterior roof products",
    )
    ax.set_box_aspect(bounds[1] - bounds[0])
    ax.view_init(elev=45, azim=-70)
    figure.suptitle(row["id"] + " · inferred glazing, not measured OSM rooflights")
    figure.tight_layout()
    figure.savefig(output / f"rooflights-{index}-sections.png", dpi=100)
    plt.close(figure)


def storage_bytes(parts):
    seen = set()
    total = 0
    for part in parts:
        for array in part.values():
            while isinstance(array.base, np.ndarray):
                array = array.base
            if id(array) not in seen:
                total += array.nbytes
                seen.add(id(array))
    return total


def benchmark(fields):
    results = []
    for method in ("ring", "bars"):
        templates.cache_clear()
        start = time.perf_counter()
        first = components(fields, method=method)
        cold = (time.perf_counter() - start) * 1000
        values = []
        for _ in range(21):
            start = time.perf_counter()
            result = components(fields, method=method)
            values.append((time.perf_counter() - start) * 1000)
        gc.collect()
        tracemalloc.start()
        measured = components(fields, method=method)
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        results.append(
            {
                "method": method,
                "fields": len(fields),
                "cold_ms": cold,
                "warm_runs_ms": values,
                "warm_median_ms": float(np.median(values)),
                "triangles": sum(len(p["faces"]) for p in result),
                "transformed_components": len(result),
                "output_unique_array_bytes": storage_bytes(result),
                "transformed_position_normal_bytes": sum(
                    p[k].nbytes for p in result for k in ("vertices", "normals")
                ),
                "tracemalloc_peak_bytes": peak,
                "cached_templates": templates.cache_info().currsize,
                "limits": "Equal physical frame/glass volume; differing internal contact faces and UV segmentation. Native GEOS/Manifold allocations not completely traced; shared host load, no isolated speedup/device claim.",
            }
        )
        del first, measured
    return results


def visible_vertices(mesh, view):
    q = (mesh["vertices"] - view.eye) @ view.basis.T
    selected = (q[:, 2] > 0.1) & np.all(
        abs(q[:, :2]) * view.focal_px
        <= np.maximum(q[:, 2:3], 0.01) * [view.width / 2, view.height / 2],
        axis=1,
    )
    return int(selected.sum())


def inside_view(fields, hall, before, after):
    region = shapely.from_wkb(hall["interior_wkb"]).buffer(-0.30)
    for item in fields:
        target = np.array(shapely.from_wkb(item["footprint_wkb"]).centroid.coords[0])
        axes = np.asarray(item["axes"])
        for shift in (-1.2, 1.2):
            eye = target + axes[1] * shift
            if not region.covers(shapely.Point(eye)):
                continue
            view = View(
                np.r_[eye, 1.8],
                np.r_[target, item["ceiling_m"] + 0.8],
                width=1920,
                height=1080,
                fov_deg=80,
            )
            if min(visible_vertices(before, view), visible_vertices(after, view)) >= 4:
                return view
    raise ValueError("No admitted interior camera sees both actual source products")


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before preparing actual render inputs")
    output.mkdir(parents=True, exist_ok=True)
    costs = []
    for index, (row, neighbors) in enumerate(sources()[:3]):
        mesh, shell, _body, _added, fields, hall, before, _info = generate(
            row, neighbors
        )
        diagram(mesh, shell, fields, row, output, index)
        costs.append({"source_id": row["id"], "costs": benchmark(fields)})
        axes = np.asarray(fields[0]["axes"])
        whole = replace(
            overview(mesh["vertices"], np.r_[axes[0] * 0.9 + axes[1] * 0.3, 0.75]),
            width=1920,
            height=1080,
        )
        views = [("Whole", whole)]
        if index == 2:
            views.append(
                (
                    "Opposite",
                    replace(
                        whole,
                        eye=whole.target - (whole.eye - whole.target) * [1, 1, -1],
                    ),
                )
            )
            views.append(("Inside", inside_view(fields, hall, before, mesh)))
            item = fields[0]
            xy = np.array(shapely.from_wkb(item["footprint_wkb"]).centroid.coords[0])
            target = np.r_[xy, item["ceiling_m"] + 0.8]
            near = View(
                target + np.r_[8 * (axes[0] + axes[1]), 7],
                target,
                width=1920,
                height=1080,
                fov_deg=55,
            )
            if min(visible_vertices(before, near), visible_vertices(mesh, near)) < 4:
                raise ValueError(
                    "Near roof comparison lacks visible reference vertices"
                )
            views.append(("Near", near))
        for name, view in views:
            for rooflit in (False, True):
                label = f"Rooflight{index}{name}" + ("Glazed" if rooflit else "Before")
                if rooflit:
                    with prototype():
                        building(
                            row,
                            view,
                            label,
                            output,
                            proof,
                            context_rows=neighbors,
                            gate_states={hall["gate"]["id"]: 1},
                            door_states={hall["entry"]["id"]: 90},
                        )
                else:
                    building(
                        row,
                        view,
                        label,
                        output,
                        proof,
                        context_rows=neighbors,
                        gate_states={hall["gate"]["id"]: 1},
                        door_states={hall["entry"]["id"]: 90},
                    )
    (output / "costs.json").write_text(
        json.dumps(proof | {"cases": costs}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
