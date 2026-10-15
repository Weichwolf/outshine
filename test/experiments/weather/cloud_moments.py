"""Compare bounded extinction-depth estimators without a rendering backend."""

import argparse
import json
import resource
import struct
import sys
import time
from dataclasses import replace
from decimal import Decimal, localcontext
from pathlib import Path

import numpy as np
from cloud_field import Layer, density, segments


def centroid_fraction(optical):
    """Mean position within a constant Beer cell, as a fraction of its length."""
    optical = np.asarray(optical, dtype=np.float64)
    safe = np.maximum(optical, np.finfo(np.float64).tiny)
    opacity = np.maximum(-np.expm1(-optical), np.finfo(np.float64).tiny)
    exact = 1.0 / safe - np.exp(-optical) / opacity
    square = optical * optical
    series = (
        0.5
        - optical / 12.0
        + optical * square / 720.0
        - optical * square * square / 30240.0
    )
    return np.where(optical < 0.01, series, exact)


def integrate_cells(begin_m, length_m, optical, midpoint=False):
    """Integrate an ordered piecewise-constant extinction profile exactly per cell."""
    optical = np.asarray(optical, dtype=np.float64)
    prefix = np.concatenate(
        [np.zeros_like(optical[..., :1]), np.cumsum(optical[..., :-1], axis=-1)],
        axis=-1,
    )
    weights = np.exp(-prefix) * -np.expm1(-optical)
    centre = 0.5 if midpoint else centroid_fraction(optical)
    first_m = np.sum(weights * (begin_m + length_m * centre), axis=-1)
    return -np.expm1(-np.sum(optical, axis=-1)), first_m


def field_cells(origin, directions, layer, steps, seconds=37.0, maximum_m=np.inf):
    intervals = segments(origin, directions, layer)
    starts, lengths, depths = [], [], []
    for interval in range(2):
        begin = intervals[..., interval, 0]
        end = np.maximum(begin, np.minimum(intervals[..., interval, 1], maximum_m))
        length = (end - begin) / steps
        for sample in range(steps):
            start = begin + sample * length
            point = origin + (start + 0.5 * length)[..., None] * directions
            starts.append(start)
            lengths.append(length)
            depths.append(density(point, layer, seconds) * length)
    return (
        np.stack(starts, axis=-1),
        np.stack(lengths, axis=-1),
        np.stack(depths, axis=-1),
    )


