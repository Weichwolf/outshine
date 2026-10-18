"""Retain positive signed normal distributions on same-source unlit view fields."""

import argparse
import json
from pathlib import Path

import numpy as np
from foliage_selection import directions
from normal_lobes import NormalMeasure, error_bound, evaluate
from raster_batch import rasterize


def encode(source, frame, corners, lights, size=16):
    if type(size) is not int or size not in (4, 16, 32):
        raise ValueError("bounded native source field required")
    projected = source["vertices"] @ frame
    chart = corners @ frame
    lower, upper = chart[:, :2].min(axis=0), chart[:, :2].max(axis=0)
    mask, normals = rasterize(
        projected,
        source["faces"],
        source["normals"].reshape(-1, 3, 3),
        lower,
        upper,
        2 * size,
    )
    normals = normals[mask].astype(float)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    y, x = np.nonzero(mask)
    slots = (y // 2 * size + x // 2).astype(np.int32)
    measure = NormalMeasure(size, lights, frame, partitions=(4, 24))
    measure.add(slots, np.full(len(slots), 0.25), normals)
    rows = []
    for count, moments in measure.moments.items():
        predicted = evaluate(moments[..., 1:], lights)
        error = abs(predicted - measure.exact)
        bound = error_bound(moments)
        assert np.all(error <= bound[None] + 1e-12)
        rows.append(
            {
                "partitions": count,
                "maximum_unit_response_error": float(error.max()),
                "rms_unit_response_error": float(np.sqrt(np.mean(error**2))),
                "maximum_finite_measure_bound": float(bound.max()),
                "moment_array_bytes": moments.nbytes,
            }
        )
    mean = measure.moments[4].sum(axis=1, keepdims=True)
    error = abs(evaluate(mean[..., 1:], lights) - measure.exact)
    return (
        measure,
        rows,
        {
            "maximum_unit_response_error": float(error.max()),
            "rms_unit_response_error": float(np.sqrt(np.mean(error**2))),
        },
    )


def study(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    report = json.loads((inputs / "report.json").read_text())
    lights = directions(12)
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for node in report["nodes"]:
        folder = inputs / f"host-{node}"
        with np.load(folder / "source.npz", allow_pickle=False) as z:
            source = dict(z)
        with np.load(folder / "views64.npz", allow_pickle=False) as z:
            views = dict(z)
        for index in (0, 17, 63):
            measure, errors, mean = encode(
                source, views["frames"][index], views["vertices"][index], lights
            )
            coverage = measure.moments[4][..., 0].sum(axis=1).reshape(16, 16)
            np.testing.assert_array_equal(coverage, views["colour"][index, ..., 3])
            np.savez_compressed(
                output / f"host-{node}-view-{index}.npz",
                moments4=measure.moments[4],
                moments24=measure.moments[24],
                frame=views["frames"][index],
                coverage=coverage,
                exact_probe_response=measure.exact,
                source_leaf_ids=source["ids"],
                physical_leaf_area_m2=source["physical_leaf_area_m2"],
            )
            rows.append(
                {
                    "host_node": node,
                    "view": index,
                    "visible_samples": measure.sample_count,
                    "partitioned": errors,
                    "mean": mean,
                }
            )
    result = {
        "query_lights": lights.tolist(),
        "cases": rows,
        "scope": "Unlit same-view finite visible-normal measures, not baked lighting; supports scalar two-sided diffuse coefficients, not calibrated RGB/PBR/group RT, neighbour shadow or arbitrary-angle coverage.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    a = p.parse_args()
    study(a.inputs, a.output)
