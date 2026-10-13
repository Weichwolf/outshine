"""Cache physical leaf normal/roughness hierarchies from stable material products."""

import hashlib
import json
import os
import resource
import shutil
import time
from pathlib import Path

import numpy as np
import tifffile
from blade_surface import Blade
from leaf_normal_checks import direct_pyramid, normal_checks
from leaf_normal_moments import (
    mip_pyramid,
    normal_slopes,
    resolve_moments,
    slope_moments,
)
from PIL import Image


def export_level(output, name, moments):
    normal, roughness, covariance = resolve_moments(moments)
    _, naive, _ = resolve_moments(moments, broaden=False)
    files = {
        "normal": f"{name}-normal.tiff",
        "roughness": f"{name}-roughness.png",
        "naive_roughness": f"{name}-naive-roughness.png",
    }
    encoded = np.rint((normal[::-1] * 0.5 + 0.5) * 65535).astype(np.uint16)
    tifffile.imwrite(
        output / files["normal"], encoded, photometric="rgb", metadata=None
    )
    decoded = tifffile.imread(output / files["normal"])[::-1] / 65535 * 2 - 1
    decoded /= np.linalg.norm(decoded, axis=2)[..., None]
    error = float(np.max(np.arccos(np.clip(np.sum(normal * decoded, axis=2), -1, 1))))
    assert error < 6e-5
    for key, value in (("roughness", roughness), ("naive_roughness", naive)):
        Image.fromarray(np.rint(value[::-1] * 255).astype(np.uint8)).save(
            output / files[key]
        )
    return files, {
        "size": len(moments),
        "normal_quantization_radians": error,
        "mean_slope_variance": float(np.mean(covariance[..., 0] + covariance[..., 2])),
        "mean_roughness_increase": float(np.mean(roughness - naive)),
    }


def build(source, output, index):
    path = source / f"developed-{index}-material.json"
    recipe = json.loads(path.read_text())
    inputs = [
        path,
        source / recipe["maps"]["height"],
        source / recipe["maps"]["roughness"],
    ]
    height = np.asarray(Image.open(inputs[1]))[::-1] / 255 * recipe["height_m"]
    roughness = np.asarray(Image.open(inputs[2]))[::-1] / 255
    started = time.perf_counter()
    slopes = normal_slopes(Blade(**recipe["blade"]), height)
    base = slope_moments(slopes, roughness)
    slope_seconds = time.perf_counter() - started
    times, results = {"direct": [], "pyramid": []}, {}
    for repeat in range(3):
        for method in (
            ("direct", "pyramid") if repeat % 2 == 0 else ("pyramid", "direct")
        ):
            began = time.perf_counter()
            results[method] = (
                direct_pyramid(base) if method == "direct" else mip_pyramid(base)
            )
            times[method].append(time.perf_counter() - began)
    maximum = max(
        float(np.max(abs(a - b))) for a, b in zip(results["direct"], results["pyramid"])
    )
    assert maximum < 1e-13
    start = time.perf_counter()
    levels, cases = [], []
    for level, moments in enumerate(results["pyramid"]):
        files, case = export_level(output, f"developed-{index}-mip-{level}", moments)
        levels.append(files)
        cases.append(case)
    export_seconds = time.perf_counter() - start
    diagnostic_start = time.perf_counter()
    moments_file = output / f"developed-{index}-moments.npz"
    np.savez_compressed(
        moments_file,
        **{
            f"level_{i}": m.astype(np.float32) for i, m in enumerate(results["pyramid"])
        },
    )
    diagnostic_seconds = time.perf_counter() - diagnostic_start
    recipe["maps"] = {
        k: os.path.relpath(source / v, output) for k, v in recipe["maps"].items()
    }
    recipe["maps"].update({k: levels[0][k] for k in ("normal", "roughness")})
    for key in (
        "vein_plan",
        "minor_network",
        "material_radius_profile",
        "pigment_table",
        "optical_reference",
    ):
        recipe[key] = os.path.relpath(source / recipe[key], output)
    recipe.update(
        version="procedural-leaf-material-4",
        normal_levels=levels,
        normal_moments=moments_file.name,
        normal_moments_sha256=hashlib.sha256(moments_file.read_bytes()).hexdigest(),
        normal_model="physical-uv-slopes-1",
        roughness_filter="isotropic-slope-variance-1",
    )
    recipe["normal_level_sha256"] = {
        n: hashlib.sha256((output / n).read_bytes()).hexdigest()
        for files in levels
        for n in files.values()
    }
    (output / path.name).write_text(json.dumps(recipe, indent=2) + "\n")
    return {
        "index": index,
        "slope_seconds": slope_seconds,
        "seconds": times,
        "moment_max_error": maximum,
        "map_export_seconds": export_seconds,
        "diagnostic_export_seconds": diagnostic_seconds,
        "levels": cases,
        "moment_float32_bytes": sum(m.nbytes // 2 for m in results["pyramid"]),
        "normal_roughness_uncompressed_bytes": sum(c["size"] ** 2 * 7 for c in cases),
        "slope_max": float(abs(slopes).max()),
    }, {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}


def scenes(source, output):
    for name in (
        "leaf-material-upper.json",
        "leaf-coupled-detail.json",
        "leaf-material-lower.json",
    ):
        scene = json.loads((source / name).read_text())
        for product in scene["products"]:
            for key in ("mesh", "placements"):
                product[key] = os.path.relpath(source / product[key], output)
        (output / name).write_text(json.dumps(scene, indent=2) + "\n")
    comparison = json.loads((output / "leaf-material-upper.json").read_text())
    sample = comparison["products"][0]
    comparison["products"] = [
        sample
        | {"placements": p["placements"], "normal_level": level, "normal_filter": mode}
        for p, level, mode in zip(
            comparison["products"], (0, 4, 4), ("variance", "mean", "variance")
        )
    ]
    for index, product in enumerate(comparison["products"]):
        filename = f"normal-comparison-{index}-mesh.npz"
        shutil.copy2(output / product["mesh"], output / filename)
        product["mesh"] = filename
    (output / "leaf-normal-levels.json").write_text(
        json.dumps(comparison, indent=2) + "\n"
    )


def main():
    source = Path("build/experiments/vegetation/pigment-leaves")
    output = source.with_name("normal-leaves")
    output.mkdir(parents=True, exist_ok=True)
    checks = normal_checks()
    cases = []
    inputs = {}
    for i in range(3):
        case, files = build(source, output, i)
        cases.append(case)
        inputs.update(files)
        print(i, {k: v for k, v in case.items() if k != "levels"}, flush=True)
    scenes(source, output)
    report = {
        "cases": cases,
        "checks": checks,
        "input_sha256": inputs,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "versions": {"tifffile": tifffile.__version__, "numpy": np.__version__},
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                *Path(__file__).parent.glob("leaf_normal*.py"),
                Path(__file__).with_name("blade_surface.py"),
            ]
        },
    }
    (output / "normals.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
