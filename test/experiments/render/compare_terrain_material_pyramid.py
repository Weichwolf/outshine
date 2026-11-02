"""Static material-cell hierarchy under movement, rotated footprints and LOD changes."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import time
from functools import partial
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from compare_terrain_material_filter import error
from probe_terrain_material_substrate import optical_row
from relief_shading_reference import direction
from terrain_material_filter import distribution, response
from terrain_material_litter import compose
from terrain_material_pyramid import bake, filtered_components


def probes(evaluate, origin, extent):
    lights = np.array(
        [
            direction(angle, azimuth)
            for azimuth in (0, 90, 180, 270)
            for angle in (0, 30, 60, 80)
        ]
    )
    views = np.array([direction(0), direction(70, 37)])
    records = []
    for fraction in (0.49, 0.51):
        at = origin + [fraction * extent, 0.5 * extent]
        for divisor in (16, 6):
            footprint = extent / divisor
            for angle, anisotropy in ((0, 1), (0.731, 1), (0.731, 16)):
                dx, dy = np.array(
                    [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
                ) * [[footprint], [footprint / anisotropy]]
                fields = [
                    distribution(evaluate, at, dx, dy, order) for order in (64, 128)
                ]
                for view in views:
                    coarse, fine = [
                        response(raw, weights, lights, view) for raw, weights in fields
                    ]
                    records.append(
                        {
                            "at": at,
                            "dx": dx,
                            "dy": dy,
                            "angle": angle,
                            "anisotropy": anisotropy,
                            "view": view,
                            "lights": lights,
                            "reference": fine,
                            "reference_delta": error(fine, coarse),
                        }
                    )
    return records


def compare(capture):
    palette, layout, receipt = load_capture(capture)
    anchor = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    records = []
    for row in (0, 3, 6, 10, 11, 12):
        evaluate = partial(compose, **optical_row(palette, layout, row))
        for extent in (0.0064, 6.4):
            origin = np.floor(anchor / extent) * extent
            references = probes(evaluate, origin, extent)
            for side in (32, 64):
                start = time.perf_counter_ns()
                pyramid = bake(evaluate, origin, extent, side)
                build_ms = (time.perf_counter_ns() - start) / 1e6
                for probe in references:
                    lengths = np.linalg.norm([probe["dx"], probe["dy"]], axis=-1)
                    for selection, size in (
                        ("major", max(lengths)),
                        ("minor", min(lengths)),
                        ("minor_quarter", min(lengths) / 4),
                    ):
                        lod = float(
                            np.clip(
                                np.log2(size / (extent / side)),
                                0,
                                len(pyramid.levels) - 1,
                            )
                        )
                        start = time.perf_counter_ns()
                        fields, weights, cells = filtered_components(
                            pyramid, probe["at"], probe["dx"], probe["dy"], lod
                        )
                        read_ms = (time.perf_counter_ns() - start) / 1e6
                        actual = response(
                            fields, weights, probe["lights"], probe["view"]
                        )
                        records.append(
                            {
                                "row": row,
                                "extent_m": extent,
                                "side": side,
                                "origin_m": origin.tolist(),
                                "at_m": probe["at"].tolist(),
                                "dx_m": probe["dx"].tolist(),
                                "dy_m": probe["dy"].tolist(),
                                "view": probe["view"].tolist(),
                                "lod": lod,
                                "selection": selection,
                                "retained_bytes": pyramid.retained_bytes,
                                "build_ms_python": build_ms,
                                "query_ms_python": read_ms,
                                "selected_cells": cells,
                                "representatives": len(weights),
                                "error": error(probe["reference"], actual),
                                "reference_delta_64_128": probe["reference_delta"],
                                "reference_rgb": probe["reference"].tolist(),
                                "rgb": actual.tolist(),
                            }
                        )
    return receipt, records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private comparison output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    receipt, records = compare(args.capture)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "python": sys.version,
        "numpy": np.__version__,
        "palette": receipt["palette"],
        "native_commit": receipt["native_commit"],
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "terrain_material_pyramid.py",
                "terrain_material_filter.py",
                "terrain_material_litter.py",
                "terrain_material_substrate.py",
                "terrain_material_plane.py",
                "relief_shading_reference.py",
                "asphalt_field.py",
            )
        },
        "limitations": [
            "Fixed metric tiles, current bound substrate/litter recipes; no actual native fragment positions.",
            "Fine point grid and compressed parent distributions are both approximations.",
            "No native class/rock/sky model or displaced visibility.",
            "Exact Shapely cell areas are a reference query, not a runtime GPU implementation.",
            "Major/minor LOD selection costs and errors are compared without accepting either policy.",
            "Quadrature convergence differences are not a proven error bound.",
            "No persistent native format, AssetDB integration, streaming/residency or frame-time proof.",
        ],
        "records": records,
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "comparison.json")


if __name__ == "__main__":
    main()
