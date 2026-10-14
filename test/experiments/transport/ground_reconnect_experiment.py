"""Fresh-worker comparison on identical saved Vienna terrain, DEM and work caps."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from ground_reconnect import reconnect_ground
from ground_refinement_experiment import cached_target, evaluate


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/reconnect"
GROUND = ROOT / "build/experiments/transport/ground/Vienna-ground.npz"
PRIOR = ROOT / "build/experiments/transport/refinement/Vienna-centroid.npz"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def worker(method, repeat):
    with np.load(GROUND) as data:
        base, protected = Mesh(data["vertices"], data["faces"]), int(data["road_face_count"])
    with np.load(PRIOR) as data:
        prior = Mesh(data["vertices"], data["faces"])
    target, metadata = cached_target()
    before, pins = digest(GROUND), digest(PRIOR)
    fine, report = reconnect_ground(prior, target, protected_faces=protected, method=method)
    np.testing.assert_array_equal(fine.vertices, prior.vertices)
    np.testing.assert_array_equal(fine.triangles[:protected], base.triangles[:protected])
    quality = evaluate(base, fine, target, protected)
    prior_quality = evaluate(base, prior, target, protected)["fine"]
    lo, hi = base.vertices[:, :2].min(axis=0), base.vertices[:, :2].max(axis=0)
    domain = shapely.box(*lo, *hi)
    polys = shapely.polygons(fine.vertices[fine.triangles, :2])
    union = shapely.coverage_union_all(polys)
    if not union.equals(domain):
        raise ValueError("exchanged surface lost complete ground coverage")
    edges, counts = np.unique(np.sort(np.vstack((fine.triangles[:, [0, 1]], fine.triangles[:, [1, 2]], fine.triangles[:, [2, 0]])), axis=1),
                             axis=0, return_counts=True)
    if np.any(counts > 2) or not np.all(shapely.covered_by(shapely.linestrings(fine.vertices[edges[counts == 1], :2]), domain.boundary)):
        raise ValueError("surface has an interior crack or nonmanifold edge")
    report.update(quality=quality, prior_quality=prior_quality, repeat=repeat, vertices=len(fine.vertices), faces=len(fine.triangles),
                  mesh_sha256=hashlib.sha256(fine.vertices.tobytes() + fine.triangles.tobytes()).hexdigest(),
                  ground_sha256=before, prior_sha256=pins, source_DEM=metadata,
                  peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024),
                  scope="Same 2500 added points/65514 faces and fixed XYZ/roads; C0 offline candidate, not native or complete LOD/DEM proof.")
    if digest(GROUND) != before or digest(PRIOR) != pins:
        raise ValueError("source plan changed during comparison")
    np.savez_compressed(OUTPUT / f"Vienna-{method}.npz", vertices=fine.vertices, faces=fine.triangles, road_face_count=protected)
    (OUTPUT / f"Vienna-{method}.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("delaunay", "fit"))
    parser.add_argument("--repeat", type=int, default=0)
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if args.worker:
        print(json.dumps(worker(args.worker, args.repeat)))
        return
    rows = []
    for method in ("delaunay", "fit"):
        reports = [json.loads(subprocess.check_output([sys.executable, "-W", "error", str(Path(__file__)), "--worker", method,
                                                      "--repeat", str(i)], text=True)) for i in range(3)]
        if len({r["mesh_sha256"] for r in reports}) != 1:
            raise ValueError("bounded worker results differ; inspect before claiming a fixed comparison")
        row = dict(method=method, repetitions=reports, p50_reconnect_ms=float(np.median([r["reconnect_ms"] for r in reports])),
                   peak_rss_bytes=max(r["peak_rss_bytes"] for r in reports), quality=reports[0]["quality"], prior_quality=reports[0]["prior_quality"])
        rows.append(row)
        print(json.dumps({k: row[k] for k in ("method", "p50_reconnect_ms", "peak_rss_bytes", "quality")}), flush=True)
    if len({row["quality"]["query_sha256"] for row in rows}) != 1:
        raise ValueError("methods did not use the same independent DEM queries")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    (OUTPUT / "comparison.json").write_text(json.dumps(dict(commit=commit, methods=rows), indent=2) + "\n")


if __name__ == "__main__":
    main()
