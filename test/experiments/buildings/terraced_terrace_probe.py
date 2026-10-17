"""Matched actual terraced buildings before/after protected accessible setbacks."""

import argparse
from pathlib import Path

import numpy as np
import shapely
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_facade_geometry import build_facade
from terraced_facade_prepare import positions
from terraced_facade_probe import facade_draw, materials
from terraced_mass_plan import plan as mass_plan
from terraced_mass_sources import sources
from terraced_terrace_access import check
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the terrace experiment before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index in (0, 3):
        for kind in ("cascade", "paired-wings"):
            mass = mass_plan(sources()[index], kind)
            o = TerraceProducts(plan(mass))
            checks = check(o)
            after = o.draw(True)
            before = facade_draw(build_facade(mass), mass)
            view = overview(
                np.r_[before["vertices"], after["vertices"]], [1, -1.25, 0.85]
            )
            metadata = {
                "source_id": mass.identifier,
                "source_footprint_wkb": mass.source_wkb.hex(),
                "source": {"material_definitions": materials()},
                "facade_materials": {},
                "ground_span_m": 10000,
                "terraces": [
                    {
                        "id": t.identifier,
                        "area_m2": shapely.from_wkb(t.deck_wkb).area,
                        "reason": t.reason,
                    }
                    for t in o.plan.terraces
                ],
                "portal_ids": [p.identifier for p in o.plan.portals],
                "access_checks": checks,
                "scope": "Actual protected source setback decks, aligned open portals and continuous routes from retained stair fronts. Additions shared between near/direct exterior. No highest-roof access, complete-city/cache/native/night/temporal admission.",
            }
            for label, mesh in (("Before", before), ("After", after)):
                write(
                    output,
                    f"{index}-{kind}-{label}",
                    mesh,
                    metadata | {"camera": camera(view)},
                    proof,
                )
            if index == 0:
                p = o.plan.portals[0]
                centre = positions(
                    np.array([[*p.centre, p.bottom_m + 1.1]]), mass.core
                )[0]
                tangent = p.tangent[0] * np.asarray(mass.core.across_xy) + p.tangent[
                    1
                ] * np.asarray(mass.core.along_xy)
                normal = np.array([tangent[1], -tangent[0]])
                eye = centre + np.r_[normal * 4 - tangent * 1.2, 0.85]
                target = centre - np.r_[normal * 0.4, 0]
                close = View(eye, target, width=1920, height=1080, fov_deg=48)
                write(
                    output,
                    f"{index}-{kind}-Portal",
                    after,
                    metadata | {"camera": camera(close)},
                    proof,
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
