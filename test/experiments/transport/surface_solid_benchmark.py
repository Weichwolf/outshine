"""Compare complete closures in fresh workers with identical meshes and topology checks."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np

from outshine_ref.contracts import Mesh
from surface_solid_checks import audit_closed
from surface_solid_methods import close_surface, dictionary_boundary, sorted_boundary


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CASES = {
    "T": "build/experiments/transport/showcase/fixtures/T-flat-surfaces.npz",
    "courtyard": "build/experiments/transport/showcase/fixtures/courtyard_loop-flat-surfaces.npz",
    "Vienna": "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz",
}


def worker(path, method):
    with np.load(path) as data:
        surface = Mesh(np.c_[data["vertices"], np.zeros(len(data["vertices"]))], data["indices"])
    samples = []
    for _ in range(7):
        began = time.perf_counter()
        result = close_surface(surface, .25, sorted_boundary if method == "sorted" else dictionary_boundary)
        samples.append((time.perf_counter() - began) * 1000)
    volume = audit_closed(result)
    return dict(method=method, input_triangles=len(surface.triangles), output_triangles=len(result.triangles),
                mesh_sha256=hashlib.sha256(result.vertices.tobytes() + result.triangles.tobytes()).hexdigest(),
                p50_ms=float(np.median(samples)), p95_ms=float(np.percentile(samples, 95)),
                peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024),
                volume_m3=volume, raw_mesh_bytes=result.vertices.nbytes + result.triangles.nbytes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("sorted", "dictionary"))
    parser.add_argument("--mesh", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "build/experiments/transport/solid/benchmark.json")
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.mesh, args.worker)))
        return
    cases = []
    for name, relative in CASES.items():
        path = ROOT / relative
        results = [json.loads(subprocess.check_output(
            [sys.executable, "-W", "error", str(Path(__file__)), "--worker", method, "--mesh", str(path)],
            cwd=ROOT, text=True,
        )) for method in ("sorted", "dictionary")]
        assert results[0]["mesh_sha256"] == results[1]["mesh_sha256"], name
        cases.append(dict(case=name, input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), methods=results))
    report = dict(cases=cases, repeats=7, scope="Complete body closure only; inputs loaded before timing. Peak RSS includes worker, input, output and the final independent topology audit.")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
