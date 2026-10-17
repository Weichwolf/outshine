"""Matched actual whole-building before/after images of the new facade programmes."""

import argparse
from dataclasses import asdict
import json
from pathlib import Path

import numpy as np
import shapely

from demand import View
from evidence import revision
from industrial_relief_probe import camera
from outshine_ref.buildings.rooflight_panels import triangle_surface
from podium_probe import write
from stepped_source_probe import overview
from terraced_facade_geometry import build_facade
from terraced_facade_access import check_arrival
from terraced_mass_plan import plan
from terraced_mass_room_probe import draw
from terraced_mass_rooms import build
from terraced_mass_sources import sources


def facade_draw(asset, mass):
    body = draw(asset.floors, mass.core, False)
    frames = triangle_surface(asset.frames, 10)
    frames["materials"] = asset.frames["materials"]
    return {
        key: np.r_[
            body[key],
            frames[key] + len(body["vertices"]) if key == "faces" else frames[key],
        ]
        for key in body
    }


def materials():
    palette = {
        9: (0.69, 0.65, 0.57, 0.75, 0.0),
        10: (0.085, 0.10, 0.11, 0.32, 0.65),
        11: (0.44, 0.46, 0.46, 0.58, 0.12),
        12: (0.74, 0.72, 0.67, 0.78, 0.0),
    }
    return {
        str(key): dict(
            zip(
                ("color.r", "color.g", "color.b", "roughness", "metallic"),
                value,
                strict=True,
            )
        )
        for key, value in palette.items()
    }


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the facade experiment before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index in (0, 3):
        row = sources()[index]
        for kind in ("cascade", "paired-wings"):
            mass = plan(row, kind)
            asset = build_facade(mass)
            arrival = check_arrival(mass, asset)
            after = facade_draw(asset, mass)
            before = draw(build(mass), mass.core, False)
            view = overview(after["vertices"], [1, -1.25, 0.85])
            for label, mesh in (("Before", before), ("After", after)):
                name = f"{index}-{kind}{label}"
                write(
                    output,
                    name,
                    mesh,
                    {
                        "camera": camera(view),
                        "source_input": {
                            "id": row["id"],
                            "properties": row["properties"],
                            "footprint_wkb": row["polygon"].wkb.hex(),
                        },
                        "facade": json.loads(
                            json.dumps(asdict(asset.facade), default=lambda v: v.hex())
                        ),
                        "actual_complete_arrival": arrival,
                        "source": {"material_definitions": materials()},
                        "facade_materials": {},
                        "ground_span_m": 10000,
                        "scope": "Same actual new mass/open-floor/core/entrance programme. After adds true window holes, closed procedural frames/panes, horizontal bands or vertical piers. No terrace doors/rails, neighbor/city/cache/native/night admission.",
                    },
                    proof,
                )
            if index == 0:
                near(output, name, mass, asset)


def near(output, whole_name, mass, asset):
    anchor = np.asarray(shapely.from_wkb(mass.source_wkb).centroid.coords[0])
    w = min(
        (w for w in asset.facade.windows if w.storey == 1 and w.edge[0] == 0),
        key=lambda w: np.linalg.norm(np.asarray(w.centre) - anchor),
    )
    centre, tangent = np.asarray(w.centre), np.asarray(w.tangent)
    outward = np.array([tangent[1], -tangent[0]])
    target = np.r_[centre - outward * 0.10, w.bottom_m + w.height_m / 2]
    view = View(
        np.r_[centre + 4 * outward - 0.8 * tangent, target[2] + 0.45],
        target,
        width=1920,
        height=1080,
        fov_deg=42,
    )
    record = json.loads((output / (whole_name + "-geometry.json")).read_text())
    name = whole_name + "Near"
    record.update(city=name, camera=camera(view), diagnostic_window=w.identifier)
    (output / (name + "-geometry.json")).write_text(json.dumps(record, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
