"""Stored paired products under physical motion and explicit direct-light controls."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_mattes import read_matte, read_prism, response_signatures
from building_mass_materials import policy
from building_material_reference import face_materials
from building_matte_image import quad
from building_motion_reference import (
    motion_bounds,
    residual_change,
    residuals,
    trajectory,
)
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_reference import reference
from compare_building_hierarchy import complete, write_images
from compare_building_mattes import compare_query
from compare_building_source_models import contact
from compare_mass_appearance import read
from process_memory import peak_rss_bytes

LIGHTS = {
    "day": {"light": [0.45, -0.5, 0.7], "intensity": 3, "ambient": 0.1},
    "grazing": {"light": [-0.8, 0.6, 0.08], "intensity": 3, "ambient": 0.1},
    "ambient_only": {"light": [0, 0, 1], "intensity": 0, "ambient": 0.01},
}


def prepared(args, cell):
    root, products = args.input / f"cell-{cell}", args.products / f"cell-{cell}"
    case, tree, at = (
        json.loads((root / "report.json").read_text()),
        read_tree(root),
        policy(),
    )
    query = case["queries"][args.query_index]
    if query["nodes"] != [0]:
        raise ValueError("existing root view required")
    baseline, baseline_material, loading = load_cut(root, case, tree, query, at)
    prism, material = read_prism(products / "prism.npz", tree, at)
    separate, merged = [
        read_matte(products / name, root / case["node_files"]["0"], tree, at)
        for name in ("matte-separate.npz", "matte-merged.npz")
    ]
    original, provenance = reference(
        args.source,
        cell,
        tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("complete fine material reference required")
    camera = view(query | {"eye": query["render_eye"]})
    return (
        {
            "original": original,
            "fields": fields,
            "baseline": baseline,
            "baseline_material": baseline_material,
            "prism": prism,
            "material": material,
            "separate": separate,
            "merged": merged,
        },
        camera,
        provenance,
        loading,
    )


def run_light(output, scene, cameras, times, box, lighting, tables):
    output.mkdir()
    reports, panels, previous = [], [], None
    for i, camera in enumerate(cameras):
        images, actual_box, report = compare_query(
            **scene, camera=camera, tables=tables, fixed_box=box, lighting=lighting
        )
        current = residuals(images)
        report["seconds_from_midpoint"] = float(times[i])
        report["eye"] = camera[0].tolist()
        report["residual_change"] = (
            residual_change(previous, current, times[i] - times[i - 1])
            if previous is not None
            else None
        )
        reports.append(report)
        previous = current
        if i in (0, len(cameras) // 2, len(cameras) - 1):
            write_images(output, f"frame-{i}", images, {}, camera, actual_box)
            panels.append((images, actual_box))
    contact(panels, output)
    return {"lighting": lighting, "frames": reports}


def run_case(args, cell, tables):
    scene, camera, provenance, loading = prepared(args, cell)
    times, cameras = trajectory(camera, args.local_velocity, args.duration, args.frames)
    positions = [scene["original"]["positions"]]
    for name in ("baseline", "prism"):
        mesh, _ = complete(scene["original"], scene[name])
        positions.append(mesh["positions"])
    positions.extend(
        quad(scene["separate"], i)
        for i in range(len(scene["separate"]["plane_origin"]))
    )
    box = motion_bounds(np.vstack(positions), cameras)
    output = args.output / f"cell-{cell}"
    output.mkdir()
    result = {
        "cell": cell,
        "reference": provenance,
        "baseline_loading": loading,
        "physical_speed_m_s": float(np.linalg.norm(args.local_velocity)),
        "local_velocity_right_up_forward_m_s": args.local_velocity,
        "times_seconds_from_midpoint": times.tolist(),
        "fixed_screen_roi": box.tolist(),
        "reconstructed_products_per_pose": 0,
        "read_once_product_array_bytes": {
            k: sum(v.nbytes for v in scene[k].values())
            for k in (
                "baseline",
                "baseline_material",
                "prism",
                "material",
                "separate",
                "merged",
            )
        },
        "offline_reference_arrays_bytes": {
            k: sum(v.nbytes for v in scene[k].values()) for k in ("original", "fields")
        },
        "lights": {},
    }
    for name, lighting in LIGHTS.items():
        result["lights"][name] = run_light(
            output / name, scene, cameras, times, box, lighting, tables
        )
        (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({"completed_cell": cell, "completed_light": name}), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--native-cell", type=int)
    parser.add_argument("--query-index", type=int, default=1)
    parser.add_argument(
        "--local-velocity", type=float, nargs=3, default=[300, 100, 200]
    )
    parser.add_argument("--duration", type=float, default=1)
    parser.add_argument("--frames", type=int, default=9)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = json.loads((args.products / "report.json").read_text())
    if receipt["response_program_sha256"] != response_signatures():
        raise ValueError("compiled response and current lighting model differ")
    wall, cpu = time.perf_counter(), time.process_time()
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": [run_case(args, cell, tables) for cell in args.cells],
    }
    result["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    result["scope"] = (
        "Finite declared-pose physical translation, three fixed direct/ambient controls,1280x720/SS16. Residual change is fixed-screen error variation after subtracting the moving full reference, not motion-reprojected temporal quality. No generation per pose. No world/night/local lights/shadows/caster/LOD-transition/native/GPU/fps admission."
    )
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
