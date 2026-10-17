"""Diagnostic full 3D export of verified local assets; never a generator/hit path."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from stair_local_replay import replay
from stair_module_display import display
from stair_module_inputs import read_arrays
from stair_module_materials import colour_component
from stair_patch_inputs import whole
from stair_terrain_experiment import frozen
from stair_terrain_solid import retaining_body


def run(args):
    commit = frozen()
    view, assets, supports, _contacts, manifest, report, saved = replay(
        args.resident, args.local, args.prior_module
    )
    if saved is None:
        raise ValueError(
            "accepted verified local module required for diagnostic export"
        )
    for key in ("base_display", "before_display", "support_scene"):
        if digest(Path(manifest[key])) != manifest[key + "_sha256"]:
            raise ValueError("original diagnostic presentation source changed")
    result = whole(view)
    plan = {
        "xy": result.mesh.vertices[:, :2],
        "faces": result.mesh.triangles,
        "ground": result.face_data["ground"],
        "targets": result.mesh.vertices[:, 2],
        "bed": result.mesh.vertices[:, 2],
    }
    body, parts = retaining_body(plan)
    reference = read_arrays(args.reference / "world-body.npz")
    if not np.array_equal(body.vertices, reference["vertices"]) or not np.array_equal(
        body.triangles, reference["faces"]
    ):
        raise ValueError("diagnostic whole body differs from independent full control")
    palette = np.array(
        json.loads(Path(manifest["support_scene"]).read_text())["colours"]
    )
    shown = read_arrays(Path(manifest["base_display"]))
    for asset in assets[1:-1]:
        first = len(shown["triangles"])
        shown = append(
            shown,
            [
                (
                    Mesh(
                        asset.data["body_vertices"].astype("f4"),
                        asset.data["body_faces"],
                    ),
                    3,
                )
            ],
            palette,
        )
        colour_component(shown, first, asset.data, palette)
    before = args.before or Path(manifest["before_display"])
    if args.prior_module and args.before is None:
        raise ValueError(
            "explicit preceding diagnostic display required after prior modules"
        )
    state = {
        "road_display": shown,
        "before_display_path": before,
        "piers": supports,
        "cache_parent_reports": [True],
    }
    output = args.local / "display"
    if output.exists():
        raise ValueError("fresh diagnostic display output required")
    output.mkdir()
    shown = display(
        output,
        Path(manifest["base_display"]).parents[1],
        Path(manifest["support_scene"]).parent,
        state,
        saved["data"],
        body,
        parts,
        result.face_data["constructed"][~result.face_data["ground"]],
    )
    proof = {
        "commit": commit,
        "local_report_sha256": digest(args.local / "comparison.json"),
        "reference_report_sha256": digest(args.reference / "comparison.json"),
        "exact_independent_whole_body": True,
        "source_count": len(report["candidate_source_ids"]),
        "display": shown,
        "scope": "Full finite export for actual private 3D comparison only; excluded from local generator/replay work.",
    }
    from stair_module_experiment import write_json

    write_json(output / "verification.json", proof)
    print(json.dumps(proof), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "local", "reference"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--before", type=Path)
    run(parser.parse_args())
