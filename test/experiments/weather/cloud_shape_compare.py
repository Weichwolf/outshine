"""Compare bounded shape candidates on original native sky rays and light inputs."""

import argparse
import hashlib
import json
import tempfile
import time
import tracemalloc
from dataclasses import asdict, replace
from pathlib import Path

import numpy as np
from cloud_field import Layer, density
from cloud_radiance import SKY_RADIANCE, SUN_IRRADIANCE
from cloud_shape_field import KINDS, Work, body_shape, extinction
from cloud_shape_transfer import transfer


def controls():
    layer = Layer(cover=0.65)
    points = np.random.default_rng(393).uniform(
        [-50000, 2500, -50000], [50000, 3300, 50000], (4096, 3)
    )
    for seconds in (0, 37, 97):
        np.testing.assert_array_equal(
            extinction(points, layer, seconds), density(points, layer, seconds)
        )
    advected = np.random.default_rng(1393).uniform(-20, 20, (4096, 3))
    heights = np.random.default_rng(2393).uniform(0, 1, 4096)
    for cover in (0.01, 0.2, 0.45, 0.65, 1):
        nine = body_shape(advected, heights, cover)
        exhaustive = body_shape(advected, heights, cover, neighborhood=2)
        np.testing.assert_array_equal(nine, exhaustive)
    for kind in KINDS:
        np.testing.assert_array_equal(
            extinction(points, replace(layer, cover=0), 37, kind), 0
        )
        value = extinction(points, layer, 37, kind)
        assert (
            np.all(np.isfinite(value))
            and np.min(value) >= 0
            and np.max(value) <= 1.5 * layer.extinction_per_m
        )
    return {
        "original_density_queries": 12288,
        "exhaustive_25_cell_queries": 20480,
        "passed": True,
    }


def load_capture(metadata, stride):
    info = json.loads(metadata.read_text())
    if not info.get("boundary_sky") or info["name"] != "sky-broken":
        raise ValueError("use the declared original native boundary-sky broken capture")
    h, w = info["height"], info["width"]
    prefix = str(metadata)[:-5]
    grid = np.memmap(prefix + ".rays.bin", dtype="<f8", mode="r", shape=(h, w, 11))
    image = np.memmap(prefix + ".images.bin", dtype="<f4", mode="r", shape=(5, h, w, 4))
    offset = stride // 2
    rays = np.array(grid[offset::stride, offset::stride]).reshape(-1, 11)
    pixels = np.array(image[:, offset::stride, offset::stride])
    parameters = np.fromfile(prefix + ".parameters.bin", "<f4")
    air, weather = parameters[:20], parameters[20:].reshape(-1, 4)
    np.testing.assert_allclose(weather[-2, :3], SUN_IRRADIANCE, atol=1e-7, rtol=0)
    np.testing.assert_allclose(weather[-1, :3], SKY_RADIANCE, atol=1e-7, rtol=0)
    layer = Layer(
        radius_m=float(weather[0, 0]),
        base_m=float(weather[0, 1]),
        thickness_m=float(weather[0, 2]),
        extinction_per_m=float(weather[0, 3]),
        cover=float(weather[1, 0]),
    )
    return info, rays, pixels, air, layer


def plot(rows, output, shape):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(3, len(KINDS), figsize=(17, 9), constrained_layout=True)
    for axis, row in zip(axes.flat, rows, strict=True):
        data = np.load(output / row["file"])
        rgb = data[:, :3].reshape(*shape, 3)
        rgb = np.maximum(4 * rgb, 0)
        axis.imshow(rgb / (1 + rgb))
        axis.set_title(
            f"{row['kind']} / cover={row['cover']:.2f} / t={row['seconds']:.0f}s"
        )
        axis.set_axis_off()
    fig.suptitle(
        "Same original native camera/air/light inputs; independent shape references\n"
        "Display4x/(1+4x); morphology alternatives, no GPU time or production choice"
    )
    fig.savefig(output / "cloud-shape-comparison.pdf")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stride", type=int, choices=(8, 16), default=16)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    control = controls()
    info, rays, pixels, medium, layer = load_capture(args.capture, args.stride)
    rows = []
    for cover, seconds in (
        (layer.cover, info["phase_s"]),
        (layer.cover, info["phase_s"] + 60),
        (0.65, info["phase_s"]),
    ):
        for kind in KINDS:
            work = Work()
            tracemalloc.start()
            before = time.perf_counter()
            result = transfer(
                rays, medium, replace(layer, cover=cover), seconds, kind, work
            )
            elapsed = time.perf_counter() - before
            live_bytes, peak_bytes = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            rgb = pixels[0, :, :, :3].reshape(-1, 3) * result[:, 3:] + result[:, :3]
            result = np.c_[rgb, result[:, 3:]]
            path = f"{kind}-{cover:.4f}-{seconds:.4f}.npy"
            np.save(output / path, result)
            baseline_error = None
            if (
                kind == "stratus"
                and cover == layer.cover
                and seconds == info["phase_s"]
            ):
                baseline_error = float(
                    np.max(np.abs(rgb - pixels[1, :, :, :3].reshape(-1, 3)))
                )
                if baseline_error > 1e-4:
                    raise AssertionError(
                        "the independent original control must match native RGB"
                    )
            rows.append(
                {
                    "kind": kind,
                    "cover": cover,
                    "seconds": seconds,
                    "file": path,
                    "work": asdict(work),
                    "python_wall_s_with_counters": elapsed,
                    "python_traced_live_bytes": live_bytes,
                    "python_traced_peak_bytes": peak_bytes,
                    "original_native_rgb_max_error": baseline_error,
                }
            )
            print(json.dumps(rows[-1]), flush=True)
    plot(rows, output, pixels.shape[1:3])
    record = {
        "capture": str(args.capture.resolve()),
        "capture_metadata_sha256": hashlib.sha256(
            args.capture.read_bytes()
        ).hexdigest(),
        "capture_inputs_sha256": {
            suffix: hashlib.sha256(
                args.capture.with_name(args.capture.stem + suffix).read_bytes()
            ).hexdigest()
            for suffix in (".images.bin", ".rays.bin", ".parameters.bin")
        },
        "reference_sources_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "cloud_shape_compare.py",
                "cloud_shape_field.py",
                "cloud_shape_transfer.py",
            )
        },
        "sampled_profile": list(pixels.shape[1:3][::-1]),
        "stride": args.stride,
        "controls": control,
        "cases": rows,
        "scope": "Offline FP64 references, tracing/counter costs included; not native timing/allocation counts",
    }
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")


if __name__ == "__main__":
    main()
