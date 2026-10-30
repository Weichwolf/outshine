"""Append the verified whole hardware to an unchanged complete consumer scene."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from elevated_rail_hardware_verify import arrays
from experiment_output import create_output
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import bake_rail_sleepers
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    report = json.loads((args.products / "report.json").read_text())
    proof = json.loads(args.receipt.read_text())
    if proof["report_sha256"] != digest(args.products / "report.json") or not proof.get(
        "whole_hardware_verified"
    ):
        raise ValueError("Fresh complete hardware verification required")
    for name, pin in report["artifacts"].items():
        if digest(args.products / name) != pin:
            raise ValueError("Actually displayed whole hardware bytes changed")
    scene = json.loads(args.scene.read_text())
    binding = json.loads((args.scene.parent / "bindings.json").read_text())["phases"][
        "after"
    ]
    if (
        scene["profile"] != [1280, 720]
        or scene.get("detail_view")
        or binding["buildings_sha256"] != report["building_report_sha256"]
        or binding["ground_sources"] != len(report["ground_source_ids"])
    ):
        raise ValueError(
            "Same bound complete consumer snapshot and whole camera required"
        )
    for body in scene["bodies"]:
        if digest(Path(body["mesh"])) != body["mesh_sha256"]:
            raise ValueError("Complete previous world/plant body changed")
    create_output(args.output)
    palette = np.array([r["colour"] for r in report["material_recipe"]])
    parameters = [
        {key: row[key] for key in ("metallic", "roughness")}
        for row in report["material_recipe"]
    ]
    shown = {
        "vertices": np.empty((0, 3), "f4"),
        "triangles": np.empty((0, 3), "u4"),
        "normals": np.empty((0, 3), "f4"),
        "materials": np.empty(0, "i4"),
        "face_base_colour_linear_rgba": np.empty((0, 4), "f4"),
    }
    origin = np.asarray(report["object_origin_m"])
    for record in report["components"]:
        data = arrays(
            args.products / record["file"],
            ("rail_vertices", "rail_triangles", "sleeper_transforms"),
        )
        rails = Mesh(data["rail_vertices"] + origin, data["rail_triangles"])
        transforms = data["sleeper_transforms"].copy()
        transforms[:, :, 3] += origin
        sleepers = bake_rail_sleepers(transforms)
        shown = append(shown, [(rails, 0), (sleepers, 1)], palette)
    mesh = args.output / "whole-hardware.npz"
    np.savez_compressed(mesh, **shown)
    hardware = {
        "id": "whole-raised-hardware",
        "mesh": str(mesh),
        "mesh_sha256": digest(mesh),
        "colours": palette.tolist(),
        "material_parameters": parameters,
    }
    first = report["components"][0]
    axis = np.asarray(first["axis_xy_m"])
    changes = [
        i
        for i in range(1, len(first["segment_sources"]))
        if first["segment_sources"][i] != first["segment_sources"][i - 1]
    ]
    centre = np.r_[axis[changes[0] if changes else len(axis) // 2], report["datum_m"]]
    cameras = {
        "whole": dict(scene),
        "detail": dict(
            scene,
            target=centre.tolist(),
            eye=(centre + [35, -45, 40]).tolist(),
            span_m=60,
            detail_view=True,
        ),
    }
    for phase in ("before", "after"):
        bodies = [*scene["bodies"], *([hardware] if phase == "after" else [])]
        for name, camera in cameras.items():
            value = dict(
                camera,
                id=f"raised-hardware-{phase}-{name}",
                bodies=bodies,
                output=str(args.output / f"{phase}-{name}.png"),
                samples=16,
            )
            (args.output / f"{phase}-{name}-scene.json").write_text(
                json.dumps(value, indent=2) + "\n"
            )
    (args.output / "bindings.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "hardware_report_sha256": digest(args.products / "report.json"),
                "hardware_receipt_sha256": digest(args.receipt),
                "baseline_scene_sha256": digest(args.scene),
                "baseline_bodies": [
                    {"id": b["id"], "mesh_sha256": b["mesh_sha256"]}
                    for b in scene["bodies"]
                ],
                "added_vertices": len(shown["vertices"]),
                "added_triangles": len(shown["triangles"]),
                "material_recipe": report["material_recipe"],
                "scope": "Same complete terrain/115 ground sources/retained raised deck94piers/69Buildings/four whole SITE plants. Only verified hardware added. Diagnostic bake; compact product remains instanced. Procedural PBR assumptions, no native/World/device admission or image publication.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
