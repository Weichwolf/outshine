"""Actual provider before/after roof sections and identical-camera Blender inputs."""

import argparse
import hashlib
import json
from dataclasses import replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from broken_hip import recipe, surface
from building_mesh import slice_triangles
from evidence import revision
from matplotlib.collections import LineCollection
from outshine_ref.buildings.modelling import heightfield_envelope
from provider_contract import attributes
from residential_roof_probe import figure
from stepped_source_probe import overview
from tile_facades import project
from tile_roof_envelopes import choose, roof_surface
from verify_roof_envelopes import measures
from verify_urban_roofs import sources


def sections(row, choice, products, output, index):
    centre = np.asarray(row["polygon"].centroid.coords[0])
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    for label, product, colour in zip(
        ("Hip", "Broken"), products, ("#668b9b", "#b16d3d"), strict=True
    ):
        for column, normal in enumerate(([1, 0, 0], [0, 1, 0])):
            tangent = np.array([normal[1], -normal[0]])
            lines = slice_triangles(
                product.mesh, normal, centre @ np.asarray(normal[:2])
            )
            segments = [
                np.c_[(points[:, :2] - centre) @ tangent, points[:, 2]]
                for points, _ in lines
            ]
            axes[column].add_collection(
                LineCollection(segments, colors=colour, linewidths=1.4, label=label)
            )
    for column, axis in enumerate(axes):
        axis.autoscale_view()
        axis.set(
            aspect="equal",
            xlabel="Across source centre [m]",
            ylabel="Height [m]",
            title="Exact mesh section · " + ("N/S" if column == 0 else "E/W"),
        )
        axis.set_ylim(choice.eave_m - 0.3, choice.eave_m + choice.rise_m + 0.3)
        axis.axhline(
            choice.eave_m + choice.rise_m * 0.65, color="#aaa", ls="--", lw=0.8
        )
        axis.legend()
        axis.grid(alpha=0.15)
    fig.suptitle(row["id"] + " · same eave/top, actual original-triangle cuts")
    fig.tight_layout()
    fig.savefig(output / f"{index}-profile-sections.png")
    plt.close(fig)


def save(row, parameters, choice, product, view, output, name, proof):
    mesh, walls, _ = project(
        product.mesh,
        row,
        parameters | {"eave_m": choice.eave_m},
        (0.48, 0.53),
        3.2,
        2.9,
    )
    mesh["materials"][walls] = 32768
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    record = proof | {
        "city": name,
        "ground_mode": "flat-no-dem",
        "ground_span_m": 10000.0,
        "source_id": row["id"],
        "source_properties": row["properties"],
        "source_footprint_wkb": row["footprint_wkb"],
        "source": {
            "material_definitions": {
                "1": {
                    "color.r": 0.72,
                    "color.g": 0.73,
                    "color.b": 0.74,
                    "roughness": 0.78,
                    "metallic": 0.0,
                }
            }
        },
        "facade_materials": {
            "32768": {"wall_material": 1, "fractions": [0.48, 0.53], "filtered": True}
        },
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
        "roof_choice": choice.__dict__,
        "automatic_profile": recipe(row, parameters, choice),
        "scope": "Actual retained provider footprint and source heights; before/forced profile component comparison. Only automatic_profile marks policy selection. Generic facade diagnostic, no fitted attic/complete architecture/night/native admission",
    }
    (output / (name + "-geometry.json")).write_text(json.dumps(record, indent=2) + "\n")


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before broken roof comparisons")
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    for index, row in enumerate(sources()):
        parameters = attributes(row["properties"])
        choice = choose(row, parameters)
        top = roof_surface(row, choice)[0]
        upper = surface(top, choice.eave_m, choice.rise_m)
        before = heightfield_envelope(top, parameters["minHeight"])
        after = heightfield_envelope(upper, parameters["minHeight"])
        measures(before)
        measures(after)
        sections(row, choice, (before, after), output, index)
        figure(
            row,
            replace(choice, kind="broken hip"),
            after,
            upper,
            output / f"{index}-profile-plan-spatial.png",
        )
        views = [("Whole", [1.0, -1.25, 0.8])]
        if index == 0:
            views.append(("Opposite", [-1.0, 1.25, 0.8]))
        for label, direction in views:
            view = overview(before.mesh["vertices"], direction)
            for kind, product in (("Hip", before), ("Broken", after)):
                save(
                    row,
                    parameters,
                    choice,
                    product,
                    view,
                    output,
                    f"{index}{kind}{label}",
                    proof,
                )
        reports.append(
            {
                "id": row["id"],
                "automatic_profile": recipe(row, parameters, choice),
                "before_triangles": len(before.mesh["faces"]),
                "after_triangles": len(after.mesh["faces"]),
                "source_top_m": parameters["buildingHeight"],
                "eave_m": choice.eave_m,
            }
        )
    (output / "comparison.json").write_text(
        json.dumps(proof | {"cases": reports}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
