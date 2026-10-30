"""Bound one whole area to the actual complete old world and append only its floor."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from cycle_bridge_display import append
from elevated_rail_hardware import bound_input
from experiment_output import create_output
from ground_area_product import read_product
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from rail_occupancy_guard import planning
from rail_occupancy_inputs import catalog
from stair_consumer_verify import meshes
from stair_occupancy_candidate_reuse import retained
from stair_terrain_experiment import frozen


def whole_context(inputs, body, upper, lower, args):
    report = json.loads((args.buildings / "comparison.json").read_text())
    parent = inputs.case["parent_snapshot"]
    if (
        digest(args.buildings / "comparison.json") != parent["building_report_sha256"]
        or digest(args.building_receipt) != parent["building_reader_sha256"]
    ):
        raise ValueError("Same unchanged complete old consumer snapshot required")
    old = bound_input(args.buildings, args.building_receipt)
    if old.state[0].patches[0].frame.identifier != inputs.case["frame"]:
        raise ValueError("Actual shared source/old-world frame changed")
    local = old.state[0].faces(
        tuple(tuple(v) for v in inputs.case["terrain"]["face_keys"]), max_faces=512
    )
    if (
        local.mesh is None
        or not np.array_equal(local.mesh.vertices, inputs.surface.vertices)
        or not np.array_equal(local.mesh.triangles, inputs.surface.triangles)
        or not np.array_equal(
            local.face_data["ground"], inputs.case["terrain"]["ground"]
        )
    ):
        raise ValueError("Whole actual old terrain query differs from saved input")
    footprint = shapely.coverage_union_all(
        shapely.polygons(upper.vertices[upper.triangles, :2])
    )
    prepared, cache = retained(catalog(old), 8192)
    query = planning(old, prepared=prepared)
    probe = BuildingFootprint(
        "whole-pedestrian-area",
        footprint,
        HeightInterval(
            float(lower.vertices[:, 2].min()),
            float(upper.vertices[:, 2].max() + inputs.clearance_m),
        ),
    )
    conflict, work = query.conflict(probe)
    if not conflict.is_empty:
        raise ValueError("Whole old transport/body/clearance conflicts with new area")
    scene = json.loads(args.scene.read_text())
    base = json.loads(args.base_scene.read_text())
    base_binding = json.loads((args.base_scene.parent / "bindings.json").read_text())[
        "phases"
    ]["after"]
    if base_binding["buildings_sha256"] != parent[
        "building_report_sha256"
    ] or base_binding["ground_sources"] != len(report["source_ids"]):
        raise ValueError("Actual complete old scene is not bound to its consumers")
    scene_binding = json.loads((args.scene.parent / "bindings.json").read_text())
    if args.scene != args.base_scene and (
        scene_binding["baseline_scene_sha256"] != digest(args.base_scene)
        or scene["bodies"][: len(base["bodies"])] != base["bodies"]
    ):
        raise ValueError("Existing complete extension must retain its exact old bodies")
    obstacles = meshes(args.buildings / "buildings-after.npz")
    boxes = []
    for mesh in obstacles:
        for cast in (np.float64, np.float32):
            xy = mesh.vertices.astype(cast).astype(float)[:, :2]
            boxes.append(shapely.box(*xy.min(0), *xy.max(0)))
    for row in scene["bodies"]:
        if digest(Path(row["mesh"])) != row["mesh_sha256"]:
            raise ValueError("Existing actually displayed whole body changed")
    for row in scene["bodies"][1:]:
        with np.load(row["mesh"], allow_pickle=False) as saved:
            xy = saved["vertices"][:, :2]
            boxes.append(shapely.box(*xy.min(0), *xy.max(0)))
    if np.any(shapely.intersects(footprint, np.array(boxes, object))):
        raise ValueError(
            "New whole floor/clearance overlaps retained buildings/plants/hardware"
        )
    if scene["profile"] != [1280, 720] or scene.get("detail_view"):
        raise ValueError("Actual complete whole-scene comparison required")
    return scene, {
        "whole_old_consumer_snapshot_bound": True,
        "old_terrain_query_unchanged": True,
        "transport_query": work,
        "retained_volume_records": cache().currsize,
        "old_building_bodies": len(obstacles),
        "plant_and_extra_body_boxes": len(scene["bodies"]) - 1,
        "no_old_mesh_or_terrain_deformation_changed": True,
    }


def run(args):
    commit = frozen()
    inputs, product, body, upper, lower, _proof = read_product(args.products)
    scene, guard = whole_context(inputs, body, upper, lower, args)
    create_output(args.output)
    palette = np.array([scene["colours"][3]])
    empty = {
        "vertices": np.empty((0, 3), "f4"),
        "triangles": np.empty((0, 3), "u4"),
        "normals": np.empty((0, 3), "f4"),
        "materials": np.empty(0, "i4"),
        "face_base_colour_linear_rgba": np.empty((0, 4), "f4"),
    }
    shown = append(empty, [(body, 0)], palette)
    mesh = args.output / "whole-area.npz"
    np.savez_compressed(mesh, **shown)
    added = {
        "id": "whole-pedestrian-area",
        "mesh": str(mesh),
        "mesh_sha256": digest(mesh),
        "colours": palette.tolist(),
    }
    centre = np.r_[
        np.array(inputs.original.centroid.coords[0]), upper.vertices[:, 2].mean()
    ]
    cameras = {
        "whole": scene,
        "detail": dict(
            scene,
            target=centre.tolist(),
            eye=(centre + [20, -27, 24]).tolist(),
            span_m=35,
            detail_view=True,
        ),
    }
    for phase in ("before", "after"):
        for name, camera in cameras.items():
            row = dict(
                camera,
                id="ground-area-" + phase + "-" + name,
                bodies=[*scene["bodies"], *([added] if phase == "after" else [])],
                samples=16,
                output=str(args.output / (phase + "-" + name + ".png")),
            )
            (args.output / (phase + "-" + name + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
    note = {
        "commit": commit,
        "product_report_sha256": digest(args.products / "report.json"),
        "baseline_scene_sha256": digest(args.scene),
        "source": product["source"],
        "guard": guard,
        "old_body_pins": [
            {"id": r["id"], "mesh_sha256": r["mesh_sha256"]} for r in scene["bodies"]
        ],
        "added_vertices": len(body.vertices),
        "added_triangles": len(body.triangles),
        "material_basis": "Existing held sealed-transport diagnostic material, not a newly measured source property.",
        "scope": "Complete old 115 ground/69 Buildings/four SITE and raised rail snapshot, one supported whole floor appended. No routing, accessibility, Source/World/GPU/device admission or image publication.",
    }
    (args.output / "bindings.json").write_text(json.dumps(note, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--building-receipt", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--base-scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
