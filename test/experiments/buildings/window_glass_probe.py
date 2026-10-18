"""One real source, one unchanged closed model and explicit window camera controls."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import ROOF_MATERIAL, plan
from terraced_facade_probe import materials
from terraced_mass_sources import sources


def views(owner, mesh):
    mass = owner.plan.body
    windows = owner.body.facade.physical().facade.windows
    anchor = np.asarray(shapely.from_wkb(mass.source_wkb).centroid.coords[0])
    window = min(
        (w for w in windows if w.storey == 1 and w.edge[0] == 0),
        key=lambda w: np.linalg.norm(np.asarray(w.centre) - anchor),
    )
    centre, tangent = np.asarray(window.centre), np.asarray(window.tangent)
    outward = np.array([tangent[1], -tangent[0]])
    z = window.bottom_m + window.height_m / 2
    target = np.r_[centre - outward * 0.10, z]
    poses = {
        "Near": (np.r_[centre + 4 * outward - 0.8 * tangent, z + 0.45], target),
        "Front": (np.r_[centre + 4 * outward, z], target),
        "Grazing": (np.r_[centre + outward + 5 * tangent, z + 0.10], target),
        "Inside": (np.r_[centre - 3 * outward + 0.5 * tangent, z], target),
    }
    result = {
        name: camera(View(eye, aim, width=1920, height=1080, fov_deg=42))
        for name, (eye, aim) in poses.items()
    }
    result["Whole"] = camera(overview(mesh["vertices"], [1, -1.25, 0.85]))
    return window.identifier, result


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the glass model producer first")
    row = sources()[0]
    owner = RoofedProducts(plan(row, "cascade"))
    mesh = owner.draw(True)
    if not np.isfinite(mesh["normals"]).all():
        raise ValueError("Window control has nonfinite source normals")
    window, poses = views(owner, mesh)
    palette = materials() | {
        str(ROOF_MATERIAL): {
            "color.r": 0.26,
            "color.g": 0.29,
            "color.b": 0.30,
            "roughness": 0.84,
            "metallic": 0.0,
        }
    }
    record = {
        "source_id": row["id"],
        "source_properties": row["properties"],
        "source_footprint_wkb": row["polygon"].wkb_hex,
        "source": {"material_definitions": palette},
        "facade_materials": {},
        "ground_span_m": 10000,
        "camera": poses["Near"],
        "diagnostic_window": window,
        "pane_thickness_m": 0.008,
        "pane_thickness_source": "terraced_facade_details.details: rectangle outward=-0.079, inward=0.087",
        "scope": "Original provider row and existing complete closed body/roof/window model; camera controls only. No independent new geometry/controller qualification.",
    }
    output.mkdir(parents=True, exist_ok=True)
    write(output, "WindowNear", mesh, record, proof)
    near = json.loads((output / "WindowNear-geometry.json").read_text())
    for name, pose in poses.items():
        if name != "Near":
            (output / ("Window" + name + "-geometry.json")).write_text(
                json.dumps(near | {"city": "Window" + name, "camera": pose}, indent=2)
                + "\n"
            )
    receipt = {
        "source_id": row["id"],
        "pane_triangles": int((mesh["materials"] == 3).sum()),
        "triangles": len(mesh["faces"]),
        "mesh_sha256": near["mesh_sha256"],
        "arrays_sha256": {
            k: hashlib.sha256(v.tobytes()).hexdigest() for k, v in mesh.items()
        },
        "camera_controls": list(poses),
    }
    (output / "models.json").write_text(json.dumps(proof | receipt, indent=2) + "\n")
    print(json.dumps(receipt), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
