"""Inspect retained Vienna surfaces before/after whole-face DEM height fitting."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface
from surface_height_checks import normal_gradients
from surface_solid_showcase import write_case


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/heights"


def case(name, xy, faces, heights, frames, roles, report, configurations):
    normalized = np.linalg.norm(np.einsum("fki,fi->fk", frames, normal_gradients(xy, faces, heights)), axis=1)
    invalid = normalized > 1.0000001
    began = time.perf_counter()
    body = close_surface(Mesh(np.c_[xy, heights], faces), .25)
    materials = np.full(len(body.triangles), 2, dtype=int)
    materials[:len(faces)] = np.where(invalid, 4, np.where(roles, 1, 0))
    report = dict(report, closing_ms=(time.perf_counter() - began) * 1000, invalid_grade_faces=int(invalid.sum()),
                  maximum_normalized_grade=float(normalized.max()), height_range_m=[float(heights.min()), float(heights.max())],
                  depth_m=.25, top_faces=len(faces), colours="Orange violates whole-face limits; green junctions; grey corridors.")
    write_case(name, body, materials, report, configurations, output_dir=OUTPUT)


def local_mesh(xy, faces, heights, owners, selected_owner):
    selected = owners == selected_owner
    vertices, remapped = np.unique(faces[selected], return_inverse=True)
    if not selected.any():
        raise ValueError("empty local height inspection")
    return xy[vertices], remapped.reshape(-1, 3), heights[vertices], selected, vertices


def build():
    with np.load(OUTPUT / "Vienna-cone-fit.npz") as data:
        xy, faces, target, fitted, frames = (data[key] for key in ("xy", "faces", "targets", "heights", "frames"))
    surface_path = ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz"
    with np.load(surface_path) as data:
        if not np.array_equal(data["vertices"], xy) or not np.array_equal(data["indices"], faces):
            raise ValueError("height fit no longer belongs to the retained surface")
        owners = data["face_owners"]
        roles = data["part_roles"][owners]
    profile_path = OUTPUT / "Vienna-profile-input.npz"
    with np.load(profile_path) as data:
        if any(not np.array_equal(data[key], value) for key, value in
               (("xy", xy), ("faces", faces), ("targets", target), ("frames", frames))):
            raise ValueError("height input or direction recipes changed after fitting")
    benchmark = json.loads((OUTPUT / "profile-benchmark.json").read_text())
    measurement = next(row for row in benchmark["cases"] if row["case"] == "Vienna" and row["method"] == "cone")
    fit_report = json.loads((OUTPUT / "Vienna-cone-fit.json").read_text())
    if not fit_report["valid"]:
        raise ValueError("cannot present an invalid fitted surface")
    direction_path = OUTPUT / "Vienna-profile-input.json"
    directions = json.loads(direction_path.read_text())
    report = dict(fit_report, source_inputs=json.loads((OUTPUT / "Vienna-targets.json").read_text()),
                  directions=dict(path=str(direction_path), sha256=hashlib.sha256(direction_path.read_bytes()).hexdigest(),
                                  recipes=directions["recipes"], crossfall=directions["crossfall"],
                                  uncertain_faces=len(directions["isotropic_uncertain_faces"])),
                  input_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
                  source_usage=json.loads((ROOT / "build/experiments/transport/flat-vienna/Vienna-flat-network.json").read_text())["usage"],
                  p50_fit_ms=measurement["p50_total_ms"], peak_rss_bytes=measurement["peak_rss_bytes"],
                  scope="One saved Vienna ground-axis tile. Whole-face grade limits, shared vertex heights and oriented closed body checked. C1, superelevation, terrain contacts, drivable turns and elevated routes remain open.")
    configurations = []
    for name, heights in (("Vienna-dem-prior", target), ("Vienna-dem-fit", fitted)):
        entry = dict(report, height_fit_applied=name.endswith("fit"))
        if not entry["height_fit_applied"]:
            entry.update(valid=False, reason="unconstrained_dem_prior_diagnostic", maximum_height_change_m=0.)
        case(name, xy, faces, heights, frames, roles, entry, configurations)
    violations = np.linalg.norm(np.einsum("fki,fi->fk", frames, normal_gradients(xy, faces, target)), axis=1)
    owner = owners[int(np.argmax(violations))]
    local_xy, local_faces, local_target, selected, vertices = local_mesh(xy, faces, target, owners, owner)
    for name, heights in (("Vienna-detail-prior", local_target), ("Vienna-detail-fit", fitted[vertices])):
        entry = dict(report, height_fit_applied=name.endswith("fit"), selected_surface_part=int(owner),
                     maximum_height_change_m=float(np.max(abs(fitted[vertices] - target[vertices]))),
                     scope=report["scope"] + " Detail contains the complete existing part with the worst prior gradient; no refitting, window clipping or vertical exaggeration.")
        if not entry["height_fit_applied"]:
            entry.update(valid=False, reason="unconstrained_dem_prior_diagnostic", maximum_height_change_m=0.)
        case(name, local_xy, local_faces, heights, frames[selected], roles[selected], entry, configurations)
    # Before/after pairs use exactly the same camera, including the height bounds.
    for prior, fitted_view in ((configurations[0], configurations[1]), (configurations[2], configurations[3])):
        for key in ("eye", "target", "span_m", "profile"):
            fitted_view[key] = prior[key]
    return configurations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    configurations = build()
    config_path = OUTPUT / "scenes.json"
    config_path.write_text(json.dumps(configurations, indent=2) + "\n")
    if args.build_only:
        return
    subprocess.run(["blender", "--background", "--factory-startup", "--python-exit-code", "1", "--python",
                    str(HERE / "render_transport_solid.py"), "--", str(config_path)], cwd=ROOT, check=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"], cwd=ROOT, text=True).strip()
    records = {}
    for configuration in configurations:
        base = OUTPUT / f"{configuration['id']}-solid"
        records[base.name] = dict(image_sha256=hashlib.sha256(base.with_suffix(".png").read_bytes()).hexdigest(),
                                 report_sha256=hashlib.sha256(base.with_suffix(".json").read_bytes()).hexdigest())
    (OUTPUT / "origin.json").write_text(json.dumps(dict(commit=commit, working_tree_dirty=bool(dirty), records=records), indent=2) + "\n")


if __name__ == "__main__":
    main()
