"""Held-out centroid/edge DEM probes shared by every accepted height objective."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from capture_height_targets import sample_targets

BARYCENTRIC = np.array(
    [[1 / 3, 1 / 3, 1 / 3], [0.5, 0.5, 0], [0, 0.5, 0.5], [0.5, 0, 0.5]]
)


def errors(values, targets, areas):
    delta = abs(values - targets)
    return {
        "probe_count": delta.size,
        "sampled_rms_m": float(np.sqrt(np.mean(delta**2))),
        "sampled_area_weighted_rms_m": float(
            np.sqrt(np.average(np.mean(delta**2, axis=1), weights=areas))
        ),
        "sampled_p95_m": float(np.percentile(delta, 95)),
        "sampled_maximum_m": float(delta.max()),
    }


def check(root, name, sources):
    path = root / "inputs" / (name + "-input.npz")
    meta = json.loads(path.with_suffix(".json").read_text())
    if digest(path) != meta["input_sha256"]:
        raise ValueError("held-out comparison input changed")
    with np.load(path) as d:
        xy, faces = d["xy"], d["faces"]
    points = xy[faces]
    locations = np.einsum("pv,fvd->fpd", BARYCENTRIC, points)
    targets, receipts = sample_targets(
        meta["origin_lat_lon"], locations.reshape(-1, 2), sources, 11
    )
    expected = {r["url"]: r["sha256"] for r in meta["dem_receipts"]}
    if any(expected.get(r["url"]) != r["sha256"] for r in receipts):
        raise ValueError("held-out DEM differs from fit source")
    targets = targets.reshape(len(faces), 4)
    a, b = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    areas = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
    rows = {
        "flat": errors(np.full_like(targets, meta["fixed_datum_m"]), targets, areas)
    }
    for mode in ("minimax", "least_squares"):
        folder = root / (name + "-" + mode)
        row = json.loads((folder / (name + "-" + mode + ".json")).read_text())
        if not row["valid"]:
            rows[mode] = {"accepted": False, "reason": row["reason"]}
            continue
        candidate = folder / (name + "-" + mode + ".npz")
        if digest(candidate) != row["mesh_sha256"]:
            raise ValueError("held-out candidate mesh changed")
        with np.load(candidate) as d:
            if not np.array_equal(xy, d["xy"]) or not np.array_equal(faces, d["faces"]):
                raise ValueError("same whole bed domain required")
            values = np.einsum("pv,fv->fp", BARYCENTRIC, d["heights"][faces])
        rows[mode] = {
            "accepted": True,
            "mesh_sha256": row["mesh_sha256"],
            **errors(values, targets, areas),
        }
    return {
        "place": name,
        "input_sha256": digest(path),
        "cases": rows,
        "scope": "Same retained coarse DEM, four held-out probes per entire face; barycentric evaluation of saved finite meshes. Sampled source errors are not whole-face DEM bounds or observations of actual roads.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--place", required=True)
    parser.add_argument("--dem-sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh held-out report required")
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    result = check(args.run, args.place, args.dem_sources)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