def controls():
    errors = []
    with localcontext() as context:
        context.prec = 90
        for optical in np.geomspace(1e-14, 1000.0, 128):
            tau = Decimal(str(optical))
            decay = (-tau).exp()
            exact = float(1 / tau - decay / (1 - decay))
            errors.append(abs(float(centroid_fraction(optical)) - exact))
    assert max(errors) < 1e-11
    opacity, first = integrate_cells(
        np.array([1000.0]), np.array([100.0]), np.array([0.0])
    )
    assert opacity == first == 0.0
    opaque, front = integrate_cells(
        np.array([1000.0]), np.array([100.0]), np.array([1000.0])
    )
    assert opaque == 1.0 and abs(front - 1000.1) < 1e-10
    whole = integrate_cells(np.array([1000.0]), np.array([600.0]), np.array([3.0]))
    divided = integrate_cells(
        np.linspace(1000.0, 1590.0, 60), np.full(60, 10.0), np.full(60, 0.05)
    )
    np.testing.assert_allclose(divided, whole, rtol=0.0, atol=1e-12)
    two = integrate_cells(
        np.array([1000.0, 80000.0]), np.array([100.0, 600.0]), np.array([0.1, 2.0])
    )
    assert 1000.0 < two[1] / two[0] < 80600.0
    assert abs(two[0] - -np.expm1(-2.1)) < 1e-15
    raw_m1 = 80000.0 * 0.9
    try:
        struct.pack("e", raw_m1)
    except OverflowError:
        pass
    else:
        raise AssertionError("raw first moment in metres must expose the FP16 overflow")
    packed_km = struct.unpack("e", struct.pack("e", raw_m1 / 1000.0))[0]
    assert packed_km == 72.0
    weights = np.array([1.0, 0.1])
    depths = np.array([1000.0, 80000.0])
    filtered_depth = np.mean(weights * depths) / np.mean(weights)
    assert abs(filtered_depth - 8181.818181818182) < 1e-10
    assert abs(np.mean(depths) - filtered_depth) > 30000.0
    layer_weights = np.array([0.1, 0.8])
    layer_depths = np.array([100.0, 10000.0])
    pixel_scale = 960.0 * 50.0 / 30.0
    exact_motion = np.sum(layer_weights * pixel_scale / layer_depths) / np.sum(
        layer_weights
    )
    mean_depth = np.sum(layer_weights * layer_depths) / np.sum(layer_weights)
    mean_depth_motion = pixel_scale / mean_depth
    assert exact_motion - mean_depth_motion > 1.0
    return {
        "decimal_centroid_max_error": max(errors),
        "raw_fp16_metre_overflow": True,
        "homogeneous_subdivision_max_error": float(
            np.max(np.abs(np.array(divided) - whole))
        ),
        "unequal_opacity_filter_bias_m": float(np.mean(depths) - filtered_depth),
        "two_depth_mean_motion_error_px": float(exact_motion - mean_depth_motion),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"controls": controls(), "cases": []}
    random = np.random.default_rng(239)
    elevation = np.linspace(-0.999, 0.999, 128)
    azimuth = random.uniform(0.0, 2.0 * np.pi, len(elevation))
    horizontal = np.sqrt(1.0 - elevation * elevation)
    directions = np.stack(
        [horizontal * np.cos(azimuth), elevation, horizontal * np.sin(azimuth)], axis=-1
    )
    for name, height, cover, extinction in [
        ("below", 1772.0, 0.65, 0.012),
        ("inside", 2800.0, 0.65, 0.012),
        ("above", 4000.0, 0.65, 0.012),
        ("thin", 1772.0, 1.0, 0.001),
    ]:
        layer = replace(Layer(), cover=cover, extinction_per_m=extinction)
        origin = np.array([0.0, height, 0.0])
        reference = integrate_cells(*field_cells(origin, directions, layer, 1024))
        coarser = integrate_cells(*field_cells(origin, directions, layer, 512))
        active = reference[0] > 1e-8
        depth = reference[1][active] / reference[0][active]
        convergence = np.max(np.abs(coarser[1][active] / coarser[0][active] - depth))
        for steps in [1, 4, 16, 64, 256]:
            started = time.perf_counter()
            cells = field_cells(origin, directions, layer, steps)
            field_s = time.perf_counter() - started
            for midpoint in [True, False]:
                opacity, first = integrate_cells(*cells, midpoint=midpoint)
                usable = active & (opacity > 1e-8)
                error = np.abs(
                    first[usable] / opacity[usable]
                    - reference[1][usable] / reference[0][usable]
                )
                record = {
                    "case": name,
                    "steps": steps,
                    "midpoint": midpoint,
                    "maximum_depth_error_m": float(error.max(initial=0.0)),
                    "p99_depth_error_m": float(np.quantile(error, 0.99))
                    if len(error)
                    else 0.0,
                    "maximum_opacity_error": float(
                        np.max(np.abs(opacity - reference[0]))
                    ),
                    "missed_active_rays": int(np.count_nonzero(active & ~usable)),
                    "reference_512_to_1024_max_m": float(convergence),
                    "density_evaluations": int(cells[2].size),
                    "cell_arrays_bytes": sum(cell.nbytes for cell in cells),
                    "field_seconds": field_s,
                }
                report["cases"].append(record)
    report["scope"] = (
        "Unisolated declared CPU field; no backend, GPU cost, history or Place-image claim."
    )
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report["process_peak_rss_bytes"] = rss if sys.platform == "darwin" else rss * 1024
    (args.output / "moments.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["controls"]))
    for record in report["cases"]:
        if record["steps"] == 16:
            print(json.dumps(record))


if __name__ == "__main__":
    main()
