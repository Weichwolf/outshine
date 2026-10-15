"""Compare identical declared optical fields, independent of any engine/GPU claims."""

import argparse
import json
import time
import xml.etree.ElementTree as ET
from dataclasses import replace
from pathlib import Path

import numpy as np
from cloud_controls import controls
from cloud_field import Layer, transmittance
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]


def view_rays(width, height, bearing_deg, pitch_deg, fov_deg, *, pixel_centers=None):
    bearing, pitch = np.deg2rad([bearing_deg, pitch_deg])
    forward = np.array(
        [
            np.sin(bearing) * np.cos(pitch),
            np.sin(pitch),
            -np.cos(bearing) * np.cos(pitch),
        ]
    )
    right = np.array([np.cos(bearing), 0.0, np.sin(bearing)])
    up = np.cross(right, forward)
    if pixel_centers is None:
        x, y = np.meshgrid(
            (2.0 * (np.arange(width) + 0.5) / width - 1.0) * width / height,
            1.0 - 2.0 * (np.arange(height) + 0.5) / height,
        )
    else:
        x = (2.0 * pixel_centers[..., 0] / width - 1.0) * width / height
        y = 1.0 - 2.0 * pixel_centers[..., 1] / height
    direction = forward + np.tan(np.deg2rad(fov_deg) / 2.0) * (
        x[..., None] * right + y[..., None] * up
    )
    return direction / np.linalg.norm(direction, axis=-1, keepdims=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=240)
    parser.add_argument("--gpu-result", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    controls()
    scenario = ET.parse(ROOT / "src/assets/places/Koerbersee.scenario").getroot()
    at, view = scenario.find("views/view/at"), scenario.find("views/view")
    origin = np.array([0.0, float(at.get("heightM")), 0.0])
    width, height = args.width, args.width * 9 // 16
    direction = view_rays(
        width,
        height,
        float(at.get("bearingDeg")),
        float(at.get("pitchDeg")),
        float(view.get("fovDeg")),
    )
    layer = Layer()
    report = {
        "source": "src/assets/places/Koerbersee.scenario",
        "utc": scenario.find("clock").get("start"),
        "resolution": [width, height],
        "weather_origin": "Declared procedural cases; no weather observation is asserted.",
        "radius_origin": "Existing kEarthAir: 6360000 m; planetary alignment belongs to Light.",
        "cases": [],
        "scope": "Optical-depth integration only; Python wall time is not GPU time.",
    }
    panels = []
    sample_count = (1, 8, 16, 32, 256)
    for case, cover in (("overcast", 1.0), ("broken", 0.65)):
        declared = replace(layer, cover=cover)
        reference = transmittance(origin, direction, declared, 256)
        row = Image.new("RGB", (width * len(sample_count), height + 28), "#181818")
        draw = ImageDraw.Draw(row)
        for column, steps in enumerate(sample_count):
            before = time.perf_counter()
            measured = transmittance(origin, direction, declared, steps)
            elapsed_ms = (time.perf_counter() - before) * 1000.0
            error = np.abs(measured - reference)
            record = {
                "case": case,
                "steps": steps,
                "wall_ms": elapsed_ms,
                "rmse": float(np.sqrt(np.mean(error**2))),
                "p99_abs": float(np.quantile(error, 0.99)),
                "max_abs": float(error.max()),
            }
            report["cases"].append(record)
            diagnostic = np.uint8(np.clip(measured * 255.0, 0.0, 255.0))
            row.paste(Image.fromarray(diagnostic).convert("RGB"), (column * width, 28))
            draw.text((column * width + 5, 7), f"{case}: {steps} taps", fill="white")
        panels.append(row)
        if case == "broken" and args.gpu_result:
            measured = np.fromfile(args.gpu_result, dtype=np.float32).reshape(
                height, width
            )
            assert np.isfinite(measured).all() and np.all(
                (measured >= 0.0) & (measured <= 1.0)
            )
            same_steps = transmittance(origin, direction, declared, 16)
            error = np.abs(measured - same_steps)
            report["gpu_float32_vs_python_float64"] = {
                "samples": measured.size,
                "rmse": float(np.sqrt(np.mean(error**2))),
                "p99_abs": float(np.quantile(error, 0.99)),
                "max_abs": float(error.max()),
                "scope": "Same 16-sample field; submission/fence latency is not GPU time.",
            }
            assert error.max() < 0.0001
    canvas = Image.new("RGB", (panels[0].width, sum(panel.height for panel in panels)))
    for index, panel in enumerate(panels):
        canvas.paste(panel, (0, index * panel.height))
    canvas.save(args.output / "transmittance-comparison.png")
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    for record in report["cases"]:
        print(json.dumps(record))
    if args.gpu_result:
        print(json.dumps(report["gpu_float32_vs_python_float64"]))


if __name__ == "__main__":
    main()
