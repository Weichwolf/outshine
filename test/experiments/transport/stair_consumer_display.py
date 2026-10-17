"""Whole 1280x720 architecture/transport diagnostics from exact saved body arrays."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from stair_consumer_verify import meshes
from stair_occupancy_display import whole_camera
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private display outputs must live under /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    report = json.loads((args.products / "comparison.json").read_text())
    verification = json.loads(args.verification.read_text())
    if verification["report_sha256"] != digest(args.products / "comparison.json"):
        raise ValueError("same-product fresh verification required")
    bodies = {}
    for phase in ("before", "after"):
        path = args.products / f"buildings-{phase}.npz"
        if digest(path) != report["artifacts"][path.name]:
            raise ValueError("saved architecture body arrays changed")
        bodies[phase] = meshes(path)
    scene = json.loads(args.scene.read_text())
    if scene["profile"] != [1280, 720] or scene.get("detail_view"):
        raise ValueError("whole 1280x720 scene required")
    base_path = Path(scene["mesh"])
    if digest(base_path) != scene["mesh_sha256"]:
        raise ValueError("reference whole transport scene mesh changed")
    with np.load(base_path, allow_pickle=False) as saved:
        base = {key: saved[key] for key in saved.files}
    scene = whole_camera(scene, base["vertices"], [*bodies["before"], *bodies["after"]])
    palette = np.array([*scene["colours"], [0.52, 0.46, 0.38, 1]])
    receipts = {"commit": commit, "producer": report["commit"], "phases": {}}
    for phase, phase_bodies in bodies.items():
        path = args.output / (phase + ".npz")
        shown = append(base, [(p, len(palette) - 1) for p in phase_bodies], palette)
        np.savez_compressed(path, **shown)
        value = dict(
            scene,
            id="Wien-actual-architecture-" + phase,
            colours=palette.tolist(),
            mesh=str(path.resolve()),
            mesh_sha256=digest(path),
            output=str((args.output / (phase + ".png")).resolve()),
        )
        (args.output / (phase + "-scene.json")).write_text(
            json.dumps(value, indent=2) + "\n"
        )
        receipts["phases"][phase] = {
            "body_count": len(phase_bodies),
            "mesh_sha256": digest(path),
        }
    (args.output / "provenance.json").write_text(json.dumps(receipts, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--verification", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
