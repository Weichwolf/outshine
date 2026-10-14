"""Actual old field and both complete extensions under three identical cameras."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from bridge_continuation_experiment import OUTPUT, inputs
from bridge_continuation_recipe import build
from capture_bridge_continuations import FIXTURE
from ground_surface_showcase import body, clipped, combined
from retained_surface import surface_domain
from surface_solid_showcase import inspection_view, write_case

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def models(records, old, region):
    result = []
    original = surface_domain(old[0])
    for state, meshes, note in records:
        parts = []
        welds = []
        for kind, mesh, depth in (
            ("top", meshes[0], 0.35),
            ("ground", meshes[1], 0.25),
        ):
            centre = mesh.vertices[mesh.triangles, :2].mean(axis=1)
            slots = (
                np.where(shapely.dwithin(original, shapely.points(centre), 1e-8), 3, 1)
                if kind == "top"
                else np.zeros(len(mesh.triangles), int)
            )
            if region is not None:
                mesh, slots, displacement = clipped(mesh, slots, region)
                welds.append(displacement)
            if len(mesh.triangles):
                parts.append(body(mesh, slots, depth))
        if not parts:
            raise ValueError("comparison camera shows no actual current geometry")
        model, colours = combined(parts)
        result.append(
            (state, model, colours, dict(note, clipping_weld_m=max(welds, default=0)))
        )
    return result


def views(configurations, name, records, old, region):
    cases = models(records, old, region)
    view = inspection_view(
        np.vstack([case[1].vertices for case in cases]), eye_direction=(-0.3, -1, 0.6)
    )
    for state, model, colours, note in cases:
        note = dict(
            note,
            state=state,
            display_scope="Actual old plan versus complete compatible three-tile rail field. Blue: previously planned upper area, green: added rail area by face centre, grey: known lower outside traffic surfaces. No height exaggeration. Indoor/tunnel/other unresolved contacts, outside routes, terrain/water, rail hardware, piers and structural load remain uncertified.",
        )
        if region is not None:
            note["displayed_xy_window_m"] = list(region.bounds)
        write_case(
            "Vienna-continuation-" + name + "-" + state,
            model,
            colours,
            note,
            configurations,
            view=view,
            output_dir=OUTPUT,
        )


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    captured, approaches, root, old, _used = inputs()
    records = [("before", old, {"state": "previous_valid_west_approach_plan"})]
    for method in ("rebuild", "retain"):
        meshes, note = build(
            captured["sources"], approaches, root, old, geometry_method=method
        )
        if not note["valid"]:
            raise ValueError(note["reason"])
        records.append((method, meshes, note))
    configurations = []
    views(configurations, "whole", records, old, None)
    views(configurations, "cut", records, old, shapely.box(3720, -95, 3900, 85))
    views(configurations, "rise", records, old, shapely.box(3780, -220, 4080, 20))
    scene = OUTPUT / "scenes.json"
    scene.write_text(json.dumps(configurations, indent=2) + "\n")
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scene),
        ],
        cwd=ROOT,
        check=True,
    )
    hashes = {}
    for config in configurations:
        stem = OUTPUT / (config["id"] + "-solid")
        hashes[stem.name] = {
            "image_sha256": hashlib.sha256(
                stem.with_suffix(".png").read_bytes()
            ).hexdigest(),
            "report_sha256": hashlib.sha256(
                stem.with_suffix(".json").read_bytes()
            ).hexdigest(),
        }
    origin = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "renderer_sha256": hashlib.sha256(
            (HERE / "render_transport_solid.py").read_bytes()
        ).hexdigest(),
        "records": hashes,
    }
    (OUTPUT / "origin.json").write_text(json.dumps(origin, indent=2) + "\n")
    print(
        json.dumps({"renderings": len(hashes), "origin": str(OUTPUT / "origin.json")})
    )


if __name__ == "__main__":
    main()
