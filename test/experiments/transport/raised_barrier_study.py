"""Bind complete side bodies to actual raised sources and an unchanged whole scene."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from cycle_bridge_display import append
from elevated_rail_hardware import bound_input, selected_bed
from elevated_rail_hardware_verify import arrays, source_axis
from elevated_rail_joints import joints
from experiment_output import create_output
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware._geometry import SLEEPER
from outshine_ref.transport.rail_hardware._planar import projected_rail_footprints
from raised_edge_barriers import BarrierRecipe, construct
from stair_module_inputs import reference_profiles
from stair_terrain_experiment import frozen


def bound_inputs(directory, receipt):
    report = json.loads((directory / "report.json").read_text())
    proof = json.loads(receipt.read_text())
    if proof.get("report_sha256") != digest(directory / "report.json") or not proof.get(
        "whole_hardware_verified"
    ):
        raise ValueError("Independently verified whole raised hardware required")
    for name, pin in report["artifacts"].items():
        if Path(name).name != name or digest(directory / name) != pin:
            raise ValueError("Bound whole hardware artifact changed")
    source = bound_input(Path(report["buildings"]), Path(report["building_receipt"]))
    bed, triangle = selected_bed(source)
    original = {r["source_id"]: r for r in source.rows}
    allowed = {
        frozenset(source.rows[i]["source_id"] for i in joint["indices"])
        for joint in joints(source.rows)
    }
    if report["ground_source_ids"] != source.state[4]["source_ids"]:
        raise ValueError("Whole old ground source snapshot changed")
    clearance = min(
        reference_profiles()[r["original_properties"]["subclass"]]["clearanceM"]
        for r in source.rows
    )
    cases = []
    for row in report["components"]:
        axis, keys = source_axis(row, original, allowed)
        widths = {original[k]["width_m"] for k in keys}
        if len(widths) != 1:
            raise ValueError("One actual existing width across whole chain required")
        data = arrays(
            directory / row["file"],
            ("rail_vertices", "rail_triangles", "sleeper_transforms"),
        )
        cases.append(
            {"axis": axis, "keys": keys, "width": widths.pop(), "hardware": data}
        )
    if {k for c in cases for k in c["keys"]} != set(original):
        raise ValueError("Every raised original must occur in a complete chain")
    return report, bed, float(triangle[0, 2]), clearance, cases


def hardware_shapes(data, cast):
    rail = Mesh(
        data["rail_vertices"].astype(cast).astype(float), data["rail_triangles"]
    )
    tr = data["sleeper_transforms"]
    positions = (
        np.einsum("nij,vj->nvi", tr[:, :, :3], SLEEPER.vertices) + tr[:, None, :, 3]
    )
    positions = positions.astype(cast).astype(float)
    return [*projected_rail_footprints(rail), *shapely.polygons(positions[:, :4, :2])]


def build(cases, bed, clearance, recipe):
    result = []
    for i, case in enumerate(cases):
        for side, row in enumerate(
            construct(case["axis"], case["width"], bed, clearance, recipe)
        ):
            result.append(dict(row, component=i, side=side, source_ids=case["keys"]))
    return result


def physical_guards(rows, cases):
    notes = []
    for cast in (np.float64, np.float32):
        hardware = np.array(
            [p for case in cases for p in hardware_shapes(case["hardware"], cast)],
            object,
        )
        tree = shapely.STRtree(hardware)
        for row in rows:
            lower = row["lower"]
            xy = lower.vertices.astype(cast).astype(float)[:, :2]
            footprint = shapely.coverage_union_all(
                shapely.polygons(xy[lower.triangles])
            )
            hits = tree.query(footprint, predicate="intersects")
            if len(hits):
                raise ValueError(
                    "Stored side body intersects original rail/sleeper hardware"
                )
            notes.append(
                {
                    "component": row["component"],
                    "side": row["side"],
                    "storage": np.dtype(cast).name,
                    "hardware_shapes": len(hardware),
                    "overlapping_shapes": len(hits),
                }
            )
    return notes


def scene_products(scene_path, rows, datum, output):
    scene = json.loads(scene_path.read_text())
    if scene["profile"] != [1280, 720] or scene.get("detail_view"):
        raise ValueError("Complete old whole-scene comparison required")
    for row in scene["bodies"]:
        if digest(Path(row["mesh"])) != row["mesh_sha256"]:
            raise ValueError("Complete old displayed body changed")
    empty = {
        "vertices": np.empty((0, 3), "f8"),
        "triangles": np.empty((0, 3), "u4"),
        "normals": np.empty((0, 3), "f8"),
        "materials": np.empty(0, "i4"),
        "face_base_colour_linear_rgba": np.empty((0, 4), "f4"),
    }
    bodies = [
        (Mesh(r["body"].vertices + [0, 0, datum], r["body"].triangles), 0) for r in rows
    ]
    shown = append(empty, bodies, np.array([[0.48, 0.46, 0.42, 1]]))
    path = output / "whole-barriers-display.npz"
    np.savez_compressed(path, **shown)
    added = {
        "id": "whole-raised-edge-barriers",
        "mesh": str(path),
        "mesh_sha256": digest(path),
        "colours": [[0.48, 0.46, 0.42, 1]],
        "material_parameters": [{"metallic": 0.0, "roughness": 0.8}],
    }
    centre = np.r_[np.mean(rows[0]["body"].vertices[:, :2], axis=0), datum + 0.5]
    for phase in ("before", "after"):
        for name, camera in (
            ("whole", scene),
            (
                "detail",
                dict(
                    scene,
                    eye=(centre + [32, -43, 31]).tolist(),
                    target=centre.tolist(),
                    span_m=65,
                    detail_view=True,
                ),
            ),
        ):
            row = dict(
                camera,
                id="raised-barrier-" + phase + "-" + name,
                samples=16,
                bodies=[*scene["bodies"], *([added] if phase == "after" else [])],
                output=str(output / (phase + "-" + name + ".png")),
            )
            (output / (phase + "-" + name + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
    return {
        "old_scene_sha256": digest(scene_path),
        "old_body_pins": scene["bodies"],
        "no_old_geometry_or_terrain_changed": True,
    }


def run(args):
    commit = frozen()
    create_output(args.output)
    report, bed, datum, clearance, cases = bound_inputs(args.hardware, args.receipt)
    recipe = BarrierRecipe()
    result = build(cases, bed, clearance, recipe)
    guard = physical_guards(result, cases)
    products = []
    for i, row in enumerate(result):
        path = args.output / ("side-" + str(i) + ".npz")
        np.savez_compressed(
            path,
            body_vertices=row["body"].vertices,
            body_triangles=row["body"].triangles,
            lower_vertices=row["lower"].vertices,
            cap_triangles=row["lower"].triangles,
        )
        products.append(
            {
                "file": path.name,
                "sha256": digest(path),
                "component": row["component"],
                "side": row["side"],
                "source_ids": row["source_ids"],
                "proof": row["proof"],
                "vertices": len(row["body"].vertices),
                "triangles": len(row["body"].triangles),
            }
        )
    scene = scene_products(args.scene, result, datum, args.output)
    result = {
        "commit": commit,
        "hardware_report_sha256": digest(args.hardware / "report.json"),
        "hardware": str(args.hardware),
        "hardware_receipt": str(args.receipt),
        "scene": str(args.scene),
        "frame": report["frame"],
        "recipe": asdict(recipe),
        "object_origin_m": [0, 0, datum],
        "clearance_m": clearance,
        "products": products,
        "hardware_guards": guard,
        "scene_binding": scene,
        "scope": "Complete local whole-source side bodies on unchanged reserved flat deck. Procedural dimensions/materials, no normative safety/vehicle/World/native/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sides": len(products),
                "sources": len({k for p in products for k in p["source_ids"]}),
                "report": str(args.output / "report.json"),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("hardware", "receipt", "scene", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    run(parser.parse_args())
