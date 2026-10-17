"""Cross-check saved primary coverage against raster coverage and Float64 rays."""

import argparse
import json
from pathlib import Path

import numpy as np
from needle_joint_contacts import camera_grid
from plant_atlas import projection
from raster_batch import rasterize
from scatter_reference import Float64Nearest


def run(source_path, product, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private verification output must be under/tmp")
    report = json.loads((product / "report.json").read_text())
    with np.load(source_path) as stored:
        source = {k: stored[k] for k in ("vertices", "faces", "normals")}
    reference = Float64Nearest(source["vertices"], source["faces"])
    results = []
    for index, facts in enumerate(report["views"]):
        size = report["size"]
        view = np.array(facts["view"])
        with np.load(product / f"view-{index}.npz") as arrays:
            pixels = arrays["pixels"]
            normals = arrays["normals"]
        frame, lower, upper, _ = projection(source["vertices"], view)
        mask, attributes = rasterize(
            source["vertices"] @ frame,
            source["faces"],
            source["normals"],
            lower,
            upper,
            size,
        )
        mask = mask.ravel()
        attributes = attributes.reshape(-1, 3)
        raymask = np.zeros(size * size, bool)
        raymask[pixels] = True
        differences = np.flatnonzero(mask != raymask)
        selected = np.unique(
            np.r_[differences, np.linspace(0, size * size - 1, 64, dtype=int)]
        )
        origins, direction, length = camera_grid(source, view, size)
        ids, _, tested = reference.query(
            origins[selected], np.broadcast_to(direction, (len(selected), 3)), length
        )
        expected = ids >= 0
        common = mask[pixels]
        raster_normals = attributes[pixels[common]].astype(float)
        raster_normals /= np.linalg.norm(raster_normals, axis=1)[:, None]
        results.append(
            {
                "view": index,
                "coverage_difference_pixels": differences.tolist(),
                "float64_probe_rays": len(selected),
                "float64_triangle_tests": tested,
                "product_coverage_disagreements": selected[
                    raymask[selected] != expected
                ].tolist(),
                "raster_coverage_disagreements": selected[
                    mask[selected] != expected
                ].tolist(),
                "common_normal_max_abs_delta": float(
                    abs(normals[common] - raster_normals).max(initial=0)
                ),
            }
        )
    output.write_text(
        json.dumps(
            {"case": str(product), "size": report["size"], "views": results}, indent=2
        )
        + "\n"
    )
    print(json.dumps(results))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.product, args.output)
