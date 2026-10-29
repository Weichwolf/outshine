"""Raw-field interpolation on controlled metric planes; actual native inputs separate."""

import argparse
import hashlib
import json
import subprocess
from functools import partial
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from relief_shading_reference import specular
from terrain_material_grid import bake, sample
from terrain_material_lattice import analytic, evaluate, prepare
from terrain_material_noise import MATRICES, rock_fields, rock_response

ROOT = Path(__file__).resolve().parents[3]


def unit(vector):
    vector = np.asarray(vector, dtype=float)
    return vector / np.linalg.norm(vector, axis=-1, keepdims=True)


def response(fields, dx, dy, recipe):
    return rock_response(
        fields,
        dx,
        dy,
        coarse_m=recipe["detailScaleM"][0],
        fine_m=recipe["detailScaleM"][1],
        height_m=recipe["heightAmplitudeM"],
        contrast=0.6,
    )


def appearance(detail, basis, footprint, recipe):
    normal = unit(np.cross(basis[:, 1], basis[:, 0]))
    dual = basis @ np.linalg.inv(basis.T @ basis)
    normals = unit(normal - detail[..., 2:] / footprint @ dual.T)
    light, view = unit([0.5, 0.8, -0.2]), unit([0, 1, 0.6])
    nl = np.maximum(normals @ light, 0)
    roughness = np.clip(recipe["roughness"] + detail[..., 1], 0.04, 1)
    albedo = np.clip(np.asarray(recipe["albedo"]) * detail[..., :1], 0, 1)
    radiance = albedo * (0.12 + 0.96 * nl[..., None]) / np.pi
    radiance += specular(normals, roughness, light, view)[..., None]
    rgb = np.where(
        radiance <= 0.0031308, 12.92 * radiance, 1.055 * radiance ** (1 / 2.4) - 0.055
    )
    return normals, radiance, Image.fromarray(np.uint8(np.clip(rgb, 0, 1) * 255))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source = ROOT / "data/world/ground-materials.json"
    recipe = next(
        r
        for r in json.loads(source.read_bytes())["classes"]
        if r["name"] == "limestone"
    )
    width, height, extent = 320, 280, (4.0, 4.0)
    u, v = np.meshgrid(
        (np.arange(width) + 0.5) / width * extent[0],
        (np.arange(height) + 0.5) / height * extent[1],
    )
    uv = np.stack((u, v), axis=-1)
    footprint = np.array([extent[0] / width, extent[1] / height])
    origin = np.array([1000.13, 1770.31, -200.27])
    report = []
    canvas = Image.new("RGB", (4 * width, 2 * (height + 24)), "white")
    labels = ImageDraw.Draw(canvas)
    for row, sign in enumerate((1, -1)):
        basis = np.array([[1, 0], [sign * 0.7, -sign * 0.3], [0, 1]], dtype=float)
        world = origin + uv @ basis.T
        dx = np.broadcast_to(basis[:, 0] * footprint[0], world.shape)
        dy = np.broadcast_to(basis[:, 1] * footprint[1], world.shape)
        exact_fields = rock_fields(
            world, *recipe["detailScaleM"], noise_queries=(analytic,) * 3
        )
        exact = response(exact_fields, dx, dy, recipe)
        normal_ref, light_ref, image = appearance(exact, basis, footprint, recipe)
        image.save(args.output / f"{row}-analytic.png")
        canvas.paste(image, (0, row * (height + 24) + 24))
        labels.text(
            (4, row * (height + 24) + 4),
            f"Controlled slope {sign:+}; analytic",
            fill="black",
        )
        arrays = {
            "uv_m": uv,
            "world_m": world,
            "dx_m": dx,
            "dy_m": dy,
            "analytic": exact,
            "analytic_normals": normal_ref,
            "analytic_radiance": light_ref,
        }
        for column, side in enumerate((17, 65), start=1):
            grids = tuple(
                bake(origin, *basis.T, extent, (side, side), matrix, wavelength)
                for matrix, wavelength in zip(
                    MATRICES,
                    (12 * recipe["detailScaleM"][0], *recipe["detailScaleM"]),
                    strict=True,
                )
            )
            sampled = tuple(sample(grid, uv) for grid in grids)
            detail = response(sampled, dx, dy, recipe)
            normals, radiance, image = appearance(detail, basis, footprint, recipe)
            image.save(args.output / f"{row}-grid-{side}.png")
            canvas.paste(image, (column * width, row * (height + 24) + 24))
            labels.text(
                (column * width + 4, row * (height + 24) + 4),
                f"Raw RGBA32 grid {side}x{side}; no MIPs",
                fill="black",
            )
            angle = np.rad2deg(
                np.arccos(np.clip(np.sum(normal_ref * normals, axis=-1), -1, 1))
            )
            error = radiance - light_ref
            report.append(
                {
                    "slope_sign": sign,
                    "grid_side": side,
                    "base_field_bytes": sum(g.retained_bytes for g in grids),
                    "complete_mip_payload_arithmetic_bytes": 3
                    * 16
                    * sum(
                        max(1, side >> level) ** 2 for level in range(side.bit_length())
                    ),
                    "normal_angle_max_p95_deg": [
                        float(angle.max()),
                        float(np.quantile(angle, 0.95)),
                    ],
                    "response_channel_max_abs": np.max(
                        abs(detail - exact), axis=(0, 1)
                    ).tolist(),
                    "radiance_max_abs_rms": [
                        float(abs(error).max()),
                        float(np.sqrt(np.mean(error * error))),
                    ],
                    "raw_components_max_abs": [
                        np.max(abs(a - b), axis=(0, 1)).tolist()
                        for a, b in zip(sampled, exact_fields, strict=True)
                    ],
                    "nominal_hashes_per_full_resolved_analytic_query": 24,
                    "nominal_field_texels_and_float32_source_words_per_query": [12, 48],
                    "hardware_samples_cache_misses_and_transactions": None,
                }
            )
            arrays[f"grid_{side}_response"] = detail
            arrays[f"grid_{side}_normals"] = normals
            arrays[f"grid_{side}_radiance"] = radiance
        corners = origin + np.array([[0, 0], [4, 0], [0, 4], [4, 4]]) @ basis.T
        lattices = []
        for matrix, wavelength in zip(
            MATRICES,
            (12 * recipe["detailScaleM"][0], *recipe["detailScaleM"]),
            strict=True,
        ):
            at = corners @ (matrix / wavelength).T
            lattices.append(prepare(np.stack((at.min(axis=0), at.max(axis=0)))))
        lattice_fields = rock_fields(
            world,
            *recipe["detailScaleM"],
            noise_queries=tuple(partial(evaluate, grid) for grid in lattices),
        )
        for a, b in zip(exact_fields, lattice_fields, strict=True):
            np.testing.assert_array_equal(a, b)
        detail = response(lattice_fields, dx, dy, recipe)
        np.testing.assert_array_equal(detail, exact)
        normals, radiance, image = appearance(detail, basis, footprint, recipe)
        np.testing.assert_array_equal(radiance, light_ref)
        image.save(args.output / f"{row}-lattice.png")
        canvas.paste(image, (3 * width, row * (height + 24) + 24))
        labels.text(
            (3 * width + 4, row * (height + 24) + 4),
            "Original integer lattice; current cubic",
            fill="black",
        )
        report.append(
            {
                "slope_sign": sign,
                "method": "original_lattice",
                "base_field_bytes": sum(g.retained_bytes for g in lattices),
                "lattice_shapes_zyx": [g.words_zyx.shape for g in lattices],
                "mips_are_used": False,
                "raw_fields_response_and_radiance_exact": True,
                "nominal_original_hashes_per_query": 0,
                "nominal_uint32_corner_words_per_query": 24,
                "hardware_samples_cache_misses_and_transactions": None,
            }
        )
        arrays["lattice_response"], arrays["lattice_radiance"] = detail, radiance
        np.savez_compressed(args.output / f"{row}-controlled.npz", **arrays)
    canvas.save(args.output / "controlled-comparison.png")
    source_names = (
        "groundRock.glsl",
        "rockCrevice.glsl",
        "surfaceNoise.glsl",
        "surfaceGradient.glsl",
    )
    summary = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "Controlled planes/explicit phase and limestone recipe only. Ideal Double GLSL reconstruction vs bilinear raw Float32 fields and original integer lattice with identical cubic evaluation. No captured native points, DEM, class/litter composition, improved filtering/NDF equivalence, native/GPU/frame/device claim. Existing simplified GGX+diffuse diagnostic, no sky/shadows or product energy table.",
        "recipe_source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "shader_source_sha256": {
            name: hashlib.sha256(
                (ROOT / "src/render/shaders" / name).read_bytes()
            ).hexdigest()
            for name in source_names
        },
        "recipe": recipe,
        "contrast_control": 0.6,
        "origin_control_m": origin.tolist(),
        "mips_are_used": False,
        "rows": report,
    }
    (args.output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
