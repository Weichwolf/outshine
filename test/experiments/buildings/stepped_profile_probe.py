"""Actual source profiles, shared rooms/access, exact sections and Blender inputs."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_plan_views import patch
from evidence import revision
from shapely.affinity import translate
from stepped_envelope_geometry import geometry
from stepped_openings import apply as openings
from stepped_source import exterior, near, plan_for
from stepped_source_probe import overview
from structural_shell_geometry import mesh_section
from verify_vertical import connected_spaces

FIXTURE = Path(__file__).parent / "data/stepped-profiles.json"


def sources():
    data = json.loads(FIXTURE.read_text())
    return [
        row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}
        for row in data["cases"]
    ]


def sections(plan, complete, output, name):
    figure, axes = plt.subplots(1, 3, figsize=(24, 9))
    origin = np.asarray(plan["footprint"].centroid.coords[0])
    indices = (0, plan["storeys"] - 2, plan["storeys"] - 1)
    for axis, index in zip(axes, indices, strict=True):
        current = plan["storey_plans"][index]
        actual = mesh_section(
            complete.mesh, complete.vertex_roots, current["floor_z_m"] + 1.5
        )
        for shape, colour in (
            (plan["footprint"], "#e9edef"),
            (current["terrace"], "#e6bd7b"),
            (actual, "#607783"),
            (current["terrace_guard"], "#6f5c41"),
        ):
            patch(axis, translate(shape, *-origin), facecolor=colour, edgecolor="none")
        for room in current["rooms"]:
            patch(
                axis,
                translate(room, *-origin),
                facecolor="#dce5df",
                edgecolor="white",
                linewidth=0.6,
            )
        for door in current["doors"]:
            centre = np.asarray(door["centre"]) - origin
            axis.plot(*centre, "o", color="#bb6f22", ms=4)
        axis.set(
            aspect="equal",
            title=f"Geschoss {index + 1} · {current['footprint'].area:.1f} m²",
            xlabel="Ost ab Quellzentrum [m]",
            ylabel="Nord [m]",
        )
        axis.autoscale_view()
        axis.grid(alpha=0.15)
    figure.suptitle(name + " · tatsächlicher Hüllenschnitt, Räume und Terrassenportale")
    figure.tight_layout()
    figure.savefig(output / (name + "-sections.png"), dpi=120)
    plt.close(figure)


def save(row, plan, view, output, name, proof):
    product, _, complete, _ = exterior(plan, 1)
    geometry_proof = geometry(plan, complete)
    product, opening_records = openings(plan, product, 32768)
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **product)
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
            "32768": {"wall_material": 1, "fractions": [1.0, 1.0], "filtered": True}
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
        "triangles": len(product["faces"]),
        "form_recipe": plan["form_recipe"],
        "room_ids": plan["storey_room_ids"],
        "geometry": geometry_proof,
        "opening_count": len(opening_records),
        "scope": "Actual closed source-storey exterior with shared near-plan openings; no complete architecture family, night or native/device/LOD admission",
    }
    (output / (name + "-geometry.json")).write_text(json.dumps(record, indent=2) + "\n")
    return product, complete


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before stepped-profile comparisons")
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    for index, profile in ((0, "corner"), (2, "diagonal")):
        row = sources()[index]
        legacy, current = plan_for(row), plan_for(row, profile)
        if not current["form_recipe"]["method"].startswith(profile):
            raise ValueError("Requested visible profile fell back: " + row["id"])
        points = np.array(
            [
                [x, y, z]
                for x in (row["polygon"].bounds[0], row["polygon"].bounds[2])
                for y in (row["polygon"].bounds[1], row["polygon"].bounds[3])
                for z in (legacy["base_z_m"], legacy["source_top_m"])
            ]
        )
        for label, plan in (("Legacy", legacy), (profile.title(), current)):
            connected_spaces(plan)
            _, access, paths, _ = near(plan, 1)
            for suffix, direction in (
                ("Whole", [1, -1.25, 0.8]),
                ("Opposite", [-1, 1.25, 0.8]),
            ):
                name = str(index) + label + suffix
                _, complete = save(
                    row, plan, overview(points, direction), output, name, proof
                )
            sections(plan, complete, output, str(index) + label)
            reports.append(
                {
                    "source_id": row["id"],
                    "profile": label,
                    "access": access,
                    "terrace_paths": paths,
                }
            )
    (output / "profile-verification.json").write_text(
        json.dumps(proof | {"cases": reports}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
