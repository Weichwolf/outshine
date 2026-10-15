"""Independent programme admission and measured mesh bounds, separate from placement search."""

from collections import Counter

import numpy as np
import shapely
from building_vertical import level
from room_programme import office_scope, requests


def admission(plan, product):
    floors, evidence = office_scope(plan)
    if (
        tuple(product["floors"]) != floors
        or product["evidence"]["origin"] != evidence["origin"]
    ):
        raise ValueError("Programme claims unsupported source use or storeys")
    seen = set()
    counts = Counter()
    complete = bool(product["rooms"]) and not product["unresolved"]
    for room in product["rooms"]:
        floor, index = room["storey"], room["room_index"]
        if floor not in floors:
            raise ValueError("Programme room is outside its actual source use scope")
        current = level(plan, floor)
        if current["room_kinds"][index] != "generic-room":
            raise ValueError("Programme occupies a circulation or service reservation")
        expected_id = plan["storey_room_ids"][floor][index]
        if room["room_id"] != expected_id or expected_id in seen:
            raise ValueError(
                "Programme duplicates or replaces an existing room identity"
            )
        seen.add(expected_id)
        expected = requests(room["kind"], current["rooms"][index].area)
        if tuple(room["required"]) != expected:
            raise ValueError(
                "Programme silently weakens a required furnishing function"
            )
        specs = tuple(item["spec"] for item in room["result"]["items"])
        if specs != expected:
            complete = False
        if room["result"]["status"] != "complete":
            complete = False
        if room["kind"] in ("work", "work-tea", "meeting") and not any(
            index in window["rooms"] for window in current["windows"]
        ):
            raise ValueError(
                "Work or meeting programme lacks an actual daylight opening"
            )
        for number, item in enumerate(room["result"]["items"]):
            if item["id"] != expected_id + f"/furniture/item-{number}":
                raise ValueError(
                    "Furnishing lost its existing room-bound stable identity"
                )
            counts[item["kind"]] += 1
    for floor in floors:
        rooms = [r for r in product["rooms"] if r["storey"] == floor]
        expected_rooms = {
            i
            for i, kind in enumerate(level(plan, floor)["room_kinds"])
            if kind == "generic-room"
        }
        kinds = {r["kind"] for r in rooms}
        if {r["room_index"] for r in rooms} != expected_rooms or not (
            {"meeting", "sanitary"} <= kinds
            and kinds & {"work", "work-tea"}
            and kinds & {"tea", "work-tea"}
        ):
            complete = False
    if (product["status"] == "complete-control") != complete:
        raise ValueError(
            "Programme completeness claim disagrees with actual required functions"
        )
    return {"status": product["status"], "functions": dict(counts), "rooms": len(seen)}


def instance_bounds(plan, product, mesh, description):
    by_id = {row["id"]: row for row in description["instances"]}
    if len(by_id) != len(description["instances"]):
        raise ValueError("Detailed furnishing mesh duplicates instance identities")
    for room in product["rooms"]:
        current = level(plan, room["storey"])
        shape = current["rooms"][room["room_index"]]
        for item in room["result"]["items"]:
            index = by_id.pop(item["id"])
            faces = mesh["faces"][
                index["first_triangle"] : index["first_triangle"] + index["triangles"]
            ]
            xyz = mesh["vertices"][faces].reshape(-1, 3)
            if not np.all(
                shapely.covers(shape.buffer(1e-8), shapely.points(xyz[:, :2]))
            ):
                raise ValueError("Actual furnishing vertices escape their room")
            tangent = item["tangent"]
            front = np.array([-tangent[1], tangent[0]])
            relative = xyz[:, :2] - item["centre"]
            local = np.c_[
                relative @ tangent, relative @ front, xyz[:, 2] - room["floor_z_m"]
            ]
            spec = item["spec"]
            np.testing.assert_allclose(
                np.r_[local.min(axis=0), local.max(axis=0)],
                [
                    -spec.width_m / 2,
                    0,
                    0,
                    spec.width_m / 2,
                    spec.depth_m,
                    spec.height_m,
                ],
                atol=1e-9,
                rtol=0,
            )
            np.testing.assert_allclose(
                [xyz[:, 2].min(), xyz[:, 2].max()],
                [room["floor_z_m"], room["floor_z_m"] + item["spec"].height_m],
                atol=1e-9,
                rtol=0,
            )
            if xyz[:, 2].max() > current["ceiling_z_m"]:
                raise ValueError("Furnishing penetrates actual ceiling")
    if by_id:
        raise ValueError("Detailed furnishing mesh contains undeclared instances")
