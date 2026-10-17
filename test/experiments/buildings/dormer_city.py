"""Actual complete988-source city with the current shared podium/wing assets."""

import argparse
import json
from dataclasses import replace
from pathlib import Path

from city_scene import inputs
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from showcase_tiles import view_for
from tile_buildings import construct
from tile_dormers import DormerFacades
from tile_elevations import ElevationFacades
from tile_podiums import PodiumDetails


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze entire dormer scene before image evidence")
    census = json.loads(
        Path(__file__)
        .with_name("data")
        .joinpath("roof-crown-city-window.json")
        .read_text()
    )
    rows, receipts, window = inputs("Paris", census)
    view = replace(view_for(rows), width=1920, height=1080)
    output.mkdir(parents=True, exist_ok=True)

    def progress(requested, accepted, identifier):
        print(
            json.dumps(
                {"requested": requested, "accepted": accepted, "id": identifier}
            ),
            flush=True,
        )

    owner = PodiumDetails(rows, view, progress=progress)
    for suffix, kind in (("Before", ElevationFacades), ("After", DormerFacades)):
        owner.facades = kind(owner.rows, view)
        owner.completed.clear()
        owner.rejected.clear()
        mesh, source = construct(
            "Paris",
            prepared=(owner.rows, {"invalid": [], "span_m": window["width_m"]}),
            ordinary_builder=owner,
        )
        if source["generated"] != window["building_polygons"] or source["omissions"]:
            raise ValueError("Dormer scene lost original sources")
        write(
            output,
            "ParisDormer" + suffix,
            mesh,
            {
                "camera": camera(view),
                "source": source,
                "sources": receipts,
                "window": window,
                "facade_materials": owner.facades.bindings,
                "procedures": owner.report(),
                "facades": owner.facades.report(),
                "ground_span_m": 10000.0,
                "scope": "Entire pinned988-source window, same eight podium/wing assets and prior facade/roof choices, attached procedural dormers on flat ground; no native/device/cache admission.",
            },
            proof,
        )
        print(
            json.dumps(
                {
                    "pass": suffix,
                    "generated": source["generated"],
                    "triangles": len(mesh["faces"]),
                }
            ),
            flush=True,
        )


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
