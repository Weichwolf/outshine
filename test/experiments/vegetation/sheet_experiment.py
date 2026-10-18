"""Measure anchored leaf charts and emit one larger tree from its canonical plan."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from axis_surface import axis_mesh
from leaf_geometry_lod import displacement, interpolate, merge_instances, parameter_grid
from leaf_sheet import average_blocks, make_sheet, rasterize, sample_image
from organ_experiment import write_product
from plant_experiment import sapling
from plant_plan import AxisLevel, PlantPlan
from shoot_plan import normal_frame

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def projection_view(blade, sheet, direction, pixels, spatial=None):
    reference = blade.mesh(64, across=2)
    frame = normal_frame(np.asarray(direction, dtype=float), np.array((0.0, 0.0, 1.0)))
    camera = np.column_stack((frame[:, 0], frame[:, 2], frame[:, 1]))
    original = reference["vertices"] @ camera
    simplified = (sheet.mesh if spatial is None else spatial)["vertices"] @ camera
    points = np.vstack((original, simplified))
    centre = (points[:, :2].max(axis=0) + points[:, :2].min(axis=0)) / 2
    extent = np.max(np.ptp(points[:, :2], axis=0)) * 1.1
    lower, upper = centre - extent / 2, centre + extent / 2
    normals = reference["normals"].reshape(-1, 3, 3)
    ref_mask, ref_normals = rasterize(
        original, reference["faces"], normals, lower, upper, pixels * 4
    )
    if spatial is None:
        card_mask, uv = rasterize(
            simplified,
            sheet.mesh["faces"],
            sheet.mesh["uv"][sheet.mesh["faces"]],
            lower,
            upper,
            pixels * 4,
        )
        sampled = sample_image(sheet.normal, uv)
        alpha = sampled[..., 3] * card_mask
        encoded = sampled[..., :3]
        decoded = (2 * encoded - sampled[..., 3, None]) / np.maximum(
            sampled[..., 3, None], 1e-12
        )
        decoded /= np.maximum(np.linalg.norm(decoded, axis=-1, keepdims=True), 1e-12)
        card_normals = decoded @ sheet.frame.T
    else:
        mask, card_normals = rasterize(
            simplified,
            spatial["faces"],
            spatial["normals"].reshape(-1, 3, 3),
            lower,
            upper,
            pixels * 4,
        )
        alpha = mask.astype(float)
        card_normals /= np.maximum(
            np.linalg.norm(card_normals, axis=-1, keepdims=True), 1e-12
        )
    ref_normals /= np.maximum(
        np.linalg.norm(ref_normals, axis=-1, keepdims=True), 1e-12
    )
    reference_coverage, card_coverage = (
        average_blocks(ref_mask.astype(float), 4),
        average_blocks(alpha, 4),
    )
    difference = np.abs(reference_coverage - card_coverage)
    union = np.maximum(reference_coverage, card_coverage).sum()
    lighting = []
    for light_direction in ((0, 0, 1), (1, 1, 1), (-1, 0.5, 0.25)):
        light = np.asarray(light_direction, dtype=float)
        light /= np.linalg.norm(light)
        ref_response = (0.2 + 0.8 * np.abs(ref_normals @ light)) * ref_mask
        card_response = (0.2 + 0.8 * np.abs(card_normals @ light)) * alpha
        lighting.append(
            float(
                abs(ref_response.sum() - card_response.sum())
                / max(ref_response.sum(), 1e-12)
            )
        )
    return {
        "pixels": pixels,
        "coverage_l1_over_union": float(difference.sum() / max(union, 1e-12)),
        "area_relative_error": float(
            abs(reference_coverage.sum() - card_coverage.sum())
            / max(reference_coverage.sum(), 1e-12)
        ),
        "light_response_relative_errors": lighting,
        "scope": "orthographic supersampled coverage and two-sided diffuse proxy; not a full BRDF or runtime image",
    }


def check_geometry(blade):
    rng = np.random.default_rng(614)
    for segments, across in ((16, 1), (64, 2)):
        mesh = blade.mesh(segments, across)
        parameters = mesh["parameters"][mesh["faces"]]
        # Welded tips have multiple parameter preimages; test ordinary triangles here.
        regular = np.all(parameters[:, :, 0] < 1, axis=1)
        weights = rng.dirichlet([1, 1, 1], regular.sum())
        uv = np.einsum("ij,ijk->ik", weights, parameters[regular])
        expected = np.einsum(
            "ij,ijk->ik", weights, mesh["vertices"][mesh["faces"][regular]]
        )
        np.testing.assert_allclose(
            interpolate(*parameter_grid(blade, segments, across), uv),
            expected,
            atol=1e-14,
        )
    mesh["uv"] = mesh["parameters"]
    origins = rng.normal(size=(3, 3))
    frames = np.repeat(np.eye(3)[None], 3, axis=0)
    merged = merge_instances(mesh, origins, frames, np.array([4, 7, 9]))
    for index in range(3):
        np.testing.assert_allclose(
            merged["vertices"].reshape(3, -1, 3)[index],
            mesh["vertices"] + origins[index],
        )
    assert displacement(blade, 64, 2) < 2e-12
    for arguments in ((0, 1), (16, 0), (-2, 1), (3, 1), (16, 3), (1.5, 1)):
        try:
            displacement(blade, *arguments)
        except ValueError:
            continue
        raise AssertionError(arguments)


def check_sheets(output):
    blades = sapling().blades
    check_geometry(blades[0])
    sheets, cases = [], []
    rng = np.random.default_rng(31)
    for variant, blade in enumerate(blades):
        alternatives = {}
        for method in ("flat", "anchored"):
            start = time.perf_counter()
            sheet = make_sheet(blade, variant, fit=method)
            elapsed = (time.perf_counter() - start) * 1000
            root = np.array((0.0, -blade.petiole, 0.0))
            np.testing.assert_allclose(
                np.dot(root - sheet.origin, sheet.frame[:, 2]), 0, atol=1e-15
            )
            np.testing.assert_allclose(
                sheet.frame.T @ sheet.frame, np.eye(3), atol=1e-15
            )
            t, s = rng.uniform(0, 1, 10000), rng.uniform(-1, 1, 10000)
            points = blade.evaluate(t, s, np.where(s < 0, -1, 1))[0]
            projected_distance = np.abs((points - root) @ sheet.frame[:, 2])
            assert projected_distance.max() <= sheet.displacement_bound_m
            np.testing.assert_array_equal(sheet.colour[..., 3], sheet.normal[..., 3])
            for values in (sheet.colour, sheet.normal):
                mean = values.mean(axis=(0, 1))
                current = values
                while len(current) > 1:
                    current = average_blocks(current)
                    np.testing.assert_allclose(
                        current.mean(axis=(0, 1)), mean, atol=2e-12
                    )
                assert np.all(np.isfinite(values)) and np.all(
                    (0 <= values) & (values <= 1)
                )
            alternatives[method] = {
                "generation_ms": elapsed,
                "displacement_bound_m": sheet.displacement_bound_m,
                "measured_displacement_m": float(projected_distance.max()),
                "alpha_mean": float(sheet.colour[..., 3].mean()),
            }
            if method == "anchored":
                sheets.append(sheet)
        assert (
            alternatives["anchored"]["displacement_bound_m"]
            < alternatives["flat"]["displacement_bound_m"]
        )
        coarse = blade.mesh(16, across=1)
        bound = displacement(blade)
        parameters = np.column_stack(
            (
                rng.uniform(-blade.petiole / blade.length, 1, 10000),
                rng.uniform(-1, 1, 10000),
            )
        )
        measured = np.linalg.norm(
            interpolate(*parameter_grid(blade, 64, 2), parameters)
            - interpolate(*parameter_grid(blade, 16, 1), parameters),
            axis=1,
        ).max()
        assert measured <= bound
        alternatives["spatial16"] = {
            "displacement_bound_m": bound,
            "measured_displacement_m": float(measured),
            "triangles": len(coarse["faces"]),
        }
        views = []
        if variant in (0, 7):
            for elevation in (0.05, 0.4, 1.0):
                for azimuth in (0.0, 1.2, 2.4, 3.6):
                    direction = (np.cos(azimuth), np.sin(azimuth), elevation)
                    views.append(
                        projection_view(blade, sheets[-1], direction, 8)
                        | {
                            "direction": direction,
                            "spatial16": projection_view(
                                blade, sheets[-1], direction, 8, coarse
                            ),
                        }
                    )
        cases.append({"variant": variant, "alternatives": alternatives, "views": views})
    flat = make_sheet(type(blades[0])(bend=0, twist=0, fold=0))
    np.testing.assert_allclose(flat.mesh["vertices"][:, 2], 0, atol=1e-15)
    (output / "sheet-checks.json").write_text(json.dumps(cases, indent=2) + "\n")
    return sheets, cases


def larger_tree(seed=17):
    levels = (
        AxisLevel(1, 7.0, 0.18, 0.065, tip_scale=1, bend=0.06, bend_variation=0.04),
        AxisLevel(
            7,
            3.8,
            0.035,
            0.012,
            tip_scale=0.65,
            first=0.45,
            last=0.98,
            angle=1.18,
            bend=0.35,
        ),
        AxisLevel(
            8,
            1.7,
            0.005,
            0.0018,
            tip_scale=0.6,
            first=0.25,
            last=0.92,
            angle=0.95,
            bend=0.4,
        ),
        AxisLevel(
            7,
            0.42,
            0.0007,
            0.0003,
            tip_scale=0.7,
            first=0.25,
            last=0.9,
            angle=0.9,
            bend=0.45,
        ),
        AxisLevel(
            6,
            0.22,
            0.00015,
            0.0001,
            tip_scale=0.8,
            first=0.3,
            last=0.95,
            angle=0.95,
            bend=0.35,
            leaf_internode=0.022,
        ),
    )
    return PlantPlan(seed, levels, sapling().blades)


def assemble_wood(plant, detail):
    vertices, faces, normals = [], [], []
    offset = 0
    retained, omitted = 0, 0
    for state in detail["axes"]:
        base_radius, tip_radius = plant.radii(state.path)
        if base_radius < 0.001:
            omitted += 1
            continue
        mesh = axis_mesh(
            state.length,
            state.bend,
            base_radius,
            tip_radius,
            segments=12,
            sides=8,
        )
        vertices.append(mesh["vertices"] @ state.frame.T + state.origin)
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"] @ state.frame.T)
        offset += len(mesh["vertices"])
        retained += 1
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
    }, {
        "rendered_axes": retained,
        "subpixel_axes_in_plan": omitted,
        "maximum_omitted_radius_m": 0.0007,
    }


def make_scenes(output, sheets, commit, modified):
    plant = larger_tree()
    summary = plant.summary()
    began = time.perf_counter()
    detail = plant.expand(summary["axes"], summary["leaves"])
    generation_ms = (time.perf_counter() - began) * 1000
    groups = detail["groups"]
    poses = {key: np.concatenate([group[key] for group in groups]) for key in groups[0]}
    np.testing.assert_array_equal(poses["ids"], np.arange(summary["leaves"]))
    np.savez_compressed(output / "canonical-leaves.npz", **poses)
    wood, wood_stats = assemble_wood(plant, detail)
    stem = write_product(
        output, "tree-wood", wood, np.zeros((1, 3)), np.eye(3)[None], "wood"
    )
    products = {"fine": [stem], "cards": [stem], "coarse": [stem]}
    for variant, (blade, sheet) in enumerate(zip(plant.blades, sheets)):
        textures = sheet.save(output, f"leaf-{variant}")
        selected = poses["variants"] == variant
        for mode, items in products.items():
            if mode == "fine":
                mesh = blade.mesh(64, across=2)
                mesh["uv"] = sheet.coordinates(mesh["vertices"])
                origins, frames = poses["origins"][selected], poses["frames"][selected]
            else:
                source_mesh = (
                    sheet.mesh if mode == "cards" else blade.mesh(16, across=1)
                )
                if mode == "coarse":
                    source_mesh["uv"] = sheet.coordinates(source_mesh["vertices"])
                mesh = merge_instances(
                    source_mesh,
                    poses["origins"][selected],
                    poses["frames"][selected],
                    poses["ids"][selected],
                )
                origins, frames = np.zeros((1, 3)), np.eye(3)[None]
            product = write_product(
                output,
                f"tree-{mode}-{variant}",
                mesh,
                origins,
                frames,
                "leaf",
            )
            product.update(textures=textures, card=mode == "cards")
            items.append(product)
    camera, target = np.array((15.0, -30.0, 13.5)), np.array((0, 0, 6.5))
    forward = (target - camera) / np.linalg.norm(target - camera)
    bound = summary["root_bound_radius_m"]
    minimum_depth = float(-camera @ forward - bound)
    radial = float(np.linalg.norm(camera + (-camera @ forward) * forward)) + bound
    focal = 720 / (2 * np.tan(np.deg2rad(35) / 2))
    jacobian = focal / minimum_depth * np.sqrt(1 + (radial / minimum_depth) ** 2)
    error = max(sheet.displacement_bound_m for sheet in sheets) * jacobian
    assert minimum_depth > 0 and error <= 1
    scenes = []
    for mode, items in products.items():
        scene = {
            "name": f"tree-{mode}",
            "title": f"Größere Laubbaumprobe · {mode}",
            "products": items,
            "camera": camera.tolist(),
            "target": target.tolist(),
            "fov_y": 35,
            "ground_size_m": 300,
            "clip_end_m": 400,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": modified,
            "plan_identity": plant.identity,
        }
        filename = f"tree-{mode}.json"
        (output / filename).write_text(json.dumps(scene, indent=2) + "\n")
        scenes.append(filename)
    return {
        "summary": summary,
        "generation_ms": generation_ms,
        "geometry_displacement_bound_px": {
            "cards_to_surface": float(error),
            "coarse_to_fine_mesh": max(displacement(blade) for blade in plant.blades)
            * jacobian,
        },
        "minimum_depth_m": minimum_depth,
        "wood": wood_stats,
        "scenes": scenes,
        "leaf_triangles": {
            mode: sum(
                product["triangles_per_instance"] * product["instances"]
                for product in items
                if product["surface"] == "leaf"
            )
            for mode, items in products.items()
        },
        "rgba8_chart_bytes_with_mips_estimate": sum(
            sheet.colour.size + sheet.normal.size for sheet in sheets
        )
        * 4
        / 3,
        "chart_byte_scope": "RGBA8 colour/normal with theoretical mip chains; actual Blender GPU format and driver cost unmeasured",
        "native_runtime_quality_proven": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/sheets")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sheets, checks = check_sheets(args.output)
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    source = [
        "leaf_sheet.py",
        "leaf_geometry_lod.py",
        "sheet_experiment.py",
        "plant_plan.py",
        "plant_experiment.py",
        "blade_surface.py",
        "shoot_plan.py",
        "curve_frame.py",
    ]
    modified = bool(
        subprocess.check_output(
            [
                "git",
                "status",
                "--porcelain",
                "--",
                *[str(HERE / name) for name in source],
            ],
            cwd=ROOT,
            text=True,
        ).strip()
    )
    tree = make_scenes(args.output, sheets, commit, modified)
    report = {
        "commit": commit,
        "source_modified": modified,
        "sheets": checks,
        "tree": tree,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "peak_scope": "whole Linux generation and correctness process; no Blender or GPU",
    }
    (args.output / "sheets.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": commit,
                "source_modified": modified,
                "tree": tree,
                "process_peak_rss_mib": report["process_peak_rss_mib"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
