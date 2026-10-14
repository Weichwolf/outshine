"""Full and clipped genuine two-level bodies, equal before/after cameras and scale."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from ground_surface_showcase import body, clipped, combined
from overpass_checks import control
from overpass_recipe import build
from overpass_ribbon import ribbon, source_frame
from surface_solid_showcase import inspection_view, write_case

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUTPUT = ROOT / "build/experiments/transport/overpasses"


def original_surface(source):
    frame = source_frame(source["xy"], source["width_m"])
    table = np.c_[
        frame["station"],
        np.interp(
            frame["station"], [0.0, frame["station"][-1]], source["end_heights_m"]
        ),
    ]
    return ribbon(frame, table, source["width_m"], source["crossfall"])


def views(configurations, name, case, *, detail=False):
    after, report = build(case)
    if after is None:
        raise ValueError("showcase case has no valid physical clearance")
    before = (original_surface(case["upper"]), after[1])
    centre = np.asarray(report["clearance"]["witness_xy"])
    region = shapely.box(*(centre - 30), *(centre + 30))
    records = []
    for state, meshes in (("before", before), ("after", after)):
        pieces, welds = [], []
        for role, mesh, depth, colour in (
            ("upper", meshes[0], 0.35, 3 if state == "after" else 4),
            ("lower", meshes[1], 0.25, 0),
        ):
            slots = np.full(len(mesh.triangles), colour, dtype=int)
            if detail:
                mesh, slots, displacement = clipped(mesh, slots, region)
                welds.append(displacement)
            pieces.append(body(mesh, slots, depth))
        model, colours = combined(pieces)
        metadata = dict(
            report,
            state=state,
            sources=case,
            clipping_weld_m=max(welds, default=0.0),
            scope="Independent two-source local level recipe. Original XY/end-height targets; no terrain, piers/load analysis, rail hardware, portals, C1 horizontal path, driving or neighboring-world proof. Before is deliberately invalid DEM-end interpolation, after has full strip/deck-underside clearance. Class widths/headroom are recipe assumptions; no height exaggeration.",
        )
        if detail:
            metadata["displayed_xy_window_m"] = list(region.bounds)
        records.append((state, model, colours, metadata))
    axis = np.asarray(case["upper"]["xy"][-1]) - case["upper"]["xy"][0]
    axis /= np.linalg.norm(axis)
    view = inspection_view(
        np.vstack([model.vertices for _, model, _, _ in records]),
        eye_direction=(-axis[1], axis[0], 0.15),
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
    cases = json.loads((HERE / "fixtures/overpass-vienna-cases.json").read_text())[
        "cases"
    ]
    case = next(c for c in cases if c["id"] == "Vienna-Donau-crossing-49")
    configurations = []
    views(configurations, "two-level-control", control(240.0))
    views(configurations, "Vienna-overpass", case)
    views(configurations, "Vienna-overpass-detail", case, detail=True)
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
    (OUTPUT / "origin.json").write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                ).strip(),
                "working_tree_dirty": bool(
                    subprocess.check_output(
                        [
                            "git",
                            "status",
                            "--porcelain",
                            "--",
                            "lib",
                            "test/experiments/transport",
                        ],
                        cwd=ROOT,
                        text=True,
                    )
                ),
                "records": records,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
