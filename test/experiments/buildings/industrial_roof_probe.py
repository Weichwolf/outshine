"""Actual hall profiles, solid sections and paired current-generator Blender inputs."""

import argparse
import hashlib
import json
from contextlib import nullcontext
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from evidence import revision
from industrial_roof_choice import section_for
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.modelling import heightfield_envelope
from provider_contract import attributes
from recess_probe import building
from structural_shell_geometry import mesh_section
from tile_buildings import construct, read_tile
from tile_facades import contexts
from tile_roof_envelopes import choose, projected_error, roof_surface
from tile_stepped import SteppedDetails
from verify_industrial_roofs import sources, view_for


def prior(row, parameters, previous):
    return previous


def diagram(row, output, index):
    p = attributes(row["properties"])
    choice = choose(row, p)
    section = section_for(row, choice)
    top = roof_surface(row, choice)[0]
    closed = heightfield_envelope(top, 0)
    figure = plt.figure(figsize=(24, 9))
    axis = figure.add_subplot(131)
    knots = np.asarray(section.knots)
    axis.plot(*knots.T, color="#236c80", marker="o")
    axis.axhline(p["buildingHeight"], color="#888888", linestyle="--")
    axis.set(
        aspect="equal",
        xlabel="Section run [m]",
        ylabel="Height [m]",
        title="Metre section · source top retained",
    )
    axis.grid(alpha=0.15)
    axis = figure.add_subplot(132)
    origin = np.asarray(row["polygon"].centroid.coords[0])
    for fraction, colour in ((0.25, "#377c91"), (0.75, "#a6683f")):
        pieces = mesh_section(
            closed.mesh, closed.vertex_roots, choice.eave_m + choice.rise_m * fraction
        )
        for polygon in shapely.get_parts(pieces):
            axis.plot(*(np.asarray(polygon.exterior.coords) - origin).T, color=colour)
            for ring in polygon.interiors:
                axis.plot(*(np.asarray(ring.coords) - origin).T, color=colour)
    axis.set(
        aspect="equal",
        xlabel="East from centroid [m]",
        ylabel="North [m]",
        title="Actual closed-mesh sections · 25% / 75% rise",
    )
    axis = figure.add_subplot(133, projection="3d")
    triangles = top["vertices"][top["faces"]].copy()
    triangles[:, :, :2] -= origin
    normals = top["normals"][top["faces"][:, 0]]
    colour = (
        np.array([0.62, 0.66, 0.69])
        * (0.45 + 0.55 * np.maximum(normals @ np.array([0.3, -0.4, 0.8660254]), 0))[
            :, None
        ]
    )
    axis.add_collection3d(
        Poly3DCollection(triangles, facecolors=colour, edgecolors="none")
    )
    low, high = (
        triangles.reshape(-1, 3).min(axis=0),
        triangles.reshape(-1, 3).max(axis=0),
    )
    axis.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        zlim=(0, p["buildingHeight"]),
        title="2.5D · exact generated upper surface",
    )
    axis.set_box_aspect((high[0] - low[0], high[1] - low[1], p["buildingHeight"]))
    figure.suptitle(row["id"] + " · " + choice.kind + " · source roof type unproven")
    figure.tight_layout()
    figure.savefig(output / f"{index}-sections.png", dpi=100)
    plt.close(figure)


def models(output, proof):
    for i, (row, neighbors) in enumerate(sources()):
        diagram(row, output, i)
        whole = view_for(row)
        far = whole
        while (
            projected_error(
                row,
                attributes(row["properties"]),
                choose(row, attributes(row["properties"])),
                far,
            )
            > 0.45
        ):
            far = replace(far, eye=far.target + (far.eye - far.target) * 1.5)
        if np.linalg.norm(far.eye - far.target) > 9000:
            raise ValueError("Far roof witness exceeds the actual camera range")
        opposite = replace(
            whole, eye=whole.target - (whole.eye - whole.target) * [1, 1, -1]
        )
        for label, view in (("Whole", whole), ("Opposite", opposite), ("Far", far)):
            with patch("industrial_roof_choice.extend", side_effect=prior):
                building(
                    row,
                    view,
                    f"{i}Before{label}",
                    output,
                    proof,
                    context_rows=neighbors,
                )
            building(
                row, view, f"{i}Sections{label}", output, proof, context_rows=neighbors
            )


def tile(output, proof):
    rows, receipt = read_tile("Grohnde")
    contexts(rows, receipt)
    buildings = [row for row in rows if row["layer"] == "building"]
    polygon = shapely.union_all([row["polygon"] for row in buildings])
    view = view_for({"polygon": polygon, "properties": {"render_height": 90}})
    for label in ("Before", "Sections"):
        owner = SteppedDetails(rows, view)
        options = (
            patch("industrial_roof_choice.extend", side_effect=prior)
            if label == "Before"
            else nullcontext()
        )
        with options:
            mesh, source = construct(
                "Grohnde", prepared=(rows, receipt), ordinary_builder=owner
            )
        path = output / f"Grohnde{label}-mesh.npz"
        np.savez_compressed(path, **mesh)
        camera = {
            "projection": "perspective",
            "eye": view.eye.tolist(),
            "target": view.target.tolist(),
            "vertical_fov_deg": view.fov_deg,
            "clip_near_m": 0.1,
            "clip_far_m": 10000,
            "profile": [view.width, view.height],
        }
        record = proof | {
            "representation": "python-city-building-study",
            "city": "Grohnde" + label,
            "ground_mode": "flat-no-dem",
            "sources": receipt,
            "source": source,
            "camera": camera,
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "triangles": len(mesh["faces"]),
            "facade_materials": owner.facades.bindings,
            "procedures": {
                "stepped": owner.report(),
                "facades": owner.facades.report(),
                "roofs": owner.facades.base.report(),
            },
            "scope": "Complete provider tile, flat plane; current Python procedures and unmeasured industrial roof estimate, no native/device/night/full image LOD admission",
        }
        (output / f"Grohnde{label}-geometry.json").write_text(
            json.dumps(record, indent=2) + "\n"
        )
        print(label, source["generated"], len(mesh["faces"]), flush=True)


def run(output, include_tile):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze sources before industrial roof diagnostics")
    output.mkdir(parents=True, exist_ok=True)
    models(output, proof)
    if include_tile:
        tile(output, proof)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tile", action="store_true")
    args = parser.parse_args()
    run(args.output, args.tile)
