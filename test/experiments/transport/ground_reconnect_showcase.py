"""Same-camera whole tile and 120-m inspections before/after soil reconnection."""

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
OUTPUT = ROOT / "build/experiments/transport/reconnect"


def main():
    with np.load(ROOT / "build/experiments/transport/ground/Vienna-ground.npz") as data:
        base = Mesh(data["vertices"], data["faces"])
        protected = int(data["road_face_count"])
    with np.load(ROOT / "build/experiments/transport/refinement/Vienna-centroid.npz") as data:
        prior = Mesh(data["vertices"], data["faces"])
    samples = SurfaceSampler(base).sample(prior.vertices[:, :2])
    if any(samples.reasons):
        raise ValueError("prior lies outside the original ground")
    centre = prior.vertices[int(np.argmax(abs(prior.vertices[:, 2] - samples.points_enu_m[:, 2]))), :2]
    region = shapely.box(*(centre - 60), *(centre + 60))
    with np.load(ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz") as data:
        roads = np.where(data["part_roles"][data["face_owners"]], 1, 0)
    slots = np.r_[roads, np.full(len(prior.triangles) - protected, 5)]
    comparison = json.loads((OUTPUT / "comparison.json").read_text())
    configurations = []
    for detail in (False, True):
        start = len(configurations)
        for method in ("centroid", "delaunay", "fit"):
            mesh, report = prior, dict(prior_visually_rejected=True)
            metrics = comparison["methods"][0]["prior_quality"]
            if method != "centroid":
                with np.load(OUTPUT / f"Vienna-{method}.npz") as data:
                    mesh = Mesh(data["vertices"], data["faces"])
                report = next(row for row in comparison["methods"] if row["method"] == method)
                metrics = report["quality"]["fine"]
            top, materials, weld = clipped(mesh, slots, region) if detail else (mesh, slots, 0.)
            solid, colours = body(top, materials, 5.)
            record = dict(report, detail_region_xy=list(region.bounds) if detail else None, maximum_inspection_weld_m=weld,
                          rms_DEM_error_m=metrics["rms_m"], p95_DEM_error_m=metrics["p95_m"],
                          scope="Identical saved Vienna points and road geometry, changed soil edges only. No vertical exaggeration; 5 m display slab, not earth volume. C0, no root-based LOD or global DEM certificate.")
            name = "Vienna-reconnect-" + ("detail-" if detail else "") + method
            write_case(name, solid, colours, record, configurations, output_dir=OUTPUT)
            for key in ("eye", "target", "span_m", "profile"):
                configurations[-1][key] = configurations[start][key]
    scene = OUTPUT / "scenes.json"
    scene.write_text(json.dumps(configurations, indent=2) + "\n")
    subprocess.run(["blender", "--background", "--factory-startup", "--python-exit-code", "1", "--python",
                    str(HERE / "render_transport_solid.py"), "--", str(scene)], cwd=ROOT, check=True)
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
