"""Full source building access and paired occupied doorway views of the same fixed plan."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
from building_vertical import mesh as building_mesh
from door_assembly import mesh as door_mesh
from door_assembly import prepare
from door_furnishing import joint
from evidence import revision
from furniture_mesh import build as furnishings
from interior_light_plan import lights
from office_support_plan import refine
from opening_details import attach, expand, slots
from room_programme_plan import CONTROLS, source_plan
from room_programme_probe import MATERIALS, combine
from verify_furniture import verify


def run(output, render=False, spp=32):
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for site, identifier in CONTROLS:
        _row, original, receipt = source_plan(site, identifier)
        plan = refine(original, door_aware=True)
        product, selection = joint(plan)
        selected = selection["selected"]
        proof = verify(plan, product, selected)
        record = {
            "site": site,
            "id": identifier,
            "receipt": receipt,
            "proof": proof,
            "selected_ids": [row["leaf"].id for row in selected],
            "unresolved": selection["unresolved"],
            "views": [],
        }
        records.append(record)
        print(site, len(selected), "doors; full open access passed", flush=True)
        if not render or site != "BerlinFernsehturm":
            continue
        body = attach(building_mesh(plan), expand(slots(plan)))
        furniture, _ = furnishings(product)
        door = next(row for row in selected if row["kind"] == "tea")
        leaf = door["leaf"]
        centre, inward = np.asarray(leaf.centre), np.asarray(leaf.inward)
        eye = np.r_[centre - inward * 0.85, leaf.floor_z_m + 1.55]
        target = np.r_[centre + inward * 0.90, leaf.floor_z_m + 1.35]
        light_state = lights(plan, "all-lit", circulation_portals=True)
        fixed = prepare(selected)
        for label, angle in (("closed", 0.0), ("open", 90.0)):
            scene_mesh = combine(
                attach(body, door_mesh(selected, angle, fixed=fixed)), furniture
            )
            stem = site + "-support-door-" + label
            path = output / (stem + "-mesh.npz")
            np.savez_compressed(path, **scene_mesh)
            config = {
                "profile": [1920, 1080],
                "mesh": str(path.resolve()),
                "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "eye": eye.tolist(),
                "target": target.tolist(),
                "fov_deg": 72.0,
                "wall_linear": [0.68, 0.67, 0.64],
                "roof_linear": [0.22, 0.24, 0.25],
                "material_definitions": MATERIALS,
                "ground_span_m": receipt["span_m"] * 1.5,
                "spp": spp,
                "seed": 17,
                "denoise": True,
                "night": True,
                "room_lights": light_state,
                "output": str((output / (stem + ".png")).resolve()),
            }
            scene = output / (stem + "-scene.json")
            scene.write_text(json.dumps(config, indent=2) + "\n")
            with (output / (stem + ".log")).open("w") as log:
                subprocess.run(
                    [
                        sys.executable,
                        str(Path(__file__).with_name("mitsuba_interior.py")),
                        str(scene),
                    ],
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    check=True,
                    timeout=900,
                )
            renderer = json.loads((output / (stem + ".renderer.json")).read_text())
            record["views"].append(
                {
                    "state": label,
                    "angle_deg": angle,
                    "renderer": renderer,
                    "door_id": leaf.id,
                    "eye": eye.tolist(),
                    "target": target.tolist(),
                }
            )
            print(stem, "rendered", flush=True)
        (output / "progress.json").write_text(
            json.dumps(revision() | {"cases": records}, indent=2) + "\n"
        )
    (output / "door-assembly.json").write_text(
        json.dumps(
            revision()
            | {
                "cases": records,
                "scope": "Selected support doors and complete source building with fixed furniture; source omissions remain explicit. Two paired fixed-camera night views are private controls, not new public whole tiles or native runtime admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    parser.add_argument("--spp", type=int, default=32)
    args = parser.parse_args()
    run(args.output, args.render, args.spp)
