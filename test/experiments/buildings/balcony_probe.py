"""Actual residential before/closed/open/opposite/compact geometry and a supported local arrival."""

import argparse
import json
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from balcony_clearance import inspect
from balcony_cost import benchmark
from balcony_geometry import products
from building_mesh import slice_triangles
from demand import View
from evidence import revision
from facade_entry_geometry import basis_for
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from recess_probe import building
from verify_balcony_cells import camera, prepared
from verify_urban_roofs import sources


def diagram(current, owner, output):
    current["doorway"]["angle_deg"] = 90.0
    parts = products(current, owner, 1, "through")
    proof = inspect(current, parts)
    triangles = np.concatenate([p["vertices"][p["faces"]] for p in parts])
    materials = np.concatenate([p["materials"] for p in parts])
    basis = basis_for(current["doorway"]["frame"])
    origin = np.r_[current["doorway"]["door"]["centre"], current["floor_z_m"]]
    local = (triangles - origin) @ basis.T
    colours = np.array(
        [(0.15, 0.22, 0.25) if m in (7, 10) else (0.68, 0.70, 0.66) for m in materials]
    )
    figure = plt.figure(figsize=(28, 9))
    axis = figure.add_subplot(141)
    for polygon, colour in (
        (current["footprint"], "steelblue"),
        (current["pocket"], "tan"),
    ):
        xy = np.asarray(polygon.exterior.coords) - origin[:2]
        uv = xy @ np.array([basis[0, :2], basis[2, :2]]).T
        axis.fill(*uv.T, color=colour, alpha=0.5)
    axis.plot([0, 0], [-1.5, 0.7], color="green", linewidth=4)
    axis.set(
        aspect="equal",
        title="Realer Balkonplatz · unterstützter Zugang",
        xlabel="Fassaden-U [m]",
        ylabel="Außennormale [m]",
    )
    mesh = {
        "vertices": local.reshape(-1, 3),
        "faces": np.arange(local.size // 3).reshape(-1, 3),
        "materials": materials,
    }
    for slot, normal, distance, axes, title in (
        (142, [1, 0, 0], 0.0, (2, 1), "Mittelschnitt · Tür / Deck / Brüstung"),
        (143, [0, 1, 0], 0.6, (0, 2), "Horizontalschnitt · tatsächliche Körper"),
    ):
        axis = figure.add_subplot(slot)
        for line, _ in slice_triangles(mesh, normal, distance):
            axis.plot(line[:, axes[0]], line[:, axes[1]], color="black")
        axis.set(aspect="equal", title=title, xlabel="Meter", ylabel="Meter")
    axis = figure.add_subplot(144, projection="3d")
    keep = np.arange(len(local)) < sum(len(part["faces"]) for part in parts[:-1])
    spatial = local[:, :, [0, 2, 1]]
    axis.add_collection3d(
        Poly3DCollection(spatial[keep], facecolors=colours[keep], edgecolors="none")
    )
    axis.set(
        xlim=(local[:, :, 0].min(), local[:, :, 0].max()),
        ylim=(local[:, :, 2].min(), local[:, :, 2].max()),
        zlim=(local[:, :, 1].min(), local[:, :, 1].max()),
        title="2,5D · Deckenplatte zur Einsicht ausgeblendet",
    )
    axis.set_box_aspect((3.2, 3.2, current["doorway"]["floor_m"]))
    axis.view_init(elev=26, azim=65)
    figure.tight_layout()
    figure.savefig(output / "balcony-plan-sections.png", dpi=120)
    plt.close(figure)
    return proof


def generate(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze balcony source before visual comparisons")
    output.mkdir(parents=True, exist_ok=True)
    row = sources()[0] | {
        "layer": "building",
        "placement_base_enu_m": 0.0,
        "ground_asl_m": None,
        "placement_origin": "flat no DEM",
    }
    _, _, _, _, owner, plan = prepared(row)
    current = plan["cells"][0]
    near = camera(current, 10)
    cases = []
    with patch("facade_balconies.BalconyDetails.plan", return_value=None):
        cases.append(building(row, near, "Before", output, proof))
    states = {c["door_id"]: 90.0 for c in plan["cells"]}
    cases.append(building(row, near, "Closed", output, proof))
    cases.append(building(row, near, "Open", output, proof, door_states=states))
    basis = basis_for(plan["frame"])
    centre = np.asarray(row["polygon"].centroid.coords[0])
    height = row["properties"]["render_height"]
    target = np.r_[centre, height * 0.45]
    bounds = row["polygon"].bounds
    distance = max(
        height * 1.8, np.linalg.norm(np.array(bounds[2:]) - bounds[:2]) * 1.4
    )
    eye = target + (basis[2] + basis[0] * 0.3) * distance + [0, 0, height * 0.65]
    overview = View(eye, target, width=1920, height=1080, fov_deg=48)
    for name, view in (
        ("Overview", overview),
        ("Opposite", replace(overview, eye=target - (eye - target) * [1, 1, -1])),
        ("Compact", camera(current, 500)),
    ):
        cases.append(building(row, view, name, output, proof))
    for case in cases:
        data = json.loads((output / (case["name"] + "-geometry.json")).read_text())
        case["planned"] = data["procedures"]["balconies"]
        case["emitted"] = data["procedures"]["balcony_geometry"]
    (output / "balcony-comparison.json").write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "arrival": diagram(current, owner, output),
                "timings": benchmark(),
                "scope": "Actual local residential balcony composition; plain provisional arrival, whole upper routes/night/LOD/native admission unproved",
            },
            indent=2,
        )
        + "\n"
    )


def run(output):
    with patch("facade_window_bays.WindowBayDetails.plan", return_value=None):
        return generate(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
