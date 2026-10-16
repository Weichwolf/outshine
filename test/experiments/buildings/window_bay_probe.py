"""Actual source bay programmes: plan/sections/2.5D, before/near/whole/opposite/compact."""

import argparse
import json
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from demand import View
from evidence import revision
from facade_entry_geometry import basis_for
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from recess_probe import building
from verify_window_bays import camera, inspect, prepared, sources
from window_bay_cost import benchmark
from window_bay_geometry import products


def diagram(current, owner, output, name):
    parts = products(current, owner, 1, "through")
    proof = inspect(current, parts)
    origin = np.r_[current["centre"], current["base_m"]]
    basis = basis_for(current["frame"])
    triangles = np.concatenate([p["vertices"][p["faces"]] for p in parts])
    local = (triangles - origin) @ basis.T
    materials = np.concatenate([p["materials"] for p in parts])
    mesh = {
        "vertices": local.reshape(-1, 3),
        "faces": np.arange(local.size // 3).reshape(-1, 3),
        "materials": materials,
    }
    figure = plt.figure(figsize=(28, 9))
    for index, normal, offset, axes, title in (
        (141, [0, 1, 0], 1.6, (0, 2), "Grundriss · Schnitt durch Fenster"),
        (142, [1, 0, 0], 0, (2, 1), "Mittelschnitt · Wandöffnung/Boden"),
        (143, [0, 0, 1], 0.45, (0, 1), "Frontschnitt · echte Fenster-/Rahmenkörper"),
    ):
        axis = figure.add_subplot(index)
        for line, material in slice_triangles(mesh, normal, offset):
            axis.plot(
                line[:, axes[0]],
                line[:, axes[1]],
                color="steelblue" if material == 3 else "black",
                linewidth=1.5,
            )
        axis.set(aspect="equal", title=title, xlabel="Meter", ylabel="Meter")
    axis = figure.add_subplot(144, projection="3d")
    colour = np.array(
        [
            (0.12, 0.27, 0.34, 0.22) if m == 3 else (0.7, 0.71, 0.67, 1)
            for m in materials
        ]
    )
    counts = np.cumsum([0, *(len(p["faces"]) for p in parts)])
    keep = np.ones(len(local), dtype=bool)
    for index in (5, len(parts) - 1):
        keep[counts[index] : counts[index + 1]] = False
    spatial = local[:, :, [0, 2, 1]]
    axis.add_collection3d(
        Poly3DCollection(spatial[keep], facecolors=colour[keep], edgecolors="none")
    )
    low, high = spatial.min(axis=(0, 1)), spatial.max(axis=(0, 1))
    axis.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        zlim=(low[2], high[2]),
        title="2,5D · nur Diagnose: Decke ausgeblendet, Glas transparent",
    )
    axis.set_box_aspect(high - low)
    axis.view_init(elev=25, azim=65)
    figure.tight_layout()
    figure.savefig(output / f"{name}-plan-sections.png", dpi=120)
    plt.close(figure)
    return proof


def whole_view(row, plan):
    centre = np.asarray(row["polygon"].centroid.coords[0])
    height = row["properties"]["render_height"]
    target = np.r_[centre, height * 0.45]
    bounds = row["polygon"].bounds
    distance = max(
        height * 1.8, np.linalg.norm(np.array(bounds[2:]) - bounds[:2]) * 1.4
    )
    basis = basis_for(plan["frame"])
    eye = target + (basis[2] + basis[0] * 0.3) * distance + [0, 0, height * 0.65]
    return View(eye, target, width=1920, height=1080, fov_deg=48)


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze bay source before visual comparisons")
    output.mkdir(parents=True, exist_ok=True)
    cases, arrivals = [], {}
    for index, source in enumerate(sources()):
        row = source | {
            "placement_base_enu_m": 0.0,
            "ground_asl_m": None,
            "placement_origin": "flat no DEM",
        }
        _, _, _, _, owner, plan = prepared(row)
        current = plan["cells"][0]
        name = "Rect" if index == 0 else "Chamfer"
        arrivals[name] = diagram(current, owner, output, name)
        near, whole = camera(current, 10), whole_view(row, plan)
        with patch("facade_window_bays.WindowBayDetails.plan", return_value=None):
            cases.append(
                building(
                    row,
                    near,
                    name + "Before",
                    output,
                    proof,
                    scope="Same source/camera before the new bay programme; prior balcony programme retained",
                )
            )
        for suffix, view in (("Near", near), ("Whole", whole)):
            cases.append(
                building(
                    row,
                    view,
                    name + suffix,
                    output,
                    proof,
                    scope="Actual current source-aligned window bay programme; simple local arrival, no full room/LOD/night/native admission",
                )
            )
        if index == 0:
            for suffix, view in (
                ("Compact", camera(current, 500)),
                (
                    "Opposite",
                    replace(
                        whole,
                        eye=whole.target - (whole.eye - whole.target) * [1, 1, -1],
                    ),
                ),
            ):
                cases.append(
                    building(
                        row,
                        view,
                        name + suffix,
                        output,
                        proof,
                        scope="Actual current source-aligned window bay programme; compact skin is render-only, no light/shadow/native admission",
                    )
                )
    (output / "window-bay-comparison.json").write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "arrival": arrivals,
                "cost": benchmark(),
                "scope": "Two actual provider sources/shape programmes; research images only",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
