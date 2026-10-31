"""Finite native source exclusion with a visible terrain opponent, no C++ changes."""

import argparse
import copy
import hashlib
import json
import os
import subprocess
from pathlib import Path

import numpy as np
from experiment_output import create_output
from export_native_field_opponents import append_mesh, execute, read_samples
from native_source_triangle_materials import DATA
from native_world_inputs import load
from PIL import Image
from source_clip_demand import query
from source_hierarchy import prepare
from source_triangle_materials import write_source


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024**2), b""):
            h.update(data)
    return h.hexdigest()


def cameras(base):
    requests = []
    for angle in (0, 65, 90):
        camera = copy.deepcopy(base)
        if angle:
            c, s = np.cos(np.deg2rad(angle)), np.sin(np.deg2rad(angle))
            rotation = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
            for axis in ("right", "up", "forward"):
                camera[axis] = (rotation.T @ np.asarray(base[axis])).tolist()
        requests.append(
            {
                "name": "angle-" + str(angle),
                "capture": camera,
                "field_lighting": "viewer",
                "lighting_camera": {"capture": copy.deepcopy(base)},
                "sun_elevation_deg": 45,
            }
        )
    return requests


def picture(colour, path):
    h, w = colour.shape[:2]
    rgb = (
        colour[: h // 2 * 2, : w // 2 * 2, :3]
        .reshape(h // 2, 2, w // 2, 2, 3)
        .mean((1, 3))
    )
    display = (1 - np.exp(-np.maximum(rgb, 0) / 32.0)) ** (1 / 2.2)
    Image.fromarray(np.clip(display * 255, 0, 255).astype(np.uint8)).save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--shader-directory", type=Path, required=True)
    parser.add_argument("--observed-native-driver-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    create_output(args.output)
    m, _, _, source = load(DATA, attributes=True)
    h = prepare(source)
    requests = cameras(m["world_cameras"]["near-direction"])
    original = args.output / "whole"
    write_source(source, np.arange(len(source["indices"]), dtype=np.uint32), original)
    terrain = args.output / "terrain" / "geometry" / "0"
    terrain.mkdir(parents=True)
    centre = h.bounds.astype(float).mean(0)
    eye = np.asarray(requests[0]["capture"]["eye_m"])
    radius = 4 * max(
        float(np.linalg.norm(eye - centre)), float(np.max(np.diff(h.bounds, axis=0)))
    )
    y = float(h.bounds[0, 1]) - 5
    x, z = centre[[0, 2]]
    description = {"parts": []}
    append_mesh(
        terrain,
        description,
        {
            "positions": [
                [x - radius, y, z - radius],
                [x + radius, y, z - radius],
                [x + radius, y, z + radius],
                [x - radius, y, z + radius],
            ],
            "indices": [[0, 2, 1], [0, 3, 2]],
            "colour": [0.25, 0.32, 0.2, 1.0],
            "material": 0,
        },
    )
    (terrain / "input.json").write_text(json.dumps(description, indent=2) + "\n")

    def inventory():
        return {
            p.name: sha(p)
            for p in sorted(args.shader_directory.iterdir())
            if p.is_file()
        }

    before = inventory()
    program_sha = sha(args.binary)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "binary_sha256": program_sha,
        "diagnostic_exposure": 1 / 32,
        "observed_driver_cpp_sha256": sha(args.observed_native_driver_source),
        "shader_before": before,
        "cases": [],
        "scope": "Existing-binary finite Iris composition, not fresh CDB/native-owner/World/frame/UMA acceptance. Source and actual Lens/pre-view inputs preserved; no C++/shader/other-checkout edits. Raw2560x1440 plus2x2 diagnostic reduction. Native Layer/ICD selection belongs to the launching environment.",
        "environment": {
            k: os.environ.get(k)
            for k in (
                "VK_DRIVER_FILES",
                "VK_INSTANCE_LAYERS",
                "VK_LAYER_PATH",
                "LD_LIBRARY_PATH",
                "SDL_VIDEODRIVER",
            )
        },
    }
    reports = {}
    for name, extra in (
        ("terrain-only", []),
        ("union", [{"directory": str(original.resolve())}]),
    ):
        folder = args.output / name
        reports[name] = execute(
            args.binary,
            folder,
            {
                "directory": str(terrain.parents[1].resolve()),
                "additional_sources": extra,
                "queries": requests,
            },
        )
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as a:
        for index, request in enumerate(requests):
            name = request["name"]
            base_case = reports["terrain-only"]["cases"][index]
            union_case = reports["union"]["cases"][index]
            if base_case["lens_operands"] != union_case["lens_operands"]:
                raise ValueError("Native capture operands differ across composition")
            lens = union_case["lens_operands"]
            words = a["near-direction/view_words"].copy()
            words[:16] = np.asarray(
                lens["view_projection_f32_column_major"], "<f4"
            ).view("<u4")
            words[48:51] = np.asarray(lens["pre_view_translation_f64"], "<f4").view(
                "<u4"
            )
            ids, work = query(
                h, words, a["placements/words"], m["raster"], raster_guard_px=0.0
            )
            values = {
                kind: read_samples(args.output / kind, name, request["capture"])
                for kind in reports
            }
            bc, bd = values["terrain-only"]
            uc, ud = values["union"]
            count = int(np.count_nonzero(bd))
            different = (bc != uc).any(2) | (bd != ud)
            row = {
                "query": name,
                "background_covered": count,
                "different_pixels": int(different.sum()),
                "same_colour_words": sha(
                    args.output / "terrain-only" / (name + "-colour.f32")
                )
                == sha(args.output / "union" / (name + "-colour.f32")),
                "same_depth_words": sha(
                    args.output / "terrain-only" / (name + "-depth.f32")
                )
                == sha(args.output / "union" / (name + "-depth.f32")),
                "same_integral_words": sha(
                    args.output / "terrain-only" / (name + "-integral.f32")
                )
                == sha(args.output / "union" / (name + "-integral.f32")),
                "native_lens_operands": lens,
                "selected": len(ids),
                "work": work,
            }
            if count == 0:
                raise ValueError("Composition opponent is not visible")
            if name == "angle-90":
                if len(ids) or not all(
                    row[k]
                    for k in (
                        "same_colour_words",
                        "same_depth_words",
                        "same_integral_words",
                    )
                ):
                    raise ValueError("Outside source changed visible opponent")
            elif row["different_pixels"] == 0:
                raise ValueError("Visible/partial source is not a composition opponent")
            for kind, (colour, depth) in values.items():
                picture(colour, args.output / (name + "-" + kind + ".png"))
                np.savez_compressed(
                    args.output / (name + "-" + kind + ".npz"), rgba=colour, depth=depth
                )
            report["cases"].append(row)
    report["binary_unchanged"] = sha(args.binary) == program_sha
    report["shaders_unchanged"] = inventory() == before
    if not report["binary_unchanged"] or not report["shaders_unchanged"]:
        raise ValueError("Observed native inputs changed")
    report["native_reports"] = reports
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "report": str(args.output / "report.json"),
                "cases": [
                    {
                        k: r[k]
                        for k in (
                            "query",
                            "background_covered",
                            "different_pixels",
                            "selected",
                        )
                    }
                    for r in report["cases"]
                ],
            }
        )
    )


if __name__ == "__main__":
    main()
