"""Compare authored angular asphalt sources/bakes without changing native materials."""

import argparse
import hashlib
import json
import resource
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from asphalt_angular import angular_tile, gaussian_tile, mip_fields
from asphalt_field import ALBEDO, ROUGHNESS, parameters
from asphalt_filter import srgb

ROOT = Path(__file__).resolve().parents[3]


def recipes():
    path = ROOT / "src/assets/world/ground-materials.json"
    data = json.loads(path.read_text())
    world = next(x for x in data["classes"] if x["name"] == "asphalt")
    return (
        ("route", ALBEDO, ROUGHNESS),
        ("catalogue", np.array(world["albedo"]), world["roughness"]),
    )


def compare_parameters(actual, reference):
    rows = []
    for name, base, roughness in recipes():
        colour, rough = parameters(actual, base, roughness)
        expected, expected_rough = parameters(reference, base, roughness)
        if (
            not np.isfinite(actual).all()
            or (colour < 0).any()
            or (colour > 1).any()
            or (rough < 0).any()
            or (rough > 1).any()
        ):
            raise ValueError("finite physical colour/roughness bounds required")
        ce = float(abs(srgb(colour) - srgb(expected)).max() * 255)
        re = float(abs(rough - expected_rough).max() * 255)
        rows.append(
            {
                "recipe": name,
                "colour_max_srgb_lsb": ce,
                "roughness_max_steps": re,
                "passes": ce <= 1 and re <= 1,
            }
        )
    return rows


def compare_density(output, density, size):
    began = time.perf_counter()
    reference, work = angular_tile(16, density, size, "area")
    if abs(reference.mean()) > 1e-12:
        raise ValueError("exact periodic area integration lost its declared zero mean")
    rows = [{"method": "area", "seconds": time.perf_counter() - began, "work": work}]
    selected = None
    for method in ("point", "ss8", "ss16", "ss32", "ss32-dc"):
        began = time.perf_counter()
        field, work = angular_tile(16, density, size, method)
        checks = compare_parameters(field, reference)
        rows.append(
            {
                "method": method,
                "seconds": time.perf_counter() - began,
                "work": work,
                "mean": float(field.mean()),
                "parameters": checks,
            }
        )
        if method == "ss32-dc":
            if not all(x["passes"] for x in checks):
                raise ValueError(
                    "selected finite bake exceeds the unchanged parameter limits"
                )
            selected = field
    levels = mip_fields(selected)
    exact_levels = mip_fields(reference)
    arrays, checks = {"exact_0": reference}, []
    for index, (level, exact) in enumerate(zip(levels, exact_levels, strict=True)):
        size = len(level)
        direct, _ = angular_tile(16, density, size, "area")
        residual = float(abs(direct - exact).max())
        if residual > 1e-12:
            raise ValueError(
                "nested box means differ from independent direct polygon integrals"
            )
        stored = level.astype("<f4")
        check = compare_parameters(stored, direct)
        if not all(x["passes"] for x in check):
            raise ValueError("stored scalar mip exceeds the unchanged parameter limits")
        arrays[f"field_{index}"] = stored
        checks.append(
            {
                "size": size,
                "direct_area_vs_nested_max": residual,
                "mean": float(stored.astype(float).mean()),
                "parameters": check,
            }
        )
    np.savez_compressed(output / f"density-{density}.npz", **arrays)
    return {
        "density": density,
        "methods": rows,
        "mips": checks,
        "stored_scalar_bytes": sum(
            a.nbytes for k, a in arrays.items() if k.startswith("field_")
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size", type=int, choices=(256, 512, 1024), default=512)
    args = parser.parse_args()
    output, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        output == temporary
        or not output.is_relative_to(temporary)
        or output.is_relative_to(ROOT)
        or output.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    output.mkdir(parents=True)
    gaussian = gaussian_tile(16, args.size)
    if abs(gaussian.mean()) > 1e-12:
        raise ValueError("periodic Gaussian control lost zero mean")
    np.savez_compressed(output / "gaussian.npz", field_0=gaussian.astype("<f4"))
    rows = []
    for density in (1, 4):
        row = compare_density(output, density, args.size)
        rows.append(row)
        print(json.dumps(row), flush=True)
    paths = [
        Path(__file__).with_name(p)
        for p in (
            "asphalt_angular.py",
            "asphalt_field.py",
            "asphalt_box_reference.py",
            "asphalt_filter.py",
            Path(__file__).name,
        )
    ]
    paths.append(ROOT / "src/assets/world/ground-materials.json")
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
        "recipe": {
            "version": "angular-asphalt-study-1",
            "period_cells": 16,
            "cell_pitch_m": 0.024,
            "tile_width_m": 0.384,
            "vertex_radius_cells": [0.14, 0.22],
            "selected_bake": "ss32-dc",
            "density_amplitude": "1/sqrt(sites_per_cell)",
            "uniform_binder": "analytic weighted polygon area mean, removed exactly; raster bias projected to known zero DC",
        },
        "cases": rows,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Periodic authored material sources and scalar box mips; not measured grain gradation, original nonperiodic Gaussian parity, native format/UV/cache/semantic hookup, arbitrary perspective/anisotropic sampling, exact integrated BRDF or world/device approval.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
