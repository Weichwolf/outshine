"""Positive normal-distribution approximation on the actual visible source measure."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from needle_joint_contacts import prepare_source
from needle_response_study import source_digest
from normal_lobe_plot import plot
from normal_lobes import (
    NormalMeasure,
    error_bound,
    evaluate,
    mass_preserving_bound,
    mass_preserving_vectors,
    quantization_bound,
)
from outshine_ref.vegetation import normal_distribution
from plant_atlas import projection
from surface_area_light import integrate


def angular_queries(view):
    random = np.random.default_rng(643).normal(size=(96, 3))
    random /= np.linalg.norm(random, axis=1)[:, None]
    return np.r_[view[None], -view[None], np.eye(3), -np.eye(3), random]


def coarsen(field, size):
    grid = field.reshape(size, size, *field.shape[1:])
    return (
        grid.reshape(size // 2, 2, size // 2, 2, *field.shape[1:])
        .mean((1, 3))
        .reshape((size // 2) ** 2, *field.shape[1:])
    )


def analyse(moments, exact, directions, size, arrays):
    facts = {"native_size": size, "representations": {}}
    total = moments[6].sum(axis=1)
    mass = total[:, 0]
    mean = total[:, 1:]
    length = np.linalg.norm(mean, axis=1)
    old = np.divide(
        mean * mass[:, None],
        length[:, None],
        out=np.zeros_like(mean),
        where=length[:, None] > 0,
    )
    for name, vectors in [
        ("normalized-mean", old[:, None]),
        ("single-first-moment", mean[:, None]),
        *[(str(count), field[..., 1:]) for count, field in moments.items()],
        *[
            (f"unit-{count}", mass_preserving_vectors(field))
            for count, field in moments.items()
        ],
    ]:
        response = evaluate(vectors, directions)
        delta = response - exact
        item = {
            "max_abs_delta": float(abs(delta).max()),
            "rmse": float(np.sqrt(np.mean(delta**2))),
            "maximum_overestimate": float(delta.max()),
            "frontal_max_abs_delta": float(abs(delta[0]).max()),
            "precomputed_vector_float32_bytes": vectors.size * 4,
            "precomputed_vector_float16_bytes": vectors.size * 2,
        }
        arrays[f"{size}-{name}"] = response
        item["isotropic_response_mean"] = float(
            np.linalg.norm(vectors, axis=-1).sum(axis=-1).mean() / 4
        )
        if name.startswith("unit-"):
            field = moments[int(name[5:])]
            bound = mass_preserving_bound(field)
            assert np.max(abs(delta) - bound) < 2e-12
            item["bound_max"] = float(bound.max())
            item["nominal_raw_moment_float16_bytes_unvalidated"] = field.size * 2
        if name.isdigit():
            field = moments[int(name)]
            bound = error_bound(field)
            if (
                np.max(response - exact) > 2e-12
                or np.max(exact - response - bound) > 2e-12
            ):
                raise AssertionError(
                    "positive first-moment theorem violated by actual source"
                )
            packed = vectors.astype(np.float16)
            quantized = evaluate(packed, directions)
            rounding = quantization_bound(vectors, packed)
            assert np.max(abs(quantized - response) - rounding) < 2e-12
            item.update(
                bound_max=float(bound.max()),
                float16_rounding_bound_max=float(rounding.max()),
                float16_max_abs_delta=float(abs(quantized - exact).max()),
            )
            arrays[f"{size}-moments-{name}"] = field
            arrays[f"{size}-packed16-{name}"] = packed
        facts["representations"][name] = item
    arrays[f"{size}-exact"] = exact
    return facts


def run(path, output, view_vector=(1.0, 0.0, 0.35)):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    view = np.asarray(view_vector, float)
    if view.shape != (3,) or not np.isfinite(view).all() or np.linalg.norm(view) == 0:
        raise ValueError("finite nonzero view required")
    view /= np.linalg.norm(view)
    output.mkdir(parents=True, exist_ok=False)
    source, scene = prepare_source(path)
    directions = angular_queries(view)
    frame = projection(source["vertices"], view)[0]
    measure = NormalMeasure(16, directions, frame)
    began = time.perf_counter()
    direct, work = integrate(
        source, scene, view, [view], size=16, order=4, normal_measure=measure
    )
    build = time.perf_counter() - began
    np.testing.assert_allclose(
        measure.exact[0].reshape(16, 16), direct[1], atol=2e-12, rtol=0
    )
    result = {
        "reference_kernel_sha256": source_digest(Path(normal_distribution.__file__)),
        "schema": "outshine-positive-normal-moments-study-v1",
        "source_sha256": source_digest(path),
        "source_owner": json.loads(path.with_name("source.json").read_text()),
        "view": view.tolist(),
        "partition_frame": frame.tolist(),
        "directions": directions.tolist(),
        "order": 4,
        "normal_samples": measure.sample_count,
        "source_work": work,
        "ray_work": scene.work,
        "build_wall_s": build,
        "levels": [],
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
    }
    result["implementation_sha256"] = {
        name: source_digest(Path(__file__).with_name(name))
        for name in (
            "normal_lobes.py",
            "normal_lobe_study.py",
            "normal_lobe_plot.py",
            "surface_area_light.py",
            "surface_area_cells.py",
        )
    }
    arrays = {"directions": directions}
    size = 16
    moments = measure.moments
    exact = measure.exact
    while True:
        result["levels"].append(analyse(moments, exact, directions, size, arrays))
        if size == 1:
            break
        moments = {count: coarsen(field, size) for count, field in moments.items()}
        exact = coarsen(exact.T, size).T
        size //= 2
    np.savez_compressed(output / "fields.npz", **arrays)
    plot(arrays, output / "comparison.png")
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["levels"][0]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view", nargs=3, type=float, default=(1.0, 0.0, 0.35))
    args = parser.parse_args()
    run(args.source, args.output, args.view)
