"""Same saved Vienna roads/DEM: conforming earth/traffic partition and bounded fit."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import shapely

from outshine_ref.transport import HeightBudget
from ground_surface import ground_layout, fit_ground
from capture_height_targets import sample_targets
from surface_height_checks import normal_gradients


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/ground"
HEIGHTS = ROOT / "build/experiments/transport/heights"


def inputs():
    with np.load(HEIGHTS / "Vienna-cone-fit.npz") as data:
        xy, faces, frames = (data[key] for key in ("xy", "faces", "frames"))
    domain = shapely.box(*(xy.min(axis=0) - 100), *(xy.max(axis=0) + 100))
    return xy, faces, frames, domain


def geometry_worker(method):
    xy, faces, _, domain = inputs()
    times, stages = [], []
    for _ in range(5):
        began = time.perf_counter()
        layout, report = ground_layout(xy, faces, domain, method=method)
        times.append((time.perf_counter() - began) * 1000)
        stages.append(report)
    return dict(method=method, repetitions_ms=times, p50_total_ms=float(np.median(times)), stages=stages,
                layout_sha256=hashlib.sha256(layout["xy"].tobytes() + layout["faces"].tobytes()).hexdigest(),
                peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("coverage", "general"))
    parser.add_argument("--compare-geometry", action="store_true")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if args.worker:
        print(json.dumps(geometry_worker(args.worker)))
        return
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if args.compare_geometry:
        rows = [json.loads(subprocess.check_output([sys.executable, "-W", "error", str(Path(__file__)), "--worker", method], text=True))
                for method in ("coverage", "general")]
        assert rows[0]["layout_sha256"] == rows[1]["layout_sha256"]
        (OUTPUT / "geometry-benchmark.json").write_text(json.dumps(dict(commit=commit, methods=rows), indent=2) + "\n")
        print(json.dumps([{k: row[k] for k in ("method", "p50_total_ms", "peak_rss_bytes")} for row in rows]))
        return
    xy, faces, road_frames, domain = inputs()
    began = time.perf_counter()
    layout, report = ground_layout(xy, faces, domain)
    meta = json.loads((HEIGHTS / "Vienna-targets.json").read_text())
    source = Path(meta["dem_sources"][0]["path"]).parent
    targets, receipts = sample_targets(meta["origin_lat_lon"], layout["xy"], source, meta["zoom"])
    prepared_ms = (time.perf_counter() - began) * 1000
    result, frames = fit_ground(layout, targets, road_frames, budget=HeightBudget(time_limit_s=8))
    report.update(result.report, commit=commit, prepared_geometry_and_DEM_ms=prepared_ms,
                  pipeline_ms=(time.perf_counter() - began) * 1000, source_dem=receipts, soil_grade_recipe=2 / 3,
                  frame="Vienna-ENU-48.21N-16.37E-Z-mapterhorn-as-supplied",
                  source_height_input_sha256=hashlib.sha256((HEIGHTS / "Vienna-profile-input.npz").read_bytes()).hexdigest(),
                  source_usage=json.loads((ROOT / "build/experiments/transport/flat-vienna/Vienna-flat-network.json").read_text())["usage"],
                  scope="One coarse C0 ground top surface, including ground-level traffic. Full domain partition, common height plan; not C1, earth volume/collision material, a functional access plan, full Place or native runtime. Prior DEM is an approximate source, not a reconstruction target.")
    if result.mesh is None:
        (OUTPUT / "Vienna-ground.json").write_text(json.dumps(report, indent=2) + "\n")
        raise RuntimeError("no valid ground patch: " + str(result.report["reason"]))
    gradient = normal_gradients(layout["xy"], layout["faces"], result.mesh.vertices[:, 2])
    normalized = np.linalg.norm(np.einsum("fki,fi->fk", frames, gradient), axis=1)
    if normalized.max() > 1.0000001:
        raise ValueError("independent whole-face ground audit failed")
    centre = layout["xy"][layout["faces"]].mean(axis=1)
    source_z, _ = sample_targets(meta["origin_lat_lon"], centre, source, meta["zoom"])
    error = abs(source_z - result.mesh.vertices[layout["faces"], 2].mean(axis=1))
    report.update(maximum_soil_grade=float(np.linalg.norm(gradient[layout["road_faces"]:], axis=1).max()),
                  maximum_centroid_DEM_error_m=float(error.max()), p95_centroid_DEM_error_m=float(np.quantile(error, .95)),
                  peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024))
    np.savez_compressed(OUTPUT / "Vienna-ground.npz", vertices=result.mesh.vertices, faces=result.mesh.triangles,
                        road_vertex_map=layout["road_vertex_map"], road_face_count=layout["road_faces"], frames=frames, targets=targets)
    (OUTPUT / "Vienna-ground.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("vertices", "road_faces", "terrain_faces", "pipeline_ms", "maximum_soil_grade",
                                              "maximum_height_change_m", "maximum_centroid_DEM_error_m", "peak_rss_bytes")}))


if __name__ == "__main__":
    main()
