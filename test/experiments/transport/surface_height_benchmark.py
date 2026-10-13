"""Same anisotropic face constraints and eight-second solver limit in fresh workers."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys

import numpy as np

from surface_height_checks import normal_gradients
from surface_height_methods import checked_fit


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/heights"


def worker(method, case):
    if case == "triangle":
        xy, faces, target = np.array([[0., 0.], [1, 0], [0, 1]]), np.array([[0, 1, 2]]), np.array([0., .04, .04])
        frames = np.array([[[1 / .08, 0.], [0., 1 / .025]]])
    else:
        with np.load(OUTPUT / "Vienna-profile-input.npz") as data:
            xy, faces, target, frames = data["xy"], data["faces"], data["targets"], data["frames"]
    results = []
    for _ in range(3):
        heights, report = checked_fit(method, xy, faces, target, seconds=8., frames=frames)
        if heights is not None:
            gradient = normal_gradients(xy, faces, heights)
            normalized = np.einsum("fki,fi->fk", frames, gradient)
            assert np.linalg.norm(normalized, axis=1).max() <= 1.0000001
        results.append(report)
    record = dict(method=method, case=case, vertices=len(xy), faces=len(faces), repeats=results,
                  input_sha256=hashlib.sha256(xy.tobytes() + faces.tobytes() + target.tobytes() + frames.tobytes()).hexdigest(),
                  peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (1 if sys.platform == "darwin" else 1024),
                  p50_total_ms=float(np.median([r["total_ms"] for r in results])),
                  valid_runs=sum(r["valid"] for r in results))
    if heights is not None and case != "triangle":
        np.savez_compressed(OUTPUT / f"Vienna-{method}-fit.npz", xy=xy, faces=faces, heights=heights, targets=target, frames=frames)
        (OUTPUT / f"Vienna-{method}-fit.json").write_text(json.dumps(report, indent=2) + "\n")
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("cone", "polygon"))
    parser.add_argument("--case", choices=("triangle", "Vienna"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker, args.case)))
        return
    records = []
    for case in ("triangle", "Vienna"):
        for method in ("cone", "polygon"):
            command = [sys.executable, "-W", "error", str(Path(__file__)), "--worker", method, "--case", case]
            records.append(json.loads(subprocess.check_output(command, cwd=ROOT, text=True)))
    assert records[0]["input_sha256"] == records[1]["input_sha256"]
    assert records[2]["input_sha256"] == records[3]["input_sha256"]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"], cwd=ROOT, text=True).strip()
    report = dict(commit=commit, working_tree_dirty=bool(dirty), cases=records,
                  scope="Whole-height fit, eight-second solver limit plus measured preparation. Fresh-worker RSS includes input, solver and independent face audit. C1, vehicle dynamics and native performance remain open.")
    (OUTPUT / "profile-benchmark.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps([{k: row[k] for k in ("case", "method", "valid_runs", "p50_total_ms", "peak_rss_bytes")} for row in records]))


if __name__ == "__main__":
    main()
