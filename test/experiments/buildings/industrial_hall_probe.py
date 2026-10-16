"""Actual source halls: plan/section/2.5D then paired Blender exterior/state/interior inputs."""

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
from demand import View
from evidence import revision
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from recess_probe import building
from stepped_source_probe import overview
from structural_shell_geometry import mesh_section
from tile_buildings import construct as tile_mesh
from tile_buildings import read_tile
from tile_facades import contexts
from tile_stepped import SteppedDetails
from verify_industrial_halls import construct, sources


def diagram(row, neighbors, output, index):
    _owner, mesh, _info, value, shell, chunks, _collision = construct(row, neighbors)
    origin = np.asarray(row["polygon"].centroid.coords[0])
    figure = plt.figure(figsize=(24, 13.5))
    ax = figure.add_subplot(131)
    for region, color in (
        (row["polygon"], "#777777"),
        (shapely.from_wkb(value["interior_wkb"]), "#236c80"),
        (shapely.from_wkb(value["route_wkb"]), "#49a46d"),
        (shapely.from_wkb(value["gate"]["pocket_wkb"]), "#b57840"),
    ):
        ax.plot(*(np.asarray(region.exterior.coords) - origin).T, color=color)
    section = mesh_section(shell.mesh, shell.vertex_roots, 1.0)
    for p in shapely.get_parts(section):
        ax.fill(*(np.asarray(p.exterior.coords) - origin).T, color="#777777", alpha=0.3)
        for ring in p.interiors:
            ax.fill(*(np.asarray(ring.coords) - origin).T, color="white")
    ax.set(
        aspect="equal",
        xlabel="East [m]",
        ylabel="North [m]",
        title="Actual shell at 1m · room / pawn route / ramp",
    )
    ax = figure.add_subplot(132)
    gate = value["gate"]
    t = np.asarray(gate["tangent"])
    inside = np.array([-t[1], t[0]])
    base = np.r_[gate["centre"], 0]
    for part, color in ((dict(shell.mesh), "#777777"), (chunks[0], "#b57840")):
        triangles = part["vertices"][part["faces"]] - base
        triangles[:, :, :2] @ t
        from building_mesh import slice_triangles

        raw = {
            "vertices": part["vertices"] - base,
            "faces": part["faces"],
            "materials": part["materials"],
        }
        for line, _ in slice_triangles(raw, [*t, 0], 0):
            depth = line[:, :2] @ inside
            ax.plot(depth, line[:, 2], color=color)
    for state, color in ((0, "#ae5361"), (0.5, "#397ca2"), (1, "#49a46d")):
        ax.plot([-0.02, -0.02], [3 * state, 3], color=color, lw=4)
    ax.set(
        xlim=(-0.4, 6),
        ylim=(-0.02, 3.7),
        xlabel="Depth inside [m]",
        ylabel="Height [m]",
        title="Actual wall/floor/ramp section · gate states",
    )
    ax = figure.add_subplot(133, projection="3d")
    p = mesh["vertices"][mesh["faces"]].copy()
    selected = p[:, :, 2].max(axis=1) < value["ceiling_m"] - 1e-6
    p = p[selected]
    p[:, :, :2] -= origin
    colours = np.tile([0.62, 0.66, 0.69], (len(p), 1))
    colours[mesh["materials"][selected] == 0] = [0.66, 0.62, 0.53]
    ax.add_collection3d(Poly3DCollection(p, facecolors=colours, edgecolors="none"))
    a, b, c, d = row["polygon"].bounds
    ax.set(
        xlim=(a - origin[0], c - origin[0]),
        ylim=(b - origin[1], d - origin[1]),
        zlim=(0, 3.6),
        title="2.5D · diagnostic removes uppermost faces only",
    )
    ax.set_box_aspect((c - a, d - b, 3.6))
    ax.view_init(elev=40, azim=-65)
    figure.suptitle(
        row["id"]
        + " · industrial use inferred from provider context; no measured architecture"
    )
    figure.tight_layout()
    figure.savefig(output / f"hall-{index}-plan-sections.png", dpi=100)
    plt.close(figure)


