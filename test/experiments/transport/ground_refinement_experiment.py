"""Same point/face/grade budget: paired edge midpoints versus face centroids."""

import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from capture_height_targets import sample_targets, terrain_tile
from ground_refinement import refine_ground
from surface_height_checks import normal_gradients


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/refinement"
GROUND = ROOT / "build/experiments/transport/ground"


def cached_target():
    meta = json.loads((ROOT / "build/experiments/transport/heights/Vienna-targets.json").read_text())
    sources = Path(meta["dem_sources"][0]["path"]).parent

    @lru_cache(maxsize=8)
    def decoded(path, coordinate, fetch):
        heights, receipt = terrain_tile(path, coordinate, fetch)
        heights.setflags(write=False)
        return heights, receipt

    def target(xy):
        heights, _ = sample_targets(meta["origin_lat_lon"], xy, sources, meta["zoom"], tile_loader=decoded)
        return heights

    return target, meta


def evaluate(base, fine, target, protected):
    lo, hi = base.vertices[:, :2].min(axis=0), base.vertices[:, :2].max(axis=0)
    random = np.random.default_rng(1383)
    queries = np.vstack((random.uniform(lo, hi, (12000, 2)), base.vertices[base.triangles[protected:], :2].mean(axis=1)))
    truth = target(queries)
    result = {}
    for name, mesh in (("coarse", base), ("fine", fine)):
        sample = SurfaceSampler(mesh).sample(queries)
        if any(sample.reasons):
            raise ValueError("independent evaluation has unresolved contact")
        error = abs(truth - sample.points_enu_m[:, 2])
        result[name] = dict(maximum_m=float(error.max()), p95_m=float(np.quantile(error, .95)),
                            mean_m=float(error.mean()), rms_m=float(np.sqrt(np.mean(error * error))))
    result["queries"] = len(queries)
    result["query_sha256"] = hashlib.sha256(queries.tobytes()).hexdigest()
    gradient = normal_gradients(fine.vertices[:, :2], fine.triangles[protected:], fine.vertices[:, 2])
    if np.linalg.norm(gradient, axis=1).max() > (2 / 3) * 1.0000001:
        raise ValueError("independent whole-face grade check failed")
    result["maximum_soil_grade"] = float(np.linalg.norm(gradient, axis=1).max())
    if not shapely.coverage_is_valid(shapely.polygons(fine.vertices[fine.triangles, :2])):
        raise ValueError("refinement coverage or shared boundaries failed")
    return result


def worker(method):
    with np.load(GROUND / "Vienna-ground.npz") as data:
        base = Mesh(data["vertices"], data["faces"])
        protected = int(data["road_face_count"])
    target, meta = cached_target()
    records = []
    for _ in range(3):
        fine, roots, report = refine_ground(base, target, protected_faces=protected, method=method)
        records.append(report)
    quality = evaluate(base, fine, target, protected)
    np.testing.assert_array_equal(fine.vertices[:len(base.vertices)], base.vertices)
    np.testing.assert_array_equal(fine.triangles[:protected], base.triangles[:protected])
    np.savez_compressed(OUTPUT / f"Vienna-{method}.npz", vertices=fine.vertices, faces=fine.triangles, face_roots=roots,
                        road_face_count=protected, road_vertex_map=np.load(GROUND / "Vienna-ground.npz")["road_vertex_map"])
    report = dict(method=method, repetitions=records, quality=quality,
                  p50_refinement_ms=float(np.median([r["refinement_ms"] for r in records])),
                  source_ground_sha256=hashlib.sha256((GROUND / "Vienna-ground.npz").read_bytes()).hexdigest(),
                  source_DEM=meta, peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024))
    (OUTPUT / f"Vienna-{method}.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("midpoint", "centroid"))
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    records = [json.loads(subprocess.check_output([sys.executable, "-W", "error", str(Path(__file__)), "--worker", method], text=True))
               for method in ("midpoint", "centroid")]
    if records[0]["quality"]["query_sha256"] != records[1]["quality"]["query_sha256"]:
        raise ValueError("method quality inputs differ")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    (OUTPUT / "comparison.json").write_text(json.dumps(dict(commit=commit, methods=records), indent=2) + "\n")
    print(json.dumps([{k: row[k] for k in ("method", "p50_refinement_ms", "quality", "peak_rss_bytes")} for row in records]))


if __name__ == "__main__":
    main()
