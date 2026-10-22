"""Cook a camera-independent roof field, then adapt saved profile roofs for display."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from bake_roof_normal_field import bake
from compare_far_height_profiles import comparison_views, plan_cell
from full_view_reference import load
from roof_normal_field import roof_uv


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    plans, needs, _, _, plan_s, archive_hash, _ = plan_cell(args, document)
    _, centre, right, up = comparison_views(source, document, context)
    basis = np.stack((right, np.cross(up, right), up))
    scenes = []
    required = set()
    for name in ["static-020", "moving-020", "moving-021", "near-control"]:
        directory = args.profiles / name / "profile"
        scene = json.loads((directory / "scene.json").read_text())
        with np.load(directory / scene["mesh"], allow_pickle=False) as data:
            mesh = {key: data[key] for key in data.files}
        binding = mesh["plan_binding"]
        roof = (mesh["native_binding"][:, 0] < 0) & (binding[:, 2] == 1)
        required.update(map(tuple, binding[roof, :2]))
        scenes.append((name, scene, mesh))
    parts = [
        part
        for ordinal in sorted(plans)
        for part in plans[ordinal]
        if (part.source, part.part) in required
    ]
    started = time.perf_counter()
    field = bake(parts, size=16)
    bake_s = time.perf_counter() - started
    np.savez_compressed(args.output / "field.npz", **field)
    rows = []
    for name, scene, mesh in scenes:
        output = args.output / name
        output.mkdir()
        uv = roof_uv(mesh, field, basis, centre)
        np.savez_compressed(output / "roof-uv.npz", uv=uv)
        old = args.profiles / name / "profile" / scene["mesh"]
        scene["mesh"] = str(old.resolve())
        scene["roof_normal_field"] = {
            "path": str((args.output / "field.npz").resolve()),
            "uv": str((output / "roof-uv.npz").resolve()),
            "normal_basis": basis.tolist(),
            "parent_origin": centre.tolist(),
        }
        scene["scope"] += (
            " Source-owned nearest RGB8 normal-field control. Geometry, original UV/metres/categories and alpha coverage unchanged; normal filtering/NDF and native procedural materials unqualified."
        )
        (output / "scene.json").write_text(json.dumps(scene, indent=2) + "\n")
        rows.append(
            {
                "pose": name,
                "loop_uv_bytes": uv.nbytes,
                "generated_roof_triangles": int(np.all(uv >= 0, axis=(1, 2)).sum()),
                "geometry_sha256": hashlib.sha256(old.read_bytes()).hexdigest(),
            }
        )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "reference_receipt": receipt,
        "archive_sha256": archive_hash,
        "needs": needs,
        "planned_seconds": plan_s,
        "field_bake_seconds": bake_s,
        "tile_resolution": [16, 16],
        "field_arrays": {key: array.nbytes for key, array in field.items()},
        "tiles": len(parts),
        "scenes": rows,
        "scope": "Canonical parent-axis RGB8 normal tiles from original plans, camera/light independent. Per-view adapters reuse identical field and unchanged saved geometry. Added draw UVs and textures counted separately; neither geometric/radiance admission nor native timing/format.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "reference_receipt"}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--profiles", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
