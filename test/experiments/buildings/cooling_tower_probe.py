"""Actual source plans, sections and whole-tile cooling-intake render inputs."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from cooling_tower_details import refine
from cooling_tower_plan import plan
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from industrial_relief_products import ArticulatedDetails
from outshine_ref.buildings.structures import classify_structure
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from recipe_graph_data import plain
from special_forms import profile
from tile_buildings import construct


def sources():
    rows, receipt, _context, view = source_tile()
    industry = shapely.union_all(
        [
            r["polygon"]
            for r in rows
            if r["layer"] == "landuse" and r["properties"].get("class") == "industrial"
        ]
    )
    towers = []
    for row in rows:
        if row["layer"] != "building":
            continue
        decision = classify_structure(
            row["polygon"],
            row["properties"],
            coordinate_step_m=row["coordinate_step_m"],
            industrial_coverage=min(
                1, row["polygon"].intersection(industry).area / row["polygon"].area
            ),
            clipped=row["clipped"],
        )
        if decision.kind == "cooling_tower":
            source, _info = profile(row["id"], row["polygon"], decision)
            towers.append((row, plan(source)))
    return rows, receipt, view, towers


def export(output, name, rows, receipt, view, proof, *, after):
    owner = ArticulatedDetails(prepare_halls(rows), view)
    mesh, source = construct(
        "Grohnde",
        error_m=0.05 if len(rows) == 1 else 1.0,
        prepared=(rows, receipt),
        ordinary_builder=owner,
        radial_builder=refine if after else None,
    )
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    record = proof | {
        "city": name,
        "ground_mode": "flat-no-dem",
        "camera": camera(view),
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "triangles": len(mesh["faces"]),
        "source": source,
        "facade_materials": owner.ordinary.facades.bindings,
        "scope": "Same provider height/footprint/classification; procedural intake/support/rim construction. No engineering or native cache/frame admission.",
    }
    (output / (name + "-geometry.json")).write_text(
        json.dumps(record, default=plain, indent=2) + "\n"
    )


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze cooling construction before evidence")
    rows, receipt, view, towers = sources()
    output.mkdir(parents=True, exist_ok=True)
    for after in (False, True):
        export(
            output,
            "GrohndeCooling" + ("After" if after else "Before"),
            rows,
            receipt,
            view,
            proof,
            after=after,
        )
    row, programme = towers[0]
    centre = np.asarray(programme.source.centre_xy)
    radius = programme.source.knots[0][1]
    for label, direction in (
        ("Near", np.array([1.0, -1.0])),
        ("Opposite", np.array([-1.0, 1.0])),
        ("Elevated", np.array([1.0, 1.0])),
    ):
        direction /= np.linalg.norm(direction)
        near = View(
            np.r_[
                centre
                + direction * (radius * 3.8 if label == "Elevated" else radius + 24),
                200.0 if label == "Elevated" else 4.5,
            ],
            np.r_[centre, 68.0 if label == "Elevated" else 8.0],
            width=1920,
            height=1080,
            fov_deg=48,
        )
        for after in (False, True):
            export(
                output,
                "Cooling" + label + ("After" if after else "Before"),
                (row,),
                receipt,
                near,
                proof,
                after=after,
            )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    models(args.output)
