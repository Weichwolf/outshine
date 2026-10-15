"""Private 2.5D and occupied interior controls from the same source-bound programme."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from building_surface_roles import apply as surface_roles
from building_vertical import level
from building_vertical import mesh as building_mesh
from evidence import revision
from furniture_mesh import build as furnishings
from interior_light_plan import lights
from opening_details import attach, expand, slots
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from stepped_source_probe import overview
from tile_room_details import corner_buffers
from verify_furniture import verify

MATERIALS = {
    "40": dict(
        shader="CityEnginePBRShader",
        **{"color.r": 0.48, "color.g": 0.31, "color.b": 0.15},
        roughness=0.65,
        metallic=0.0,
    ),
    "41": dict(
        shader="CityEnginePBRShader",
        **{"color.r": 0.22, "color.g": 0.25, "color.b": 0.27},
        roughness=0.32,
        metallic=0.7,
    ),
    "42": dict(
        shader="CityEnginePBRShader",
        **{"color.r": 0.18, "color.g": 0.39, "color.b": 0.36},
        roughness=0.9,
        metallic=0.0,
    ),
    "43": dict(
        shader="CityEnginePBRShader",
        **{"color.r": 0.05, "color.g": 0.08, "color.b": 0.10},
        roughness=0.18,
        metallic=0.1,
    ),
    "44": dict(
        shader="CityEnginePBRShader",
        **{"color.r": 0.81, "color.g": 0.79, "color.b": 0.72},
        roughness=0.35,
        metallic=0.0,
    ),
}


def combine(body, detail, cutaway=False):
    selected = (
        body["materials"] != 2 if cutaway else np.ones(len(body["faces"]), dtype=bool)
    )
    return corner_buffers(
        {
            "vertices": np.r_[body["vertices"], detail["vertices"]],
            "faces": np.r_[
                body["faces"][selected], detail["faces"] + len(body["vertices"])
            ],
            "materials": np.r_[body["materials"][selected], detail["materials"]],
        },
        1,
    )


def cameras(plan, product, full, cutaway):
    view = overview(full["vertices"], [1.0, -1.0, 1.5])
    work = next(
        r
        for r in product["rooms"]
        if r["kind"] in ("work", "work-tea") and r["result"]["items"]
    )
    current = level(plan, work["storey"])
    room = current["rooms"][work["room_index"]]
    eye = np.r_[work["portal_anchors"][0], work["floor_z_m"] + 1.60]
    target = np.r_[
        np.asarray(room.representative_point().coords[0]), work["floor_z_m"] + 1.10
    ]
    window = next(w for w in current["windows"] if work["room_index"] in w["rooms"])
    tangent = np.asarray(window["tangent"])
    outside = np.r_[
        np.asarray(window["centre"]) + 3.0 * np.array([tangent[1], -tangent[0]]),
        work["floor_z_m"] + 1.50,
    ]
    aperture = np.r_[window["centre"], work["floor_z_m"] + 1.45]
    return (
        ("2_5d", cutaway, view.eye, view.target, False),
        ("interior", full, eye, target, False),
        ("night", full, eye, target, True),
        ("window-night", full, outside, aperture, True),
    )


def probe(output, site, identifier, spp, support_plan=False, overview_only=False):
    output.mkdir(parents=True, exist_ok=True)
    _row, plan, receipt = source_plan(site, identifier)
    subdivision = None
    if support_plan:
        from office_support_plan import refine
        from office_support_verification import admission

        original = plan
        plan = refine(original)
        subdivision = admission(original, plan)
    product = programme(plan, "jittered-anchors")
    proof = verify(plan, product)
    detail, instances = furnishings(product)
    body = attach(building_mesh(plan), expand(slots(plan)))
    section = None
    if support_plan:
        from office_support_cutaway import cutaway as support_cutaway

        cutaway, section = support_cutaway(plan, product, body)
    body, roles = surface_roles(plan, body)
    full = combine(body, detail)
    if not support_plan:
        cutaway = combine(body, detail, True)
    provenance = revision()
    names = []
    views = cameras(plan, product, full, cutaway)
    if support_plan:
        view = overview(cutaway["vertices"], [1.0, -1.0, 1.5])
        views = (("2_5d", cutaway, view.eye, view.target, False), *views[1:])
    for name, mesh, eye_point, target_point, night in views:
        if overview_only and name != "2_5d":
            continue
        stem = "room-programme-" + name
        path = output / (stem + "-mesh.npz")
        np.savez_compressed(path, **mesh)
        config = {
            "profile": [1920, 1080],
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "eye": np.asarray(eye_point).tolist(),
            "target": np.asarray(target_point).tolist(),
            "fov_deg": 48.0,
            "wall_linear": [0.68, 0.67, 0.64],
            "roof_linear": [0.22, 0.24, 0.25],
            "material_definitions": MATERIALS,
            "ground_span_m": receipt["span_m"] * 1.5,
            "spp": spp,
            "seed": 17,
            "denoise": True,
            "night": night,
            "room_lights": lights(plan, "all-lit" if night else "off"),
            "output": str((output / (stem + ".png")).resolve()),
        }
        scene = output / (stem + "-scene.json")
        scene.write_text(json.dumps(config, indent=2) + "\n")
        log = Path(tempfile.gettempdir()) / (
            "outshine-buildings-"
            + stem
            + "-"
            + site
            + "-"
            + provenance["commit"][:9]
            + ".log"
        )
        with log.open("w") as stream:
            subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).with_name("mitsuba_interior.py")),
                    str(scene),
                ],
                stdout=stream,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=900,
            )
        renderer = json.loads((output / (stem + ".renderer.json")).read_text())
        report = provenance | {
            "id": identifier,
            "view": name,
            "programme_status": product["status"],
            "evidence": product["evidence"],
            "proof": proof,
            "instances": instances,
            "furnishing_methods": {
                r["room_id"]: r["placement_method"] for r in product["rooms"]
            },
            "roles": roles,
            "support_subdivision": subdivision,
            "section": section if name == "2_5d" else None,
            "renderer": renderer,
            "profile": [1920, 1080],
            "image_sha256": renderer["image_sha256"],
            "scope": "Private actual office-POI control with own procedural furnishings. Section metadata declares the support ground-floor cut; legacy 2.5D removes ceiling only. Interior eye uses an actual compiled actor-checked portal anchor. Complete architecture recipe families, door privacy, services, ventilation and native LOD remain open",
        }
        record = output / (stem + "-report.json")
        record.write_text(json.dumps(report, indent=2) + "\n")
        names.append(record.name)
        print(site, name, "rendered", flush=True)
    (output / "reports.json").write_text(
        json.dumps(provenance | {"reports": names}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/room-programme-probe"),
    )
    parser.add_argument("--site", default=CONTROLS[1][0])
    parser.add_argument("--id", default=CONTROLS[1][1])
    parser.add_argument("--spp", type=int, default=32)
    parser.add_argument("--support-plan", action="store_true")
    parser.add_argument("--overview-only", action="store_true")
    args = parser.parse_args()
    probe(
        args.output, args.site, args.id, args.spp, args.support_plan, args.overview_only
    )
