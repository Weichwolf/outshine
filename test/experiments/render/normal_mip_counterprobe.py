"""Finite-LOD normal envelope for fixed pixel-centre UVs and a declared TBN.

Exact rational stationary points cover the ideal trilinear model. This is not a
GPU conformance test: actual UV/TBN, sampler rounding and undefined operations
must be investigated separately. Input images are native decoded bytes.
"""

import argparse
import hashlib
import json
import tempfile
import time
from fractions import Fraction as F
from itertools import pairwise
from pathlib import Path

import numpy as np
from moment_codec import sample

NORMAL_ERROR_SQUARED = F(48, 255 * 255)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def segment_candidates(a, b, target):
    """Endpoints and any isolated interior stationary direction on a segment."""
    a, b, target = (tuple(map(F, v)) for v in (a, b, target))
    d = tuple(y - x for x, y in zip(a, b, strict=True))
    aa, ad, dd = dot(a, a), dot(a, d), dot(d, d)
    minimum_t = max(F(0), min(F(1), -ad / dd)) if dd else F(0)
    if aa + 2 * minimum_t * ad + minimum_t * minimum_t * dd == 0:
        return None
    if not dot(target, target):
        return None
    A, D = dot(a, target), dot(d, target)
    denominator = D * ad - A * dd
    candidates = [F(0), F(1)]
    if denominator:
        t = (A * ad - D * aa) / denominator
        if 0 < t < 1:
            candidates.append(t)
    result = []
    cosine_limit = 1 - NORMAL_ERROR_SQUARED / 2
    for t in candidates:
        q = tuple(x + t * dx for x, dx in zip(a, d, strict=True))
        product = dot(q, target)
        norm_product = dot(q, q) * dot(target, target)
        cosine = (1 if product >= 0 else -1) * np.sqrt(
            float(product * product / norm_product)
        )
        outside = (
            product < 0
            or product * product < cosine_limit * cosine_limit * norm_product
        )
        result.append(
            {
                "weight": str(t),
                "fraction": float(t),
                "unit_error": float(np.sqrt(max(0, 2 - 2 * np.clip(cosine, -1, 1)))),
                "outside_unchanged_normal_bound": bool(outside),
            }
        )
    return result


def mip_vectors(levels, x, y, basis):
    """Exact UNORM8 weights at centres of a bounded power-of-two base image."""
    size = len(levels[0])
    if (
        size < 1
        or size > 4096
        or size & (size - 1)
        or not (0 <= x < size and 0 <= y < size)
    ):
        raise ValueError("bounded square power-of-two image and pixel required")
    if len(levels) != size.bit_length():
        raise ValueError("complete mip chain required")
    matrix = np.asarray(basis, dtype=float)
    if (
        matrix.shape != (3, 3)
        or not np.isfinite(matrix).all()
        or (abs(matrix) > 2).any()
    ):
        raise ValueError("bounded finite tangent-to-world basis required")
    basis = [[F(float(c)) for c in row] for row in matrix]
    vectors = []
    for k, image in enumerate(levels):
        n = size >> k
        if image.shape != (n, n, 4) or image.dtype != np.uint8:
            raise ValueError("complete RGBA8 normal levels required")
        pixels, weights = sample(image, [(x + 0.5) / size, (y + 0.5) / size])
        encoded = [
            sum(
                F(int(p[c]), 255) * F(float(w))
                for p, w in zip(pixels, weights, strict=True)
            )
            for c in range(3)
        ]
        tangent = [2 * v - 1 for v in encoded]
        vectors.append(tuple(dot(row, tangent) for row in basis))
    return vectors


def analyse(levels, before, after, basis):
    size = len(levels[0])
    if before.shape != (size, size, 4) or after.shape != before.shape:
        raise ValueError("matching four-channel normal readbacks required")
    if not np.isfinite(before).all() or not np.isfinite(after).all():
        raise ValueError("nonfinite readback is outside this finite-LOD model")
    if (before[:, :, 3] != after[:, :, 3]).any():
        raise ValueError("changed facing/coverage is outside the fixed-frame model")
    changed = (before[:, :, :3] != after[:, :, :3]).any(2)
    yy, xx = np.nonzero(changed)
    if len(xx) > 4096:
        raise ValueError("at most4096 changed pixels in one diagnostic")
    records = []
    for x, y in zip(xx, yy, strict=True):
        vectors = mip_vectors(levels, int(x), int(y), basis)
        rows = []
        for k, (a, b) in enumerate(pairwise(vectors)):
            candidates = segment_candidates(a, b, after[y, x, :3].astype(float))
            if candidates is None:
                raise ValueError(
                    "zero-vector segment leaves normal direction undefined"
                )
            rows.extend(dict(row, level=k + row["fraction"]) for row in candidates)
        baseline = segment_candidates(
            vectors[0], vectors[0], before[y, x, :3].astype(float)
        )
        if baseline is None:
            raise ValueError("before normal undefined")
        best = min(rows, key=lambda q: q["unit_error"])
        records.append(
            {
                "pixel_xy": [int(x), int(y)],
                "before": before[y, x].tolist(),
                "after": after[y, x].tolist(),
                "before_base_unit_error": baseline[0]["unit_error"],
                "before_within_bound": not baseline[0][
                    "outside_unchanged_normal_bound"
                ],
                "all_finite_lod_outside_bound": all(
                    q["outside_unchanged_normal_bound"] for q in rows
                ),
                "best_model_candidate": best,
                "stationary_and_endpoint_candidates": rows,
            }
        )
    return records