def models(output, proof):
    for index, (row, neighbors) in enumerate(sources()[:3]):
        _, mesh, _, value, *_ = construct(row, neighbors)
        gate = value["gate"]
        t = np.asarray(gate["tangent"])
        normal = np.array([t[1], -t[0]])
        whole = overview(mesh["vertices"], np.r_[normal + t * 0.35, 0.65])
        whole = replace(whole, width=1920, height=1080)
        diagram(row, neighbors, output, index)
        for label in ("Before", "Hall"):
            option = (
                patch("tile_industrial_halls.HallDetails.__call__", return_value=None)
                if label == "Before"
                else nullcontext()
            )
            with option:
                building(
                    row,
                    whole,
                    f"Hall{index}{label}",
                    output,
                    proof,
                    context_rows=neighbors,
                    gate_states={gate["id"]: 1},
                    door_states={value["entry"]["id"]: 90},
                )
        if index == 2:
            centre = np.asarray(gate["centre"])
            close = View(
                np.r_[centre + normal * 9 + t * 1.2, 2.2],
                np.r_[centre - normal * 0.8, 1.6],
                width=1920,
                height=1080,
                fov_deg=48,
            )
            interior = replace(
                close,
                eye=np.r_[centre - normal * 9 + t * 1.2, 1.8],
                target=np.r_[centre, 1.6],
            )
            opposite = replace(
                whole, eye=whole.target - (whole.eye - whole.target) * [1, 1, -1]
            )
            for label, view, state in (
                ("Closed", close, 0),
                ("Half", close, 0.5),
                ("Open", close, 1),
                ("Inside", interior, 1),
                ("Opposite", opposite, 1),
            ):
                building(
                    row,
                    view,
                    "Hall2" + label,
                    output,
                    proof,
                    context_rows=neighbors,
                    gate_states={gate["id"]: state},
                    door_states={value["entry"]["id"]: 90},
                )


def tile(output, proof):
    from provider_contract import attributes
    from recipes import choose

    rows, receipt = read_tile("Grohnde")
    contexts(rows, receipt)
    buildings = [r for r in rows if r["layer"] == "building"]
    fitting = [
        r
        for r in buildings
        if choose(
            r["properties"],
            r.get("context", {}),
            r["polygon"],
            attributes(r["properties"])["buildingHeight"],
            0,
        )["family"]
        == "industrial"
    ]
    region = shapely.union_all([r["polygon"] for r in fitting])
    a, b, c, d = region.bounds
    view = overview(
        np.array([[x, y, z] for x in (a, c) for y in (b, d) for z in (0, 8)]),
        [1, -1, 0.65],
    )
    view = replace(view, width=1920, height=1080)
    for name, factor in (("Grohnde", 1.0), ("GrohndeClose", 0.45)):
        current = replace(view, eye=view.target + (view.eye - view.target) * factor)
        tile_product(output, proof, rows, receipt, current, name)


def tile_product(output, proof, rows, receipt, view, name):
    for label in ("Before", "Halls"):
        owner = SteppedDetails(rows, view)
        option = (
            patch("tile_industrial_halls.HallDetails.__call__", return_value=None)
            if label == "Before"
            else nullcontext()
        )
        with option:
            mesh, source = tile_mesh(
                "Grohnde", prepared=(rows, receipt), ordinary_builder=owner
            )
        path = output / f"{name}{label}-mesh.npz"
        np.savez_compressed(path, **mesh)
        record = proof | {
            "city": name + label,
            "ground_mode": "flat-no-dem",
            "sources": receipt,
            "source": source,
            "camera": {
                "projection": "perspective",
                "eye": view.eye.tolist(),
                "target": view.target.tolist(),
                "vertical_fov_deg": view.fov_deg,
                "clip_near_m": 0.1,
                "clip_far_m": 10000,
                "profile": [1920, 1080],
            },
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "triangles": len(mesh["faces"]),
            "facade_materials": owner.facades.bindings,
            "procedures": {
                "facades": owner.facades.report(),
                "roofs": owner.facades.base.report(),
            },
            "scope": "All actual provider building sources retained; industrial group camera, flat/no DEM; simple source-bound halls, no native/device/night/whole-image LOD admission",
        }
        (output / f"{name}{label}-geometry.json").write_text(
            json.dumps(record, indent=2) + "\n"
        )
        print(
            label,
            source["generated"],
            len(mesh["faces"]),
            len(owner.facades.halls.completed),
            owner.facades.halls.rejected,
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tile", action="store_true")
    args = parser.parse_args()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before preparing render inputs")
    args.output.mkdir(parents=True, exist_ok=True)
    models(args.output, proof)
    if args.tile:
        tile(args.output, proof)
