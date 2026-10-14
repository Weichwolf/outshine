"""Current deck versus two genuine shared approaches, full and contact detail."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from bridge_approach_experiment import OUTPUT, inputs
from bridge_approach_recipe import build
from ground_surface_showcase import body, clipped, combined
from surface_solid_showcase import inspection_view, write_case

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def views(configurations, captured, base, *, detail=False):
    alternatives = []
    for method in ("parabolic", "cubic"):
        mesh, note = build(captured["approaches"], *base, method=method)
        if mesh is None:
            raise ValueError("showcase has no valid source approach")
        alternatives.append((method, mesh, note))
    profile = alternatives[0][2]["profile"]
    axis = np.asarray(profile["direction_xy"])
    centre = (
        np.asarray(profile["origin_xy"]) + axis * profile["transition_station_m"][0]
    )
    region = shapely.box(*(centre - 60), *(centre + 60))
    old_domain = shapely.coverage_union_all(
        shapely.polygons(base[0].vertices[base[0].triangles, :2])
    )
    cases = [
        ("before", base[0], {"state": "current_reference_without_neighbor_approaches"})
    ] + alternatives
    records = []
    for state, top, note in cases:
        positions = top.vertices[top.triangles, :2].mean(axis=1)
        slots = np.where(shapely.covers(old_domain, shapely.points(positions)), 3, 1)
        ground_slots = np.zeros(len(base[1].triangles), dtype=int)
        shown_top, shown_ground = top, base[1]
        welds = []
        if detail:
            shown_top, slots, error = clipped(top, slots, region)
            welds.append(error)
            shown_ground, ground_slots, error = clipped(base[1], ground_slots, region)
            welds.append(error)
        pieces = [body(shown_top, slots, 0.35)]
        if len(shown_ground.triangles):
            pieces.append(body(shown_ground, ground_slots, 0.25))
        model, colours = combined(pieces)
        report = dict(
            note,
            state=state,
            source_approaches=captured["approaches"],
            unresolved_extra_contacts=captured["conflicts"],
            clipping_weld_m=max(welds, default=0),
            display_scope="Actual current joined deck before; after adds real west bridge/ground subway sources using a shared height field. Green: approaches, blue: deck, grey: known lower outdoor surfaces. No height exaggeration. Unresolved indoor/tunnel/elevated paths, other tile borders, terrain, rail hardware, piers/load and driving remain outside the certificate.",
        )
        if detail:
            report["displayed_xy_window_m"] = list(region.bounds)
        records.append((state, model, colours, report))
    view = inspection_view(
        np.vstack([r[1].vertices for r in records]),
        eye_direction=(-axis[1], axis[0], 0.20),
    )
    name = "Vienna-approach-detail" if detail else "Vienna-approach"
    for state, model, colours, report in records:
        write_case(
            name + "-" + state,
            model,
            colours,
            report,
            configurations,
            view=view,
            output_dir=OUTPUT,
        )


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    captured, _sources, base, _report = inputs()
    configurations = []
    views(configurations, captured, base)
    views(configurations, captured, base, detail=True)
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
    records = {}
    for config in configurations:
        stem = OUTPUT / (config["id"] + "-solid")
        records[stem.name] = {
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
        "fixture_sha256": hashlib.sha256(
            (HERE / "fixtures/bridge-approaches-vienna.json").read_bytes()
        ).hexdigest(),
        "records": records,
        "renderer_sha256": hashlib.sha256(
            (HERE / "render_transport_solid.py").read_bytes()
        ).hexdigest(),
    }
    (OUTPUT / "origin.json").write_text(json.dumps(origin, indent=2) + "\n")
    print(
        json.dumps({"origin": str(OUTPUT / "origin.json"), "renderings": len(records)})
    )


if __name__ == "__main__":
    main()
