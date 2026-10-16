"""Compare masonry bump filters against independent screen-polygon integrals."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, WIDTH
from facade_footprint import native_profiles
from masonry_relief_filter import brick_gradient, filtered, normal
from masonry_relief_reference import exact

MAX_NORMAL_ERROR_DEG = 2.0
RMS_NORMAL_ERROR_DEG = 0.5
SIDES = (0, 1, 2, 4, 8)


def synthetic():
    profiles = native_profiles()
    profile = profiles[1]
    angle = 0.61
    u = np.array([np.cos(angle), np.sin(angle)])
    v = np.array([-u[1], u[0]])
    axes = (([1, 0], [0, 1]), (u, v), (u, v + 0.8 * u), (u, v * 0.02))
    points = np.random.default_rng(387).uniform(-0.4, 0.4, (4, 2))
    points = np.concatenate([points, [[0.005, 0.04], [0.005, 0.005]]])
    for orientation, (horizontal, vertical) in enumerate(axes):
        for width in (0.0002, 0.002, 0.01, 0.06, 0.25):
            dx_m, dy_m = width * np.array(horizontal), width * np.array(vertical)
            for index, point in enumerate(points):
                for masked in (False, True):
                    at = np.concatenate([point / profile[:2] + [0.26, 0.235], point])
                    dx = np.concatenate([dx_m / profile[:2], dx_m])
                    dy = np.concatenate([dy_m / profile[:2], dy_m])
                    yield {
                        "name": f"synthetic-{orientation}-{width}-p{index}-mask{int(masked)}",
                        "at": at,
                        "dx": dx,
                        "dy": dy,
                        "fractions": profile[2:] if masked else np.zeros(2),
                        "per_m": 1 / profile[1] if masked else 0,
                    }


def native_cases(source):
    if source is None:
        return
    names = sorted(source.glob("*-d0.f32"))
    if not names:
        raise ValueError(
            "native input must contain actual MRT coordinates and mask records"
        )
    for path in names:
        coordinate = np.fromfile(path, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
        settings = np.fromfile(
            path.with_name(path.name.replace("-d0.", "-d1.")), dtype="<f4"
        ).reshape(3, HEIGHT, WIDTH, 4)
        chosen = {(y, x) for y in (17, 41, 65, 89) for x in (13, 47, 81, 115, 149)}
        for y, x in sorted(chosen):
            at, dx, dy = coordinate[:, y, x].astype(float)
            yield {
                "name": f"{path.stem}-{x}-{y}",
                "at": at,
                "dx": dx,
                "dy": dy,
                "fractions": settings[1, y, x, 1:3].astype(float),
                "per_m": float(settings[1, y, x, 0]),
            }


def counterexamples():
    profile = native_profiles()[1]
    fractions, per_m = profile[2:], 1 / profile[1]
    for angle in (0, 0.61, 1.19):
        u = np.array([np.cos(angle), np.sin(angle)])
        v = np.array([-u[1], u[0]])
        for ratio in (1, 1 / 64, 1 / 1024):
            dx_m, dy_m = 0.01 * u, 0.01 * v * ratio
            dx = np.concatenate([dx_m / profile[:2], dx_m])
            dy = np.concatenate([dy_m / profile[:2], dy_m])
            for offset in np.linspace(-0.006, 0.006, 25):
                point = np.array([0.005 + offset, 0.04])
                uv = np.array([0.26 + offset / profile[0], 0.4])
                yield {
                    "name": f"edge-motion-{angle}-{ratio}-{offset}",
                    "at": np.concatenate([uv, point]),
                    "dx": dx,
                    "dy": dy,
                    "fractions": fractions,
                    "per_m": per_m,
                }
    for label, point in (("pane", [0.005, 0.005]), ("flat-edge", [0.12, 0.04])):
        uv = np.array([0.5, 0.5]) if label == "pane" else np.array([0.26, 0.4])
        yield {
            "name": label,
            "at": np.concatenate([uv, point]),
            "dx": np.array([0.002 / profile[0], 0, 0.002, 0]),
            "dy": np.array([0, 0.002 / profile[1], 0, 0.002]),
            "fractions": fractions,
            "per_m": per_m,
        }
    for dx_m, dy_m in (([0, 0], [0, 0]), ([0.01, 0], [0.02, 0])):
        yield {
            "name": f"degenerate-{dx_m}-{dy_m}",
            "at": np.array([0.1, 0.1, 0.005, 0.04]),
            "dx": np.concatenate([np.array(dx_m) / profile[:2], dx_m]),
            "dy": np.concatenate([np.array(dy_m) / profile[:2], dy_m]),
            "fractions": fractions,
            "per_m": per_m,
        }
    for anchor in ([0, 0], [512, 8], [8192, 128]):
        for point in ([0.005, 0.005], [0.005, 0.04], [0.12, 0.04]):
            for width in (0.002, 0.01):
                yield {
                    "name": f"coordinate-{anchor}-{point}-{width}",
                    "at": np.r_[0.1, 0.1, np.array(point) + anchor],
                    "dx": np.array([0, 0, width, 0]),
                    "dy": np.array([0, 0, 0, width]),
                    "fractions": np.zeros(2),
                    "per_m": 0,
                }
    yield {
        "name": "whole-period",
        "at": np.array([0.1, 0.1, 0.005, 0.005]),
        "dx": np.array([0, 0, 0.25, 0]),
        "dy": np.array([0, 0, 0, 0.16]),
        "fractions": np.zeros(2),
        "per_m": 0,
    }


def controls():
    checked = 0
    for width in (0, 0.00001, 0.002, 0.08, 0.25, 0.5, 2.0):
        for x in (0.001, 0.0041, 0.005, 0.0061, 0.12, 0.2449, 0.249):
            at = np.array([0, 0, x, 0.04])
            dx, dy = np.array([0, 0, width, 0]), np.array([0, 0, 0, 1e-6])
            reference, _ = exact(at, dx, dy, np.zeros(2), 0)
            actual = brick_gradient(at[None, 2:], np.array([[width, 1e-6]]))[0]
            np.testing.assert_allclose(actual, reference, rtol=0, atol=1e-9)
            checked += 1
    return checked


def chart(output, rows):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(2, 1, figsize=(13, 8), constrained_layout=True)
    for side in SIDES:
        error = [x[f"side{side}"]["normal_error_deg"] for x in rows]
        label = "Point" if side == 0 else f"{side}x{side} analytic subcells"
        axes[0].plot(error, label=label)
        axes[1].plot([x[f"side{side}"]["gradient_error"] for x in rows], label=label)
    axes[0].axhline(MAX_NORMAL_ERROR_DEG, color="black", linestyle=":")
    axes[0].set_ylabel("Resolved mean-gradient normal error (degrees)")
    axes[1].set_ylabel("Material-gradient error (metre/metre)")
    axes[1].set_xlabel("Same synthetic / actual native pixel inputs")
    axes[0].legend(ncol=3)
    figure.suptitle(
        "Declared2mm bevel/depth; independent polygon integral, not a BRDF reference"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    began = time.monotonic()
    checked = controls()
    inputs = [*synthetic(), *native_cases(args.native_input), *counterexamples()]
    at, dx, dy, fractions = [
        np.array([x[k] for x in inputs]) for k in ("at", "dx", "dy", "fractions")
    ]
    per_m = np.array([x["per_m"] for x in inputs])
    np.savez(
        output / "inputs.npz", at=at, dx=dx, dy=dy, fractions=fractions, per_m=per_m
    )
    references, work = [], []
    for item in inputs:
        result, count = exact(
            item["at"], item["dx"], item["dy"], item["fractions"], item["per_m"]
        )
        references.append(result)
        work.append(count)
    reference = np.array(references)
    reference.tofile(output / "reference.f64")
    rows = [
        {
            "case": item["name"],
            "reference": reference[i].tolist(),
            "reference_work": work[i],
        }
        for i, item in enumerate(inputs)
    ]
    methods = []
    for side in SIDES:
        started = time.monotonic()
        actual = filtered(at, dx, dy, fractions, per_m, side)
        elapsed = time.monotonic() - started
        assert np.isfinite(actual).all() and np.abs(actual).max() <= 1.00000001
        np.testing.assert_array_equal(
            filtered(at, dx, dy, fractions, per_m, side, depth=0), 0 * actual
        )
        difference = normal(actual, dx[:, 2:], dy[:, 2:]) - normal(
            reference, dx[:, 2:], dy[:, 2:]
        )
        errors = np.rad2deg(
            2 * np.arcsin(np.minimum(np.linalg.norm(difference, axis=-1) / 2, 1))
        )
        gradient_errors = np.linalg.norm(actual - reference, axis=-1)
        for i, row in enumerate(rows):
            row[f"side{side}"] = {
                "normal_error_deg": float(errors[i]),
                "gradient_error": float(gradient_errors[i]),
            }
            if row["case"] in ("pane", "flat-edge"):
                np.testing.assert_array_equal(actual[i], np.zeros(2))
        rms = float(np.sqrt(np.mean(errors * errors)))
        maximum = float(errors.max())
        methods.append(
            {
                "side": side,
                "maximum_normal_deg": maximum,
                "rms_normal_deg": rms,
                "profile_boxes": len(at) * max(side, 1) ** 2,
                "cpu_seconds": elapsed,
                "accepted": maximum <= MAX_NORMAL_ERROR_DEG
                and rms <= RMS_NORMAL_ERROR_DEG,
            }
        )
        actual.tofile(output / f"side{side}.f64")
        print(methods[-1], flush=True)
    report = {
        "controls": checked,
        "cases": rows,
        "methods": methods,
        "chosen": next((r["side"] for r in methods if r["accepted"]), None),
        "limits": {
            "max_normal_deg": MAX_NORMAL_ERROR_DEG,
            "rms_normal_deg": RMS_NORMAL_ERROR_DEG,
        },
        "seconds": time.monotonic() - began,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "CPU study only; no native GPU/BRDF/world/frame claim",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    chart(output, rows)


if __name__ == "__main__":
    main()
