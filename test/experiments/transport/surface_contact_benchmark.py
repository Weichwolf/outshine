"""Equal selected-face contacts: STRtree predicate versus boxes plus exact filter."""

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

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/ground"


class BoxSampler(SurfaceSampler):
    def _pairs(self, xy):
        tolerance = self.xy_tolerance_m
        pairs = self._tree.query(shapely.box(xy[:, 0] - tolerance, xy[:, 1] - tolerance,
                                             xy[:, 0] + tolerance, xy[:, 1] + tolerance))
        valid = shapely.dwithin(self._polygons[pairs[1]], shapely.points(xy[pairs[0]]), tolerance)
        return pairs[:, valid]


def worker(method):
    with np.load(OUTPUT / "Vienna-ground.npz") as data:
        mesh = Mesh(data["vertices"], data["faces"])
    random = np.random.default_rng(739)
    lo, hi = mesh.vertices[:, :2].min(axis=0), mesh.vertices[:, :2].max(axis=0)
    queries = np.vstack((random.uniform(lo - 10, hi + 10, (20000, 2)), mesh.vertices[:1000, :2],
                         mesh.vertices[mesh.triangles[:1000, :2], :2].mean(axis=1)))
    began = time.perf_counter()
    sampler = (SurfaceSampler if method == "predicate" else BoxSampler)(mesh, max_faces=131072)
    preparation_ms = (time.perf_counter() - began) * 1000
    records = []
    for _ in range(5):
        began = time.perf_counter()
        result = sampler.sample(queries)
        records.append((time.perf_counter() - began) * 1000)
    valid = result.face_indices >= 0
    p = mesh.vertices[mesh.triangles[result.face_indices[valid]]]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normal /= np.where(normal[:, 2:3] < 0, -1., 1.) * np.linalg.norm(normal, axis=1)[:, None]
    np.testing.assert_allclose(normal, result.normals_enu[valid], atol=1e-12)
    error = np.einsum("ij,ij->i", result.points_enu_m[valid] - p[:, 0], normal)
    assert np.max(abs(error), initial=0) < 1e-10
    digest = hashlib.sha256(result.points_enu_m.tobytes() + result.normals_enu.tobytes() + result.face_indices.tobytes()
                            + "\n".join(result.reasons).encode()).hexdigest()
    return dict(method=method, queries=len(queries), input_sha256=hashlib.sha256(queries.tobytes()).hexdigest(),
                result_sha256=digest, preparation_ms=preparation_ms, repetitions_ms=records,
                p50_query_ms=float(np.median(records)), contacts=dict(result.report),
                independent_max_plane_error_m=float(np.max(abs(error), initial=0)),
                peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("predicate", "boxes"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    records = [json.loads(subprocess.check_output([sys.executable, "-W", "error", str(Path(__file__)), "--worker", method], text=True))
               for method in ("predicate", "boxes")]
    assert records[0]["input_sha256"] == records[1]["input_sha256"]
    assert records[0]["result_sha256"] == records[1]["result_sha256"]
    report = dict(methods=records, commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  scope="Saved Vienna combined top surface, 22000 interior/boundary/outside requests. Five queries on prepared sampler in fresh worker per method; independent plane/normal audit. No native runtime or smooth-normal proof.")
    (OUTPUT / "contacts-benchmark.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(records))


if __name__ == "__main__":
    main()
