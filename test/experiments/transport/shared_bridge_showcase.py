"""Actual fixed-port omission versus a shared elevated deck; paired cameras."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from ground_surface_showcase import body, clipped, combined
from shared_bridge_checks import FIXTURE, controls
from shared_bridge_recipe import build
from surface_solid_showcase import inspection_view, write_case

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUTPUT = ROOT / "build/experiments/transport/shared-bridge"


def views(configurations, name, sources, *, detail=False):
    after, note = build(sources)
    before, failure = build(sources, port_lift_m=0)
    if not note["valid"] or failure["reason"] != "deck_port_height_budget":
        raise ValueError(
            "showcase requires a checked deck and an actual fixed-port omission"
        )
    centre = np.asarray(note["clearance"]["witness_xy"])
    region = shapely.box(*(centre - 40), *(centre + 40))
    records = []
    for state, meshes, report in (("fixed", before, failure), ("shared", after, note)):
        pieces, welds = [], []
        for mesh, depth, colour in ((meshes[0], 0.35, 3), (meshes[1], 0.25, 0)):
            if mesh is None:
                continue
            slots = np.full(len(mesh.triangles), colour, dtype=int)
            if detail:
                mesh, slots, displacement = clipped(mesh, slots, region)
                welds.append(displacement)
            pieces.append(body(mesh, slots, depth))
        model, colours = combined(pieces)
        metadata = dict(
            report,
            state=state,
            deck_present=meshes[0] is not None,
            source_scope=sources,
            clipping_weld_m=max(welds, default=0),
            display_scope="Fixed: the actual common-plan result omits the deck because every original DEM endpoint is held. Shared: one constant joined deck with each endpoint rise capped at 8 m; known outdoor ground kept. No height exaggeration, terrain, piers, structural load, rail hardware, horizontal route or outside ramp proof. Indoor paths have no height certificate and are not rendered as floors.",
        )
        if detail:
            metadata["displayed_xy_window_m"] = list(region.bounds)
        records.append((state, model, colours, metadata))
    upper = max(
        (s for s in sources if s["role"] == "deck"),
        key=lambda s: shapely.LineString(s["xy"]).length,
    )
    axis = np.asarray(upper["xy"][-1], float) - upper["xy"][0]
    axis /= np.linalg.norm(axis)
    view = inspection_view(
        np.vstack([r[1].vertices for r in records]),
        eye_direction=(-axis[1], axis[0], 0.22),
    )
    for state, model, colours, metadata in records:
        write_case(
            name + "-" + state,
            model,
            colours,
            metadata,
            configurations,
            view=view,
            output_dir=OUTPUT,
        )


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sources = json.loads(FIXTURE.read_text())["sources"]
    configurations = []
    views(configurations, "shared-control", controls())
    views(configurations, "Vienna-shared-bridge", sources)
    views(configurations, "Vienna-shared-bridge-detail", sources, detail=True)
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
        check=True,
        cwd=ROOT,
    )
    records = {}
    for row in configurations:
        stem = OUTPUT / (row["id"] + "-solid")
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
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
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
