"""Render the actual previous plan, both portal recipes and an open lining inspection."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from capture_tunnel_portals import FIXTURE, PIN, base_surfaces
from ground_surface_showcase import body, clipped, combined
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_showcase import inspection_view, write_case
from tunnel_portal_recipe import HEADROOM_M, ROOF_M, build
from whole_tile_showcase import COLOURS

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/tunnel-portals"
PALETTE = COLOURS


def inputs():
    if hashlib.sha256(FIXTURE.read_bytes()).hexdigest() != PIN:
        raise ValueError("pinned portal input changed")
    fixture = json.loads(FIXTURE.read_text())
    previous, origin = base_surfaces()
    if origin != fixture["base"]:
        raise ValueError("previous whole plan differs from the chosen contact input")
    return fixture, previous


def assembled(model, previous, *, region=None, cutaway=False):
    original = surface_domain(previous[0])
    parts = []
    for name, depth, offset, colour in (
        ("top", 0.35, 0, -1),
        ("ground", 0.25, 0, 0),
        ("walls", HEADROOM_M, HEADROOM_M, 6),
        ("roof", ROOF_M, 0, 7),
    ):
        if name not in model:
            continue
        surface = model[name]
        centre = surface.vertices[surface.triangles, :2].mean(axis=1)
        slots = (
            np.where(shapely.dwithin(original, shapely.points(centre), 1e-8), 3, 1)
            if colour == -1
            else np.full(len(surface.triangles), colour)
        )
        if region is not None:
            surface, slots, _weld = clipped(surface, slots, region)
        if cutaway and name == "roof":
            # A display-only lateral cut reveals the true lining and floor surfaces.
            lo, hi = (
                surface.vertices[:, :2].min(axis=0),
                surface.vertices[:, :2].max(axis=0),
            )
            surface, slots, _weld = clipped(
                surface,
                slots,
                shapely.box(lo[0] - 1, (lo[1] + hi[1]) / 2, hi[0] + 1, hi[1] + 1),
            )
        if len(surface.triangles):
            surface = Mesh(surface.vertices + [0, 0, offset], surface.triangles)
            parts.append(body(surface, slots, depth))
    if not parts:
        raise ValueError("empty portal inspection")
    if cutaway and region is not None:
        terrain = model["terrain"]
        display = region.difference(model["open_trench"])
        lo, hi = np.array(region.bounds[:2]), np.array(region.bounds[2:])
        display = display.intersection(
            shapely.box(lo[0], (lo[1] + hi[1]) / 2, hi[0], hi[1])
        )
        terrain, colours, _weld = clipped(
            terrain, np.full(len(terrain.triangles), 5), display
        )
        parts.append(body(terrain, colours, 0.05))
    return combined(parts)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    fixture, previous = inputs()
    records = [
        (
            "before",
            {"top": previous[0], "ground": previous[1]},
            {
                "valid": True,
                "scope": "Previous verified bridge/ground partial plan before tunnel portals.",
            },
        )
    ]
    for method in ("parabolic", "cubic"):
        model, note = build(fixture, previous, method=method)
        if not note["valid"]:
            raise ValueError(note["reason"])
        records.append((method, model, note))
    cases = [
        (state, *assembled(model, previous), note) for state, model, note in records
    ]
    view = inspection_view(
        np.vstack([case[1].vertices for case in cases]),
        eye_direction=(-0.3, -1.0, 0.85),
    )
    view["colours"] = PALETTE
    configurations = []
    for state, mesh, slots, note in cases:
        note = dict(
            note,
            height_exaggeration=1.0,
            input_sha256=PIN,
            source_origin=fixture["base"],
        )
        write_case(
            "Vienna-tunnel-whole-" + state,
            mesh,
            slots,
            note,
            configurations,
            view=view,
            output_dir=OUTPUT,
        )
    state, model, note = records[-1]
    origin, axis, _table = model["field"]
    portal = origin + axis * note["field"]["roof_start_m"]
    region = shapely.box(*(portal - [65, 35]), *(portal + [65, 35]))
    mesh, slots = assembled(model, previous, region=region, cutaway=True)
    detail = inspection_view(mesh.vertices, eye_direction=(0.8, -0.8, 0.6))
    detail["colours"] = PALETTE
    write_case(
        "Vienna-tunnel-portal-" + state,
        mesh,
        slots,
        dict(
            note,
            display_only_cutaway=True,
            terrain_display_thickness_m=0.05,
            terrain_display_is_earth_volume=False,
            scope=note["scope"],
        ),
        configurations,
        view=detail,
        output_dir=OUTPUT,
    )
    scene = OUTPUT / "scenes.json"
    scene.write_text(json.dumps(configurations, indent=2) + "\n")
    for configuration in configurations:
        Path(configuration["output"]).unlink(missing_ok=True)
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
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
        cwd=ROOT,
        text=True,
    ).strip()
    records = {}
    for configuration in configurations:
        base = OUTPUT / f"{configuration['id']}-solid"
        records[base.name] = {
            "image_sha256": hashlib.sha256(
                base.with_suffix(".png").read_bytes()
            ).hexdigest(),
            "report_sha256": hashlib.sha256(
                base.with_suffix(".json").read_bytes()
            ).hexdigest(),
        }
    (OUTPUT / "origin.json").write_text(
        json.dumps(
            {"commit": commit, "working_tree_dirty": bool(dirty), "records": records},
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
