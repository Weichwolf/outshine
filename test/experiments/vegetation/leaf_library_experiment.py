"""Migrate exact leaf procedures, compare spectral costs and persist shader controls."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from leaf_library_cache import export_materials
from leaf_library_scenes import scenes
from leaf_pigment_optics import direct_colour
from outshine_ref.vegetation import LeafOpticsTable, leaf_diffuse_response
from outshine_ref.vegetation.leaf_normals import (
    mip_pyramid,
    resolve_moments,
    slope_moments,
    tangent_slopes,
)
from outshine_ref.vegetation.leaf_optics import gamut_map, lookup, optical_mixture
from outshine_ref.vegetation.leaf_pigments import pigment_coordinates
from PIL import Image

OUTPUT = Path("build/experiments/vegetation/leaf-library")


def baseline_checks():
    with np.load(OUTPUT / "before-material-kernel.npz") as saved:
        results = {
            "pigment": pigment_coordinates(
                saved["points"], saved["ridge"], 121, 45, 0.2
            ),
            "raw": lookup(saved["table"], saved["pigment"]),
            "tangent": tangent_slopes(
                saved["du"], saved["dv"], saved["hu"], saved["hv"]
            ),
            "moments": slope_moments(saved["slopes"], saved["rough"]),
        }
        results["optical"], results["scale"] = gamut_map(results["raw"])
        for i, moments in enumerate(mip_pyramid(results["moments"])):
            results[f"level-{i}"] = moments
            for name, value in zip(
                ("normal", "roughness", "covariance"), resolve_moments(moments)
            ):
                results[f"{name}-{i}"] = value
        for name, value in results.items():
            assert np.array_equal(value, saved[name]), name
        assert np.array_equal(
            LeafOpticsTable(saved["table"]).sample(saved["pigment"]), saved["optical"]
        )
    return {"exact_arrays": len(results), "owned_table_exact": True}


def cached_maps(table):
    folder = Path("build/experiments/vegetation/pigment-leaves")
    cases = []
    for i in range(3):
        with np.load(folder / f"developed-{i}-optics.npz") as saved:
            optical = table.sample(saved["pigments"])
            assert np.array_equal(optical, saved["optical"])
        maps = optical_mixture(optical)
        for name, value in zip(
            ("colour", "transmittance", "transmission_weight"), maps
        ):
            shape = (512, 512, 3) if value.ndim == 2 else (512, 512)
            expected = np.rint(value.reshape(shape)[::-1] * 255).astype(np.uint8)
            with Image.open(folder / f"developed-{i}-{name}.png") as image:
                assert np.array_equal(expected, np.asarray(image)), name
        cases.append(
            {"state": i, "exact_optical_texels": len(optical), "exact_png_maps": 3}
        )
    return cases


def costs(table):
    rng = np.random.default_rng(841)
    points = rng.uniform((0, 0), (80, 10), (128, 2))
    direct = gamut_map(np.array([direct_colour(*p) for p in points]))[0]
    maximum = float(np.max(abs(direct - table.sample(points))))
    assert maximum <= 1 / 1024
    runs = {"spectral_ms": [], "table_ms": [], "table_and_sheet_ms": []}
    normals = np.tile((0, 0, 1), (len(points), 1))
    for i in range(5):
        order = ("spectral", "table") if i % 2 == 0 else ("table", "spectral")
        for mode in order:
            start = time.perf_counter()
            result = (
                gamut_map(np.array([direct_colour(*p) for p in points]))[0]
                if mode == "spectral"
                else table.sample(points)
            )
            runs[mode + "_ms"].append((time.perf_counter() - start) * 1000)
            np.testing.assert_array_equal(
                result, direct if mode == "spectral" else table.sample(points)
            )
        start = time.perf_counter()
        leaf_diffuse_response(table.sample(points), normals, (0, 0, 1), (0, 0.6, -0.8))
        runs["table_and_sheet_ms"].append((time.perf_counter() - start) * 1000)
    return {
        "same_queries": len(points),
        "alternating_runs": runs,
        "median_ms": {key: float(np.median(value)) for key, value in runs.items()},
        "table_vs_spectral_max_rgb_error": maximum,
        "table_bytes": table.values.nbytes,
    }


def shader_vectors(table):
    pigment = np.array(((45, 0.2), (8, 0.2), (10, 8)))
    optical = table.sample(pigment)
    z = np.linspace(-1, 1, 201)
    lights = np.column_stack((np.sqrt(1 - z * z), np.zeros(len(z)), z))
    response = np.stack(
        [
            leaf_diffuse_response(np.tile(o, (len(z), 1)), (0, 0, 1), (0, 0, 1), lights)
            for o in optical
        ]
    )
    np.savez(
        OUTPUT / "shader-vectors.npz",
        pigments=pigment,
        optical=optical,
        light_cosine=z,
        lights=lights,
        response=response,
    )
    return {
        "states": pigment.tolist(),
        "angles_per_state": len(z),
        "normal_orientation": "camera-facing geometric normal",
        "model": "reciprocal-diffuse-sheet-1",
        "response": "R*max(n.L,0)/pi + T*max(-n.L,0)/pi",
        "specular_included": False,
    }


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = Path("build/experiments/vegetation/pigment-leaves/pigment-table.npz")
    with np.load(source) as saved:
        table = LeafOpticsTable(saved["values"])
    (OUTPUT / "table.json").write_text(json.dumps(table.recipe(), indent=2) + "\n")
    report = {
        "baseline": baseline_checks(),
        "maps": cached_maps(table),
        "costs": costs(table),
        "shader": shader_vectors(table),
        "scenes": scenes(OUTPUT),
        "material_cache": export_materials(OUTPUT),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "input_sha256": {str(source): hashlib.sha256(source.read_bytes()).hexdigest()},
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    (OUTPUT / "leaf-library-report.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
