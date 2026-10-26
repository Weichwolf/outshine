"""Source-bound leaf selection, actual solid rays and door-aware local room routes."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
from building_vertical import level
from door_mesh import expand, template
from door_support_plan import select
from evidence import revision
from office_support_plan import refine
from outshine_ref.buildings.interiors import access
from room_programme import portals, programme
from room_programme_plan import CONTROLS, source_plan
from structural_shell_geometry import native


def verify(plan, product):
    selection = select(plan, product)
    rooms = {row["room_id"]: row for row in product["rooms"]}
    verified = []
    for selected in selection["selected"]:
        room_id = selected["room_id"]
        current = level(plan, selected["storey"])
        room = rooms[room_id]
        shape = current["rooms"][room["room_index"]]
        anchors, reserved = portals(current, room["room_index"])
        result = access(
            shape.difference(selected["open"]),
            anchors,
            room["result"]["items"],
            reserved,
        )
        leaf = selected["leaf"]
        fixed = template(leaf)
        closed, opened = (native(expand(leaf, fixed, angle)) for angle in (0, 90))
        ray_count = 0
        for across in np.linspace(
            -leaf.width_m / 2 + 0.01, leaf.width_m / 2 - 0.01, 13
        ):
            xy = np.asarray(leaf.centre) + np.asarray(leaf.tangent) * across
            start = xy - np.asarray(leaf.inward) * 0.5
            finish = xy + np.asarray(leaf.inward) * 0.5
            for height in (0.04, 1.04, leaf.height_m - 0.04):
                a, b = (
                    [*start, leaf.floor_z_m + height],
                    [*finish, leaf.floor_z_m + height],
                )
                if not closed.ray_cast(a, b) or opened.ray_cast(a, b):
                    raise AssertionError(
                        "Actual source door state does not match portal ray expectation"
                    )
                ray_count += 1
        verified.append(
            {
                "room_id": room_id,
                "leaf": asdict(leaf),
                "triangles_per_leaf": 12,
                "paired_state_rays": ray_count,
                "route_count": len(result["paths"]),
                "motion_proof": selected["proof"],
            }
        )
    return {
        "verified": verified,
        "unresolved": selection["unresolved"],
        "scope": "Door solids, straight portal rays and 2D routes in affected support rooms; full building assembly, oblique privacy, frames and full actor route sampling remain unverified",
    }


def run(output):
    records = []
    for site, identifier in CONTROLS:
        _row, original, receipt = source_plan(site, identifier)
        plan = refine(original)
        product = programme(plan, "jittered-anchors")
        records.append(
            {
                "site": site,
                "id": identifier,
                "receipt": receipt,
                **verify(plan, product),
            }
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(revision() | {"cases": records}, indent=2) + "\n")
    print([(r["site"], len(r["verified"]), len(r["unresolved"])) for r in records])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
    )
    run(parser.parse_args().output)
