"""Isolate CPU facade dispatch cost with real palettes on small repeated controls."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import drjit as dr
import mitsuba as mi
import mitsuba_mesh
import numpy as np
from evidence import revision
from mitsuba_camera import camera
from mitsuba_interior import materials


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Palette control needs a clean frozen implementation")
    original = json.loads(args.scene.read_text())
    selected = list(original["facade_materials"].items())[: args.count]
    if not selected or len(selected) != args.count:
        raise ValueError("Requested palette count is unavailable")
    wall_ids = {str(row["wall_material"]) for _, row in selected}
    config = original | {
        "profile": [64, 36],
        "spp": 1,
        "eye": [80, -100, 100],
        "target": [80, 80, 0],
        "fov_deg": 55,
        "facade_materials": dict(selected),
        "material_definitions": {
            key: value
            for key, value in original["material_definitions"].items()
            if key in wall_ids
        },
    }
    mi.set_variant("llvm_ad_rgb")
    dr.set_thread_count(2)
    dr.set_flag(dr.JitFlag.KernelHistory, True)
    if args.lookup == "shape":
        # Diagnostic baseline only: recover the prior nested attribute dispatch.
        mitsuba_mesh.bind_facade_extent = lambda bsdf, data, indices: bsdf
    started = time.perf_counter()
    palette = materials(config)
    data = dict(np.load(args.mesh))
    sensor, projection = camera(config, 1)
    scene = {
        "type": "scene",
        "integrator": {"type": "path", "max_depth": 2},
        "sensor": sensor,
        "sky": {"type": "constant"},
    }
    rows = np.arange(len(data["faces"]))
    for index, (identifier, _) in enumerate(selected):
        shifted = data | {
            "vertices": data["vertices"] + [25 * (index % 8), 20 * (index // 8), 0]
        }
        scene["body_" + str(index)], _ = mitsuba_mesh.mesh(
            str(index), shifted, rows, *palette[int(identifier)]
        )
    scene = mi.load_dict(scene)
    dr.sync_thread()
    setup = time.perf_counter() - started
    started = time.perf_counter()
    image = mi.render(scene, spp=1, seed=17)
    dr.eval(image)
    dr.sync_thread()
    elapsed = time.perf_counter() - started
    values = np.asarray(image)
    if not np.isfinite(values).all():
        raise ValueError("Nonfinite palette control")
    history = dr.kernel_history()
    fields = (
        "size",
        "operation_count",
        "execution_time",
        "codegen_time",
        "backend_time",
    )
    report = proof | {
        "lookup": args.lookup,
        "facades": args.count,
        "scene_sha256": hashlib.sha256(args.scene.read_bytes()).hexdigest(),
        "template_mesh_sha256": hashlib.sha256(args.mesh.read_bytes()).hexdigest(),
        "setup_seconds": setup,
        "render_seconds": elapsed,
        "max_process_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "projection": projection,
        "kernels": [{key: row[key] for key in fields if key in row} for row in history],
        "scope": "Small 64x36/1spp repeated diagnostic bodies with real source palette. Isolates dispatch scaling; not a Place, native or GPU result.",
    }
    args.output.mkdir(parents=True, exist_ok=True)
    np.save(args.output / "radiance.npy", values)
    (args.output / "cost.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: report[key]
                for key in (
                    "lookup",
                    "facades",
                    "setup_seconds",
                    "render_seconds",
                    "max_process_rss_kib",
                )
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--lookup", choices=("shape", "triangle"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