def plot_report(records, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(9.6, 5.4), layout="constrained")
    for row in records:
        candidates = row["stationary_and_endpoint_candidates"]
        ax.scatter(
            [c["level"] for c in candidates],
            [c["unit_error"] for c in candidates],
            label=f"After pixel {tuple(row['pixel_xy'])}",
        )
        ax.scatter(0, row["before_base_unit_error"], marker="x", color="green")
    ax.axhline(
        np.sqrt(float(NORMAL_ERROR_SQUARED)), color="red", label="Original normal bound"
    )
    ax.set(
        xlabel="Finite LOD candidate (endpoints and stationary directions)",
        ylabel="Unit-normal distance to recorded pixel",
        title="Native normal bytes: fixed UV / declared TBN control only",
    )
    ax.legend()
    fig.savefig(output / "finite-lod-control.png", dpi=120)
    plt.close(fig)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("base", "lower-mips", "before", "after", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--size", type=int, required=True)
    p.add_argument("--basis", type=float, nargs=9, required=True)
    p.add_argument("--plot", action="store_true")
    args = p.parse_args()
    n = args.size
    if n < 2 or n > 1024 or n & (n - 1):
        p.error("bounded square power-of-two image required")
    if not np.isfinite(args.basis).all() or (abs(np.array(args.basis)) > 2).any():
        p.error("finite declared tangent-to-world basis required")
    paths = [args.base, args.lower_mips, args.before, args.after]
    counts = [
        4 * n * n,
        sum(4 * (n >> k) ** 2 for k in range(1, n.bit_length())),
        16 * n * n,
        16 * n * n,
    ]
    for path, count in zip(paths, counts, strict=True):
        if path.stat().st_size != count:
            p.error("exact image/readback byte counts required")
    args.output = args.output.resolve()
    if not args.output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        p.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    levels = [np.fromfile(args.base, np.uint8).reshape(n, n, 4)]
    data = args.lower_mips.read_bytes()
    offset = 0
    for k in range(1, n.bit_length()):
        size = n >> k
        levels.append(
            np.frombuffer(data, np.uint8, count=4 * size * size, offset=offset).reshape(
                size, size, 4
            )
        )
        offset += 4 * size * size
    before, after = [np.fromfile(path, "<f4").reshape(n, n, 4) for path in paths[2:]]
    basis = np.array(args.basis).reshape(3, 3)
    began = time.perf_counter()
    records = analyse(levels, before, after, basis)
    analysis_ms = (time.perf_counter() - began) * 1000
    if args.plot:
        plot_report(records, args.output)
    result = {
        "input_sha256": {
            str(path): hashlib.sha256(memoryview(buffer)).hexdigest()
            for path, buffer in zip(
                paths, (levels[0], data, before, after), strict=True
            )
        },
        "input_array_bytes": sum(counts),
        "analysis_ms": analysis_ms,
        "segments": len(records) * (len(levels) - 1),
        "direction_candidates": sum(
            len(row["stationary_and_endpoint_candidates"]) for row in records
        ),
        "cost_scope": "Input arrays/read bytes and finite work counted; Fraction allocations/bit costs, sampler/device timing and whole-process peak unmeasured.",
        "size": n,
        "declared_basis": basis.tolist(),
        "original_normal_error_squared": str(NORMAL_ERROR_SQUARED),
        "records": records,
        "scope": "Ideal non-anisotropic finite-LOD linear sampling, fixed pixel-centre UV and declared constant TBN; exact rational exclusion for that model only. Actual UV/TBN, sampling precision, nonfinite/undefined derivatives, GPU image lifetime and device behaviour remain unproved. No native rerender or shader changes.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    for row in records:
        print(
            row["pixel_xy"],
            row["before_base_unit_error"],
            row["best_model_candidate"],
            row["all_finite_lod_outside_bound"],
        )


if __name__ == "__main__":
    main()
