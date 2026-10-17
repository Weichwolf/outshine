"""Both whole-city passes share the existing prepared podium/wing assets."""

import argparse
import json
from dataclasses import replace
from pathlib import Path

from city_scene import inputs
from clad_facade_mesh import CladFacades
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from showcase_tiles import view_for
from tile_buildings import construct
from tile_podiums import PodiumDetails
from tile_roof_crowns import CrownFacades


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze crown whole-city producer before image evidence")
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
    for after in (False, True):
        owner.facades = (
            CrownFacades(owner.rows, view) if after else CladFacades(owner.rows, view)
        )
        owner.completed.clear()
        owner.rejected.clear()
        mesh, source = construct(
            "Paris",
            prepared=(owner.rows, {"invalid": [], "span_m": window["width_m"]}),
            ordinary_builder=owner,
        )
        if source["generated"] != window["building_polygons"] or source["omissions"]:
            raise ValueError("Crown city lost original sources")
        write(
            output,
            "ParisCrown" + ("After" if after else "Before"),
            mesh,
            {
                "camera": camera(view),
                "source": source,
                "sources": receipts,
                "window": window,
                "facade_materials": owner.facades.bindings,
                "procedures": owner.report(),
                "roof_crowns": owner.facades.base.completed if after else [],
                "roof_crown_rejected": owner.facades.base.rejected if after else [],
                "scope": "Entire pinned988-source window, shared eight prepared podiums, all prior cladding and flat ground; inferred roof programme. No native/cache/device admission.",
            },
            proof,
        )
        print(
            json.dumps(
                {
                    "pass": "after" if after else "before",
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
    if output == Path("/tmp").resolve() or not output.is_relative_to(
        Path("/tmp").resolve()
    ):
        parser.error("--output requires a subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
