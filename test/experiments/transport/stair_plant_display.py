"""Whole 720p actual transport/buildings plus complete physically emitted plants."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from stair_occupancy_display import whole_camera
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private whole-plant diagnostics require /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    report = json.loads((args.products / "comparison.json").read_text())
    receipt = json.loads(args.receipt.read_text())
    if (
        receipt["report_sha256"] != digest(args.products / "comparison.json")
        or not receipt["all_physical_bounds_and_SITE_products_verified"]
    ):
        raise ValueError("fresh same-product whole-plant verification required")
    for name in ("plants.json", "prototype/complete.npz"):
        if digest(args.products / name) != report["artifacts"][name]:
            raise ValueError("actual whole-plant display input changed")
    accepted = json.loads((args.products / "plants.json").read_text())
    with np.load(args.products / "prototype/complete.npz", allow_pickle=False) as data:
        vertices, faces, leaf = data["vertices"], data["faces"], data["leaf_faces"]
    bodies = [
        Mesh(vertices + row["result"]["world_origin_m"], faces) for row in accepted
    ]
    scene = json.loads(args.scene.read_text())
    if scene["profile"] != [1280, 720] or scene.get("detail_view"):
        raise ValueError("whole 1280x720 architecture reference required")
    base_path = Path(scene["mesh"])
    if digest(base_path) != scene["mesh_sha256"]:
        raise ValueError("actual architecture/transport reference changed")
    with np.load(base_path, allow_pickle=False) as data:
        base = {k: data[k] for k in data.files}
    scene = whole_camera(scene, base["vertices"], bodies)
    palette = np.array(
        [*scene["colours"], [0.26, 0.15, 0.07, 1], [0.055, 0.22, 0.075, 1]]
    )
    wood_slot, leaf_slot = len(palette) - 2, len(palette) - 1
    path = args.output / "base.npz"
    np.savez_compressed(path, **base)
    common = {
        "id": "Wien-transport-buildings",
        "mesh": str(path.resolve()),
        "mesh_sha256": digest(path),
        "colours": palette.tolist(),
    }
    plant_rows = []
    empty = {key: value[:0] for key, value in base.items()}
    slots = np.where(leaf, leaf_slot, wood_slot)
    for i, body in enumerate(bodies):
        shown = append(empty, [(body, leaf_slot)], palette)
        shown["materials"][:] = slots
        shown["face_base_colour_linear_rgba"][:] = palette[slots]
        path = args.output / f"plant-{i}.npz"
        np.savez_compressed(path, **shown)
        plant_rows.append(
            {
                "id": f"physical-plant-{i}",
                "mesh": str(path.resolve()),
                "mesh_sha256": digest(path),
                "colours": palette.tolist(),
            }
        )
    for phase, parts in (("before", [common]), ("after", [common, *plant_rows])):
        row = dict(
            scene,
            id="Wien-actual-plants-" + phase,
            mesh=common["mesh"],
            mesh_sha256=common["mesh_sha256"],
            colours=palette.tolist(),
            bodies=parts,
            output=str((args.output / (phase + ".png")).resolve()),
        )
        (args.output / (phase + "-scene.json")).write_text(
            json.dumps(row, indent=2) + "\n"
        )
    (args.output / "provenance.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "producer_commit": report["commit"],
                "plant_report_sha256": digest(args.products / "comparison.json"),
                "architecture_scene_sha256": digest(args.scene),
                "actual_plants": len(bodies),
                "scope": "Complete emitted Float64 wood/short-shoot/needle geometry; renderer converts to Float32, subpixel organs may collapse. Whole 1280x720, no closeup/galerie.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
