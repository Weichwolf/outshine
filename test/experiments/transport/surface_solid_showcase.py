"""Build checked 3D inspections from existing surfaces, then render their exact buffers."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface
from surface_solid_checks import audit_closed


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/solid"


def lifted(path, gradient, offset, depth):
    with np.load(path) as data:
        xy, faces = data["vertices"], data["indices"]
        roles = data["part_roles"][data["face_owners"]]
    source = Mesh(np.c_[xy, xy @ gradient + offset], faces)
    began = time.perf_counter()
    body = close_surface(source, depth)
    elapsed = (time.perf_counter() - began) * 1000
    materials = np.full(len(body.triangles), 2, dtype=int)
    materials[:len(faces)] = np.where(roles, 1, 0)
    return body, materials, dict(closing_ms=elapsed, depth_m=depth, input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                 vertex_gradient=gradient.tolist(), height_assumption_m=offset)


def bridge():
    meshes, slots = [], []
    for xy, height, depth, colour in (([[-30, -4], [30, -4], [30, 4], [-30, 4]], 6.2, .35, 3),
                                      ([[-3, -25], [3, -25], [3, 25], [-3, 25]], 0., .25, 0)):
        surface = Mesh(np.c_[xy, np.full(4, height)], np.array([[0, 1, 2], [0, 2, 3]]))
        meshes.append(close_surface(surface, depth))
        material = np.full(len(meshes[-1].triangles), 2, dtype=int)
        material[:2] = colour
        slots.append(material)
    body = Mesh(np.vstack([m.vertices for m in meshes]),
                np.vstack((meshes[0].triangles, meshes[1].triangles + len(meshes[0].vertices))))
    return body, np.concatenate(slots), dict(clearance_m=5.85, requested_clearance_m=5.,
                                            scope="Synthetic crossing with explicit heights; no ramps, supports, terrain or network-level elevation solver.")


def inspection_view(vertices, *, eye_direction=(.62, -.85, .85)):
    minimum, maximum = vertices.min(axis=0), vertices.max(axis=0)
    centre = (minimum + maximum) * .5
    span = max(float((maximum - minimum)[:2].max()), 1.)
    eye = centre + np.asarray(eye_direction) * span
    direction = centre - eye
    direction /= np.linalg.norm(direction)
    right = np.cross(direction, [0., 0., 1.])
    right /= np.linalg.norm(right)
    up = np.cross(right, direction)
    relative = vertices - centre
    frame_span = max(float(np.abs(relative @ right).max()) * 2,
                     float(np.abs(relative @ up).max()) * 2 * 1920 / 1080)
    return dict(eye=eye.tolist(), target=centre.tolist(), span_m=frame_span * 1.1, profile=[1920, 1080])


def write_case(name, body, materials, report, configurations, view=None, *, output_dir=OUTPUT):
    output = output_dir / f"{name}-solid.npz"
    np.savez_compressed(output, vertices=body.vertices, triangles=body.triangles, materials=materials)
    volume = audit_closed(body)
    if volume <= 0:
        raise ValueError("closed body must have positive volume")
    configuration = dict(id=name, mesh=str(output), mesh_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                         output=str(output_dir / f"{name}-solid.png"), **inspection_view(body.vertices))
    if view:
        configuration.update(view)
    configurations.append(configuration)
    report.update(triangles=len(body.triangles), vertices=len(body.vertices), volume_m3=volume,
                  raw_mesh_bytes=body.vertices.nbytes + body.triangles.nbytes,
                  closed_oriented_edges=True, renderer="Blender Workbench", profile=configuration["profile"],
                  mesh_sha256=configuration["mesh_sha256"])
    (output_dir / f"{name}-solid.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    configurations = []
    for name, case, gradient, offset in (("T", "T", [.06, .02], 1.5), ("courtyard", "courtyard_loop", [0., 0.], 1.0)):
        path = ROOT / f"build/experiments/transport/showcase/fixtures/{case}-flat-surfaces.npz"
        body, materials, report = lifted(path, np.array(gradient), offset, .35)
        write_case(name, body, materials, report, configurations)
    body, materials, report = bridge()
    write_case("bridge", body, materials, report, configurations)
    path = ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz"
    surface_report = json.loads(path.with_suffix(".json").read_text())
    if surface_report["ports"]["invalid_sections"] or surface_report["cuts"]["remaining_cut_conflicts"]:
        raise ValueError("Vienna source does not have valid shared cuts")
    body, materials, report = lifted(path, np.array([0., 0.]), 0., .25)
    input_path = ROOT / "build/experiments/transport/inputs/Vienna-input.json"
    report.update(shared_port_segments=surface_report["shared_port_segments"],
                  source_usage=json.loads((ROOT / "build/experiments/transport/flat-vienna/Vienna-flat-network.json").read_text())["usage"],
                  source_input=str(input_path), source_input_sha256=hashlib.sha256(input_path.read_bytes()).hexdigest(),
                  source_receipt=json.loads(input_path.read_text())["receipt"],
                  source_polygons_deferred=len(json.loads(input_path.read_text())["deferred_sources"]),
                  scope="One full Vienna source tile at assumed flat height; only supported ground axes used. No DEM, drivable turns or elevated-route recipes proven.")
    write_case("Vienna", body, materials, report, configurations)
    config_path = OUTPUT / "scenes.json"
    config_path.write_text(json.dumps(configurations, indent=2) + "\n")
    if args.build_only:
        return
    subprocess.run(["blender", "--background", "--factory-startup", "--python-exit-code", "1", "--python", str(HERE / "render_transport_solid.py"),
                    "--", str(config_path)], cwd=ROOT, check=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
                                    cwd=ROOT, text=True).strip()
    records = {}
    for row in configurations:
        base = OUTPUT / f"{row['id']}-solid"
        records[base.name] = dict(image_sha256=hashlib.sha256(base.with_suffix(".png").read_bytes()).hexdigest(),
                                 report_sha256=hashlib.sha256(base.with_suffix(".json").read_bytes()).hexdigest())
    (OUTPUT / "origin.json").write_text(json.dumps(dict(commit=commit, working_tree_dirty=bool(dirty), records=records), indent=2) + "\n")


if __name__ == "__main__":
    main()
