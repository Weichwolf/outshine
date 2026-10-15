"""Render current Python vegetation at an actual fixed native Place camera."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_preview import material
from outshine_ref.places import Elevation
from place_camera import PLACES, RESOLUTION, selected_cameras
from place_canopy import populate
from place_ground import terrain
from place_lighting import ground_material, setup, sun_direction
from place_stand import sites
from place_water import water

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def surface(name, model, shader):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(model.vertices.tolist(), [], model.triangles.tolist())
    mesh.materials.append(shader)
    mesh.update()
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def renderer_camera(scene):
    actual = np.array(scene.camera.matrix_world)
    projection = scene.camera.calc_matrix_camera(
        bpy.context.evaluated_depsgraph_get(),
        x=RESOLUTION[0],
        y=RESOLUTION[1],
        scale_x=1,
        scale_y=1,
    )
    return {
        "eye_enu_m": actual[:3, 3].tolist(),
        "basis_right_up_back": actual[:3, :3].tolist(),
        "vertical_fov_deg": float(np.rad2deg(2 * np.arctan(1 / projection[1][1]))),
    }


def render(name, sources, products, output):
    modified = subprocess.check_output(
        ["git", "status", "--porcelain"], cwd=ROOT, text=True
    )
    if modified:
        raise ValueError("render needs a clean frozen source checkout")
    start = time.perf_counter()
    camera = next(c for c in selected_cameras(ROOT) if c.name == name)
    inputs = json.loads(sources.read_text())
    selected = next(p for p in inputs["places"] if p["camera"]["name"] == name)
    if camera.scenario_sha256 != selected["camera"]["scenario_sha256"]:
        raise ValueError("input sources refer to a different Place camera")
    receipts = [r for r in inputs["receipts"] if r["url"] in selected["source_urls"]]
    elevation = Elevation(receipts)
    ground = float(elevation.sample(camera.latitude_deg, camera.longitude_deg))
    model, sampler, ground_report = terrain(
        camera, elevation, selected["terrain_source_radius_m"]
    )
    contact = sampler.sample(camera.eye(ground)[None, :2])
    clearance = float(camera.eye(ground)[2] - contact.points_enu_m[0, 2])
    if not np.isfinite(clearance) or clearance <= 0:
        raise ValueError("declared actual camera is inside source-derived terrain")
    site_arrays, site_report = sites(camera, receipts, sampler)
    water_model, water_report = water(camera, elevation, receipts)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / f"{name}-sites.npz", **site_arrays)
    print(
        json.dumps(
            {
                "stage": "plan",
                "sites": site_report["sites"],
                "elapsed_s": time.perf_counter() - start,
            }
        ),
        flush=True,
    )
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    surface("actual DEM terrain", model, ground_material())
    if len(water_model.triangles):
        surface(
            "source water", water_model, material("water", (0.018, 0.07, 0.065), 0.12)
        )
    sun, solar = sun_direction(camera)
    receiver_floor = min(
        float(model.vertices[:, 2].min()),
        float(water_model.vertices[:, 2].min())
        if len(water_model.vertices)
        else float("inf"),
    )
    plant_inputs, canopy = populate(
        site_arrays, products, camera, ground, sun, receiver_floor
    )
    print(
        json.dumps(
            {
                "stage": "canopy",
                "demand": canopy["demand"],
                "elapsed_s": time.perf_counter() - start,
            }
        ),
        flush=True,
    )
    scene = setup(camera, ground, RESOLUTION, sun)
    bpy.context.view_layer.update()
    actual_camera = renderer_camera(scene)
    scene.render.filepath = str((output / f"{name}.png").resolve())
    setup_s = time.perf_counter() - start
    print(json.dumps({"stage": "render", "setup_s": setup_s}), flush=True)
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    renderer_s = time.perf_counter() - began
    code = list((ROOT / "test/experiments/vegetation").glob("*.py")) + list(
        (ROOT / "lib/src/outshine_ref/places").glob("*.py")
    )
    paths = [
        sources,
        ROOT / "src/assets/places" / f"{name}.scenario",
        *plant_inputs,
        *(Path(r["path"]) for r in receipts if not r.get("missing")),
    ]
    report = {
        "place": name,
        "scenario_sha256": camera.scenario_sha256,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_modified": False,
        "scene_source_modified": False,
        "resolution": list(RESOLUTION),
        "actual_camera": actual_camera,
        "ground_at_camera_asl_m": ground,
        "camera_clearance_m": clearance,
        "source_radius_m": selected["initial_source_radius_m"],
        "terrain": ground_report,
        "water": water_report,
        "stand": site_report,
        "canopy": canopy,
        "solar": solar,
        "setup_s": setup_s,
        "render_s": renderer_s,
        "engine": scene.render.engine,
        "backend": gpu.platform.backend_type_get(),
        "renderer": gpu.platform.renderer_get(),
        "input_sha256": {str(p.resolve()): digest(p) for p in paths},
        "implementation_sha256": {str(p.relative_to(ROOT)): digest(p) for p in code},
        "limits": "Current Python vegetation with initial actual geospatial context. Buildings context and final transport reservations missing; incomplete far vegetation/species/terrain materials. No native world, botanical, LOD, frame or device admission.",
    }
    (output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "place": name,
                "sites": canopy["sites"],
                "setup_s": setup_s,
                "render_s": renderer_s,
                "renderer": report["renderer"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--place", required=True, choices=PLACES)
    parser.add_argument("--sources", required=True, type=Path)
    parser.add_argument("--products", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    render(args.place, args.sources, args.products, args.output)
