"""Replay only a frozen requested twig: unshadowed diffuse spatial filtering."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from needle_response import (
    exact_response,
    mean_response,
    quadratic_moments,
    quadratic_response,
)
from plant_atlas import projection
from raster_batch import rasterize


def samples(path, view, size):
    if path.stat().st_size > 16 * 1024 * 1024 or size != 256:
        raise ValueError(
            "selected frozen twig <=16MiB and fixed256-square probe required"
        )
    began = time.perf_counter()
    with np.load(path) as source:
        vertices, faces, normals = (source[k] for k in ("vertices", "faces", "normals"))
    frame, lower, upper, _ = projection(vertices, view)
    covered, attributes = rasterize(
        vertices @ frame, faces, normals, lower, upper, size
    )
    selected = attributes[covered].astype(float)
    lengths = np.linalg.norm(selected, axis=1)
    if np.any(lengths < 1e-10):
        raise ValueError("nonzero interpolated source normals required")
    attributes = attributes.astype(float)
    attributes[covered] = selected / lengths[:, None]
    return (
        covered,
        attributes,
        {
            "triangles": len(faces),
            "source_array_bytes": vertices.nbytes + faces.nbytes + normals.nbytes,
            "covered_samples": int(covered.sum()),
            "sample_grid": [size, size],
            "retained_sample_bytes": covered.nbytes + attributes.nbytes,
            "sample_wall_s": time.perf_counter() - began,
            "source_sha256": source_digest(path),
        },
    )


def source_digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def compare(covered, normals, directions, block):
    size = len(covered)
    if size % block:
        raise ValueError("integer disjoint spatial footprints required")
    count = size // block
    fields = np.zeros((len(directions), 3, count, count))
    moments_bytes = 0
    began = time.perf_counter()
    for y in range(count):
        for x in range(count):
            ys, xs = (
                slice(y * block, (y + 1) * block),
                slice(x * block, (x + 1) * block),
            )
            mask = covered[ys, xs]
            n = normals[ys, xs][mask]
            w = np.full(len(n), 1 / (block * block))
            moments = quadratic_moments(n, w)
            moments_bytes += moments.nbytes
            for i, direction in enumerate(directions):
                fields[i, :, y, x] = (
                    exact_response(n, w, direction),
                    mean_response(n, w, direction),
                    quadratic_response(moments, direction),
                )
    errors = fields[:, 1:] - fields[:, :1]
    return fields, {
        "native_size": [count, count],
        "source_samples_per_pixel": block * block,
        "quadratic_float64_field_bytes": moments_bytes,
        "mean_float64_field_bytes": count * count * 4 * 8,
        "max_abs_error_mean_quadratic": np.max(abs(errors), axis=(0, 2, 3)).tolist(),
        "rmse_mean_quadratic": np.sqrt(
            np.mean(errors * errors, axis=(0, 2, 3))
        ).tolist(),
        "negative_quadratic_pixels": int((fields[:, 2] < 0).sum()),
        "comparison_wall_s": time.perf_counter() - began,
    }


def run(source, output):
    output = output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    view = np.array([1.0, 0.0, 0.35])
    view /= np.linalg.norm(view)
    directions = np.r_[view[None], -view[None], np.eye(3), -np.eye(3)]
    covered, normals, report = samples(source, view, 256)
    report.update(
        model="unshadowed-unit-albedo-positive-cosine-v1",
        shadows=False,
        source=str(source.resolve()),
        directions=directions.tolist(),
    )
    report["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
    ).strip()
    report["implementation_sha256"] = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in (
            "needle_response.py",
            "needle_response_study.py",
            "raster_batch.py",
        )
    }
    report["footprints"] = []
    for block in (4, 16, 64, 256):
        fields, facts = compare(covered, normals, directions, block)
        report["footprints"].append(facts)
        np.savez_compressed(output / f"native-{256 // block}.npz", response=fields)
        if block == 16:
            fig, axes = plt.subplots(3, 3, figsize=(16, 9), dpi=160)
            for row, index in enumerate((0, 1, 2)):
                for column, title in enumerate(
                    (
                        "source response before filtering",
                        "normalized mean normal",
                        "degree2 / nine scalars",
                    )
                ):
                    axes[row, column].imshow(
                        fields[index, column],
                        vmin=0,
                        vmax=1,
                        cmap="inferno",
                        interpolation="nearest",
                    )
                    axes[row, column].set_title(
                        f"{title}\nlight={np.round(directions[index], 3)}"
                    )
                    axes[row, column].axis("off")
            fig.suptitle(
                "Actual frozen twig / native16×16 / no shadows / common exposure"
            )
            fig.tight_layout()
            fig.savefig(output / "comparison.png")
            plt.close(fig)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.output)
