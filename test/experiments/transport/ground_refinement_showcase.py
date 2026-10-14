"""Exact coarse/fine terrain plan and a continuous morph with fixed road contacts."""

import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from ground_surface_showcase import body, clipped
from surface_solid_showcase import write_case


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/refinement"


def main():
    with np.load(ROOT / "build/experiments/transport/ground/Vienna-ground.npz") as data:
        base = Mesh(data["vertices"], data["faces"])
        protected = int(data["road_face_count"])
    with np.load(OUTPUT / "Vienna-centroid.npz") as data:
        fine = Mesh(data["vertices"], data["faces"])
        roots = data["face_roots"]
    coarse_samples = SurfaceSampler(base).sample(fine.vertices[:, :2])
    if any(coarse_samples.reasons):
        raise ValueError("refined point does not belong to its original surface")
    zero = Mesh(coarse_samples.points_enu_m, fine.triangles)
    halfway = Mesh((zero.vertices + fine.vertices) * .5, fine.triangles)
    movement = abs(fine.vertices[:, 2] - zero.vertices[:, 2])
    centre = fine.vertices[int(np.argmax(movement)), :2]
    region = shapely.box(*(centre - 60), *(centre + 60))
    with np.load(ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz") as data:
        roads = np.where(data["part_roles"][data["face_owners"]], 1, 0)
    coarse_slots = np.r_[roads, np.full(len(base.triangles) - protected, 5)]
    fine_slots = coarse_slots[roots]
    comparison = json.loads((OUTPUT / "comparison.json").read_text())
    report = next(row for row in comparison["methods"] if row["method"] == "centroid")
    configurations = []
    for name, mesh, slots in (("coarse", base, coarse_slots), ("morph", halfway, fine_slots), ("fine", fine, fine_slots)):
        selected, material, displacement = clipped(mesh, slots, region)
        solid, colours = body(selected, material, 5.)
        record = dict(report, maximum_vertical_LOD_difference_m=float(movement.max()),
                      maximum_inspection_weld_m=displacement, detail_region_xy=list(region.bounds),
                      original_vertices_fixed=True, original_roads_fixed=True,
                      scope="120×120 m from the same saved Vienna ground plan. Coarse/fine and 50% vertex morph; fixed original roads and outer boundaries, no vertical exaggeration. Local sampled DEM check, not global DEM or C1 proof. 5 m display block only.")
        write_case("Vienna-refinement-" + name, solid, colours, record, configurations, output_dir=OUTPUT)
    for row in configurations[1:]:
        for key in ("eye", "target", "span_m", "profile"):
            row[key] = configurations[0][key]
    scene_path = OUTPUT / "scenes.json"
    scene_path.write_text(json.dumps(configurations, indent=2) + "\n")
    subprocess.run(["blender", "--background", "--factory-startup", "--python-exit-code", "1", "--python",
                    str(HERE / "render_transport_solid.py"), "--", str(scene_path)], cwd=ROOT, check=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"], cwd=ROOT, text=True).strip()
    records = {}
    for row in configurations:
        stem = OUTPUT / (row["id"] + "-solid")
        records[stem.name] = dict(image_sha256=hashlib.sha256(stem.with_suffix(".png").read_bytes()).hexdigest(),
                                  report_sha256=hashlib.sha256(stem.with_suffix(".json").read_bytes()).hexdigest())
    (OUTPUT / "origin.json").write_text(json.dumps(dict(commit=commit, working_tree_dirty=bool(dirty), records=records), indent=2) + "\n")


if __name__ == "__main__":
    main()
