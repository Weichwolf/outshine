"""Diagnostic full-body export from the resident patch view, never a cache-hit path."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from stair_module_display import display
from stair_module_inputs import read_arrays
from stair_module_materials import colour_component
from stair_patch_inputs import load_prepared, whole
from stair_terrain_experiment import frozen
from stair_terrain_solid import retaining_body
from wien_support_verify import unpack


def run(args):
    frozen()
    view, _manifest = load_prepared(args.prepared)
    result = whole(view)
    mesh, data = result.mesh, result.face_data
    body, parts = retaining_body(
        {
            "xy": mesh.vertices[:, :2],
            "faces": mesh.triangles,
            "ground": data["ground"],
            "targets": mesh.vertices[:, 2],
            "bed": mesh.vertices[:, 2],
        }
    )
    original = read_arrays(args.reference / "world-body.npz")
    if not np.array_equal(body.vertices, original["vertices"]) or not np.array_equal(
        body.triangles, original["faces"]
    ):
        raise ValueError("diagnostic body differs from actual original whole body")
    scene = json.loads((args.supports / "after-overview-scene.json").read_text())
    palette = np.array(scene["colours"])
    shown = read_arrays(args.base / "after/display-body.npz")
    for path in args.module[:-1]:
        module = read_arrays(path / "module.npz")
        first = len(shown["triangles"])
        shown = append(
            shown,
            [(Mesh(module["body_vertices"].astype("f4"), module["body_faces"]), 3)],
            palette,
        )
        colour_component(shown, first, module, palette)
    support = json.loads((args.supports / "comparison.json").read_text())
    piers, _contacts, _ = unpack(args.supports / (support["selected"] + ".npz"))
    state = {
        "road_display": shown,
        "before_display_path": args.reference / "after-display.npz",
        "piers": piers,
        "cache_parent_reports": [True],
    }
    output = args.prepared / "display"
    output.mkdir()
    display(
        output,
        args.base,
        args.supports,
        state,
        read_arrays(args.module[-1] / "module.npz"),
        body,
        parts,
        data["constructed"][~data["ground"]],
    )
    expected = read_arrays(args.reference / "after-display.npz")
    actual = read_arrays(output / "after-display.npz")
    if set(expected) != set(actual) or any(
        expected[k].tobytes() != actual[k].tobytes() for k in expected
    ):
        raise ValueError("diagnostic presentation changed original actual buffers")
    print("exact closed body and presentation arrays", flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    parser.add_argument("--module", type=Path, action="append", default=[])
    run(parser.parse_args())
