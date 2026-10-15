"""Independent saved-mesh, whole-gradient and complete fixed-rail-bed replay."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest, rail_footprints


def verify(inputs, result, rails, name, mode):
    meta = json.loads((inputs / (name + "-input.json")).read_text())
    report = json.loads((result / (name + "-" + mode + ".json")).read_text())
    path = inputs / (name + "-input.npz")
    if report["input_sha256"] != digest(path) or meta["input_sha256"] != digest(path):
        raise ValueError("saved source or report belongs to different prepared bed")
    if not report["valid"]:
        if (result / (name + "-" + mode + ".npz")).exists():
            raise ValueError("invalid height candidate published a mesh")
        return {
            "accepted": False,
            "reason": report["reason"],
            "original_bed_retained": True,
        }
    mesh = result / (name + "-" + mode + ".npz")
    if digest(mesh) != report["mesh_sha256"]:
        raise ValueError("saved height mesh changed")
    with np.load(path) as d:
        xy, faces, frames, fixed = (d[k] for k in ("xy", "faces", "frames", "fixed"))
    with np.load(mesh) as d:
        if not np.array_equal(xy, d["xy"]) or not np.array_equal(faces, d["faces"]):
            raise ValueError("height fitting changed source XY/topology")
        z = d["heights"]
    if z.shape != (len(xy),) or not np.isfinite(z).all():
        raise ValueError("incomplete saved height vector")
    p = xy[faces]
    equations = np.stack((p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1)
    delta = z[faces[:, 1:]] - z[faces[:, :1]]
    gradients = np.linalg.solve(equations, delta[..., None])[..., 0]
    normalized = np.linalg.norm(np.einsum("nij,nj->ni", frames, gradients), axis=1)
    if normalized.max() > 1 + 1e-7:
        raise ValueError("saved whole-face grade exceeds unchanged contract")
    datum = meta["fixed_datum_m"]
    if abs(z[fixed] - datum).max(initial=0) > 1e-7:
        raise ValueError("saved full rail-bed contacts moved")
    footprints, rail_datum, rail_binding = rail_footprints(
        rails, name, meta["source_binding"]["generation_sha256"]
    )
    if rail_datum != datum or rail_binding != meta["rail_binding"]:
        raise ValueError("rail support belongs to another generation/datum")
    # Rebuild the union from ALL actual unchanged-height output facets, without
    # trusting the producer's selected-facet list or just testing corner points.
    flat = np.all(abs(z[faces] - datum) <= 1e-7, axis=1)
    bed = shapely.coverage_union_all(shapely.polygons(xy[faces[flat]]))
    shapely.prepare(bed)
    if not shapely.covers(bed, footprints).all():
        raise ValueError("entire unchanged hardware lacks actual flat-bed support")
    return {
        "accepted": True,
        "maximum_normalized_grade": float(normalized.max()),
        "whole_rail_footprints": len(footprints),
        "unchanged_flat_facets": int(flat.sum()),
        "source_xy_and_faces_exact": True,
        "mesh_sha256": digest(mesh),
        "generation_commit": report["commit"],
        "generation_dirty": report["dirty"],
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--result", type=Path, required=True)
    p.add_argument("--rails", type=Path, required=True)
    p.add_argument("--place", required=True)
    p.add_argument("--mode", choices=("minimax", "least_squares"), required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    if args.output.exists():
        p.error("fresh independent report required")
    row = verify(args.inputs, args.result, args.rails, args.place, args.mode)
    args.output.write_text(json.dumps(row, indent=2) + "\n")
    print(json.dumps(row), flush=True)


if __name__ == "__main__":
    main()
