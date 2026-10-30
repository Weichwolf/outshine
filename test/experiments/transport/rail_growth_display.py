"""Matched complete Transport/Buildings/SITE views of saved ground growth."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from rail_consumers_buildings import bound_products
from stair_consumer_verify import meshes
from stair_module_materials import colour_component
from stair_occupancy_display import whole_camera
from stair_patch_inputs import whole
from stair_terrain_experiment import frozen
from stair_terrain_solid import retaining_body


def checked(root, receipt, required):
    report = json.loads((root / "comparison.json").read_text())
    proof = json.loads(receipt.read_text())
    if proof["report_sha256"] != digest(root / "comparison.json") or not proof.get(
        required
    ):
        raise ValueError("Fresh complete saved consumer verification required")
    for name, sha in report["artifacts"].items():
        if digest(root / name) != sha:
            raise ValueError("Saved whole consumer artifact changed: " + name)
    return report


def world(rail, buildings, scene):
    view, assets, supports, _contacts, _manifest = rail.state
    shown = {
        "vertices": np.empty((0, 3), "f4"),
        "triangles": np.empty((0, 3), "i4"),
        "normals": np.empty((0, 3), "f4"),
        "materials": np.empty(0, "i4"),
        "face_base_colour_linear_rgba": np.empty((0, 4), "f4"),
    }
    palette = np.array([*scene["colours"], [0.52, 0.46, 0.38, 1]])
    for asset in assets:
        first = len(shown["triangles"])
        shown = append(
            shown,
            [(Mesh(asset.data["body_vertices"], asset.data["body_faces"]), 3)],
            palette,
        )
        colour_component(shown, first, asset.data, palette)
    surface = whole(view)
    terrain, parts = retaining_body(
        {
            "xy": surface.mesh.vertices[:, :2],
            "faces": surface.mesh.triangles,
            "ground": surface.face_data["ground"],
            "targets": surface.mesh.vertices[:, 2],
            "bed": surface.mesh.vertices[:, 2],
        }
    )
    first = len(shown["triangles"])
    shown = append(shown, [(terrain, 1)], palette)
    points = terrain.vertices[terrain.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    slots = np.where(abs(normals[:, 2]) < np.linalg.norm(normals, axis=1) * 1e-6, 6, 1)
    soil = np.flatnonzero(~parts["ground"])
    slots[soil[surface.face_data["constructed"][~surface.face_data["ground"]]]] = 6
    shown["materials"][first:] = slots
    shown["face_base_colour_linear_rgba"][first:] = palette[slots]
    shown = append(
        shown,
        [
            *((p, 6) for p in supports),
            (rail.deck, 3),
            *((p, 6) for p in rail.supports),
            *((p, len(palette) - 1) for p in buildings),
        ],
        palette,
    )
    return shown, palette


def plants(root, scene, output, cache):
    rows = json.loads((root / "plants.json").read_text())
    prototype = root / "prototype/complete.npz"
    identity = digest(prototype)
    palette = np.array(
        [*scene["colours"], [0.26, 0.15, 0.07, 1], [0.055, 0.22, 0.075, 1]]
    )
    with np.load(prototype, allow_pickle=False) as data:
        vertices, faces = data["vertices"], data["faces"]
        slots = np.where(data["leaf_faces"], len(palette) - 1, len(palette) - 2)
    result = []
    for row in rows:
        origin = row["result"]["world_origin_m"]
        key = identity, tuple(origin)
        if key not in cache:
            empty = {
                "vertices": np.empty((0, 3), "f4"),
                "triangles": np.empty((0, 3), "i4"),
                "normals": np.empty((0, 3), "f4"),
                "materials": np.empty(0, "i4"),
                "face_base_colour_linear_rgba": np.empty((0, 4), "f4"),
            }
            shown = append(empty, [(Mesh(vertices + origin, faces), 0)], palette)
            shown["materials"][:] = slots
            shown["face_base_colour_linear_rgba"][:] = palette[slots]
            path = output / f"plant-{len(cache)}.npz"
            np.savez_compressed(path, **shown)
            cache[key] = {"mesh": str(path), "mesh_sha256": digest(path)}
        result.append(
            dict(cache[key], id=f"whole-SITE-{len(result)}", colours=palette.tolist())
        )
    return result


def run(args):
    commit = frozen()
    original_scene = json.loads(args.scene.read_text())
    if original_scene["profile"] != [1280, 720]:
        raise ValueError("Explicit whole 1280x720 diagnostic camera required")
    args.output.mkdir(parents=True, exist_ok=False)
    phases, reports, rails, snapshots, cache = {}, {}, {}, {}, {}
    for phase in ("before", "after"):
        building = getattr(args, phase + "_buildings")
        plant = getattr(args, phase + "_plants")
        report = checked(
            building,
            getattr(args, phase + "_receipt"),
            "float32_bodies_against_actual_raised_volumes",
        )
        plant_report = checked(
            plant,
            getattr(args, phase + "_plant_receipt"),
            "all_physical_bounds_and_SITE_products_verified",
        )
        if plant_report["building_report_sha256"] != digest(
            building / "comparison.json"
        ):
            raise ValueError("SITE must consume the same complete Buildings")
        rail = bound_products(report)
        rails[phase] = rail
        scene = dict(original_scene)
        shown, palette = world(rail, meshes(building / "buildings-after.npz"), scene)
        path = args.output / (phase + ".npz")
        np.savez_compressed(path, **shown)
        scene["colours"] = palette.tolist()
        base = {
            "id": phase + "-whole-world",
            "mesh": str(path),
            "mesh_sha256": digest(path),
            "colours": scene["colours"],
        }
        phase_plants = plants(plant, scene, args.output, cache)
        phases[phase] = dict(
            scene,
            bodies=[base, *phase_plants],
            **{k: base[k] for k in ("mesh", "mesh_sha256")},
        )
        snapshots[phase] = shown["vertices"]
        reports[phase] = {
            "buildings_sha256": digest(building / "comparison.json"),
            "plants_sha256": digest(plant / "comparison.json"),
            "ground_sources": len(report["source_ids"]),
            "whole_plants": len(phase_plants),
            "whole_buildings": report["actual_building_bodies"],
        }
    if (
        rails["before"].identity != rails["after"].identity
        or rails["before"].support_identity != rails["after"].support_identity
    ):
        raise ValueError("The same retained raised products are required")
    whole_view = whole_camera(phases["after"], np.vstack(tuple(snapshots.values())), [])
    new = rails["after"].state[1][-1].data["body_vertices"]
    centre = (new.min(0) + new.max(0)) / 2
    views = {
        "whole": whole_view,
        "detail": dict(
            whole_view,
            target=centre.tolist(),
            eye=(centre + [110, -150, 145]).tolist(),
            span_m=180,
            detail_view=True,
        ),
    }
    for phase, scene in phases.items():
        for name, camera in views.items():
            row = dict(
                camera,
                **{k: scene[k] for k in ("bodies", "mesh", "mesh_sha256", "colours")},
                id=f"ground-growth-{phase}-{name}",
                profile=[1280, 720],
                samples=16,
                output=str(args.output / f"{phase}-{name}.png"),
            )
            (args.output / f"{phase}-{name}-scene.json").write_text(
                json.dumps(row, indent=2) + "\n"
            )
    (args.output / "bindings.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "camera_palette_source_sha256": digest(args.scene),
                "phases": reports,
                "scope": "Actual full saved ground replay/terrain and unchanged raised bodies, complete adjusted Buildings and complete SITE plants. Matched 1280x720 whole/detail inspection only; no native, World, device or publication admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for phase in ("before", "after"):
        for field in ("buildings", "receipt", "plants", "plant-receipt"):
            parser.add_argument("--" + phase + "-" + field, type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp output required")
    run(args)
