"""Inspect actual region-relative Float32 encoding without changing logical XYZ."""

import argparse
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def encode(points, origin):
    points, origin = np.asarray(points, float), np.asarray(origin, float)
    if (
        points.ndim != 2
        or points.shape[1] not in (2, 3)
        or not 1 <= len(points) <= 65536
        or origin.shape != (points.shape[1],)
        or not np.isfinite(points).all()
        or not np.isfinite(origin).all()
    ):
        raise ValueError(
            "bounded finite positions and explicit matching origin required"
        )
    encoded = np.empty(points.shape, dtype="f4")
    np.subtract(points, origin, out=encoded)
    if not np.isfinite(encoded).all():
        raise ValueError("relative Float32 positions must remain finite")
    reconstructed = encoded.astype(float)
    reconstructed += origin
    return encoded, reconstructed


def inspect(points, faces, origin):
    encoded, actual = encode(points, origin)
    triangles = actual[faces]
    a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    signed = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    return {
        "origin_m": origin.tolist(),
        "vertices": len(points),
        "faces": len(faces),
        "nonpositive_faces": int(np.count_nonzero(signed <= 0)),
        "minimum_signed_double_area_m2": float(signed.min()),
        "merged_vertices": len(points) - len(np.unique(actual, axis=0)),
        "coverage_valid": bool(shapely.coverage_is_valid(shapely.polygons(triangles))),
        "maximum_vertex_displacement_m": float(
            np.linalg.norm(actual - points, axis=1).max()
        ),
        "encoded_position_bytes": encoded.nbytes,
        "origin_bytes": origin.nbytes,
        "diagnostic_reconstruction_bytes": actual.nbytes,
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    with np.load(args.input, allow_pickle=False) as data:
        points, faces = data["xy"], data["faces"]
    if points.shape[1] != 2 or not 1 <= len(faces) <= 32768:
        raise ValueError("bounded two-dimensional terrain topology input required")
    origin = np.round((points.min(axis=0) + points.max(axis=0)) / 128) * 64
    report = {
        "commit": commit,
        "input_sha256": digest(args.input),
        "absolute": inspect(points, faces, np.zeros(2)),
        "regional": inspect(points, faces, origin),
        "scope": "Same logical binary64 XY; explicit relative Float32 encoding and diagnostic binary64 reconstruction. No mixed-region seam, height/volume, native GPU or world admission proof.",
    }
    args.output.mkdir()
    write_json(args.output / "comparison.json", report)
    print(report)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
