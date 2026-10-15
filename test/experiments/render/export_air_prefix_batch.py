"""Generate explicit ray batches for throughput/checksum experiments, never scene frames."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
RAY = np.dtype([("ray", "<f4", 5), ("begin", "<u4"), ("end", "<u4"), ("pad", "<u4")])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rays", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lanes", type=int, required=True)
    parser.add_argument("--queries-per-ray", type=int, default=1)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private batches belong outside the repository")
    count = args.lanes * args.queries_per_ray
    if (
        not 1 <= args.lanes <= 4 * 1024**2
        or not 1 <= args.queries_per_ray <= 64
        or count * 4 > 128 * 1024**2
    ):
        raise ValueError("batch exceeds the explicit native-probe input budget")
    templates = np.fromfile(args.rays, dtype=RAY)
    if not len(templates):
        raise ValueError("at least one complete physical ray is required")
    rays = templates[np.arange(args.lanes) % len(templates)].copy()
    rays["begin"] = np.arange(args.lanes, dtype=np.uint32) * args.queries_per_ray
    rays["end"] = rays["begin"] + args.queries_per_ray
    fractions = np.arange(1, args.queries_per_ray + 1, dtype="<f4")
    queries = rays["ray"][:, 4, None] * fractions / np.float32(args.queries_per_ray)
    output.mkdir(parents=True, exist_ok=True)
    rays.tofile(output / "rays.bin")
    queries.astype("<f4").tofile(output / "queries.bin")
    report = {
        "scope": "Synthetic repeated physical rays; no scene rendering, image coverage or frame-time proof. The events64 shader uses one output per lane and internally averages 64 queries as a live checksum.",
        "lanes": args.lanes,
        "queries_per_ray": args.queries_per_ray,
        "input_template_count": len(templates),
        "input_template_sha256": hashlib.sha256(args.rays.read_bytes()).hexdigest(),
        "ray_bytes": rays.nbytes,
        "query_bytes": queries.nbytes,
        "output_bytes": count * 32,
    }
    (output / "batch.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
