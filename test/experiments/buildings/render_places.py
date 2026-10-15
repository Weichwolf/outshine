"""Render current Python building scenes at the four fixed native Place cameras."""

import argparse
import gc
import hashlib
import json
import subprocess
import sys
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from evidence import revision
from place_materials import default_finish
from place_reference import PLACES
from place_scene import prepare


def render(name, inputs, output, spp):
    output.mkdir(parents=True, exist_ok=True)
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Place render requires a frozen clean Python implementation")
    started = time.perf_counter()
    mesh, scene = prepare(name, inputs)
    count = len(mesh["faces"])
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    del mesh
    gc.collect()
    view = scene["view"]
    config = {
        "profile": [view.width, view.height],
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "eye": view.eye.tolist(),
        "target": view.target.tolist(),
        "fov_deg": view.fov_deg,
        "wall_linear": [0.68, 0.67, 0.64],
        "roof_linear": [0.22, 0.24, 0.25],
        "material_definitions": scene["material_definitions"],
        "facade_materials": scene["facade_materials"],
        "facade_finishes": default_finish(),
        "ground_span_m": 240000.0,
        "spp": spp,
        "seed": 17,
        "denoise": True,
        "output": str((output / (name + ".png")).resolve()),
    }
    scene_path = output / (name + "-scene.json")
    scene_path.write_text(json.dumps(config, indent=2) + "\n")
    with (output / (name + "-render.log")).open("w") as log:
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).with_name("mitsuba_interior.py")),
                str(scene_path),
            ],
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=900,
        )
    renderer = json.loads((output / (name + ".renderer.json")).read_text())
    if renderer["included_source_triangles"] != count:
        raise ValueError("Place renderer omitted generated source triangles")
    report = provenance | {
        "representation": "python-place-buildings",
        "place": name,
        "camera": asdict(scene["camera"]),
        "camera_ground_height_m": scene["camera_ground_height_m"],
        "ground_mode": scene["ground_mode"],
        "profile": config["profile"],
        "image": name + ".png",
        "image_sha256": renderer["image_sha256"],
        "renderer": renderer,
        "triangles": count,
        "tiles": scene["tiles"],
        "source_radius_m": scene["source_radius_m"],
        "limits": scene["limits"],
        "total_build_and_render_seconds": time.perf_counter() - started,
    }
    record = output / (name + "-report.json")
    record.write_text(json.dumps(report, indent=2) + "\n")
    print(name, "rendered", count, "triangles", flush=True)
    return record.name


def run(inputs, output, places, spp):
    names = {}
    for place in places:
        names[place] = render(place, inputs, output, spp)
    if set(names) == set(PLACES):
        (output / "place-reports.json").write_text(
            json.dumps({"commit": revision()["commit"], "reports": names}, indent=2)
            + "\n"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--places", nargs="+", choices=PLACES, default=list(PLACES))
    parser.add_argument("--spp", type=int, default=32)
    args = parser.parse_args()
    run(json.loads(args.sources.read_text()), args.output, args.places, args.spp)
