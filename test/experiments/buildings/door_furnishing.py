"""Choose inward support doors before bounded furniture placement and open-state access."""

from building_vertical import level
from door_support_plan import occupancy, portal_hinges
from outshine_ref.buildings.interiors import layout
from room_programme import portals, programme, requests


def fit(current, source, beam_width=1):
    room = current["rooms"][source["room_index"]]
    anchors, reserved = portals(current, source["room_index"])
    record = {
        "room_id": source["id"],
        "portal_anchors": anchors,
        "reserved": reserved,
        "required": requests(source["kind"], room.area),
        "placement_method": "wall-anchors",
    }
    accepted, attempts = [], []
    for candidate in portal_hinges(current, source):
        report = {
            "room_id": record["room_id"],
            "hinge_side": candidate["leaf"].hinge_side,
            "tested": 0,
        }
        try:
            if candidate["outside_room_m2"] > 1e-8:
                raise ValueError("Inward door motion escapes the supplied room")
            result = layout(
                room.difference(candidate["open"]),
                record["portal_anchors"],
                record["required"],
                record["room_id"] + "/furniture",
                record["reserved"],
                record["placement_method"],
                beam_width=beam_width,
                placement_reserved=(candidate["motion"],),
            )
            report["tested"] = result["tested"]
            if result["status"] != "complete":
                raise ValueError(result["reason"])
            candidate = occupancy(candidate, result["items"])
            if not candidate["fits_existing"]:
                raise ValueError("Placed activity intersects the reserved motion")
            cost = sum(path.length for path in result["proof"]["paths"])
            accepted.append((cost, candidate["leaf"].hinge_side, candidate, result))
            report.update(status="admitted", path_length_m=cost)
        except ValueError as error:
            report.update(status="rejected", reason=str(error))
        attempts.append(report)
    return (min(accepted, key=lambda row: row[:2]) if accepted else None), attempts


def joint(plan, beam_width=1):
    product = programme(plan, "jittered-anchors")
    sources = {record["id"]: record for record in plan["support_refinement"]["spaces"]}
    rooms, selected, unresolved, attempts = [], [], [], []
    for record in product["rooms"]:
        if record["room_id"] not in sources:
            rooms.append(record)
            continue
        chosen, trials = fit(
            level(plan, record["storey"]), sources[record["room_id"]], beam_width
        )
        attempts.extend(trials)
        if chosen:
            _, _, candidate, result = chosen
            rooms.append(
                record
                | {
                    "result": result,
                    "placement_reserved": (candidate["motion"],),
                    "door_plan_id": candidate["leaf"].id,
                }
            )
            selected.append(candidate)
        else:
            rooms.append(record)
            unresolved.append(
                {
                    "room_id": record["room_id"],
                    "reason": "Neither bounded inward hinge admits every required object and activity",
                }
            )
    complete = (
        bool(rooms)
        and not product["unresolved"]
        and not unresolved
        and all(record["result"]["status"] == "complete" for record in rooms)
    )
    return product | {
        "rooms": tuple(rooms),
        "status": "complete-control" if complete else "incomplete",
        "unresolved": [*product["unresolved"], *unresolved],
        "door_placement_beam_width": beam_width,
    }, {"selected": tuple(selected), "unresolved": unresolved, "attempts": attempts}
