"""Fit inward support-room hinges to the actual room and existing activity layout."""

import numpy as np
import shapely
from building_vertical import level
from outshine_ref.buildings.interiors import access
from outshine_ref.buildings.interiors.doors import DoorLeaf, pose, sweep
from room_programme import portals


def portal_hinges(current, record, method="segmented"):
    result = []
    index = record["room_index"]
    room = current["rooms"][index]
    door = next(d for d in current["doors"] if d["id"] == record["door_id"])
    centre, tangent = np.asarray(door["centre"]), np.asarray(door["tangent"])
    normal = np.array([-tangent[1], tangent[0]])
    inward = next(
        (
            normal * sign
            for sign in (-1, 1)
            if room.covers(shapely.Point(centre + normal * sign * 0.55))
        ),
        None,
    )
    if inward is None:
        raise ValueError("Support door has no actual room-side anchor")
    for side in (-1, 1):
        leaf = DoorLeaf(
            record["door_id"] + "/leaf",
            tuple(centre),
            tuple(tangent),
            tuple(inward),
            current["floor_z_m"],
            width_m=float(np.ptp(shapely.get_coordinates(door["polygon"]) @ tangent)),
            height_m=door["height_m"],
            wall_face_m=shapely.Point(centre).distance(room),
            hinge_side=side,
        )
        motion, proof = sweep(leaf, method=method)
        outside = motion.difference(room).area
        result.append(
            {
                "room_id": record["id"],
                "storey": record["storey"],
                "kind": record["kind"],
                "door_id": door["id"],
                "leaf": leaf,
                "motion": motion,
                "open": pose(leaf, 90.0),
                "closed": pose(leaf, 0.0),
                "proof": proof,
                "outside_room_m2": outside,
            }
        )
    return result


def hinges(plan, method="segmented"):
    return [
        candidate
        for record in plan["support_refinement"]["spaces"]
        for candidate in portal_hinges(level(plan, record["storey"]), record, method)
    ]


def occupancy(candidate, fixed):
    motion = candidate["motion"]
    furniture = sum(motion.intersection(item["footprint"]).area for item in fixed)
    activity = sum(motion.intersection(item["activity"]).area for item in fixed)
    return candidate | {
        "furniture_overlap_m2": furniture,
        "activity_overlap_m2": activity,
        "fits_existing": max(candidate["outside_room_m2"], furniture, activity) <= 1e-8,
    }


def choices(plan, product, method="segmented"):
    rooms = {r["room_id"]: r for r in product["rooms"]}
    return [
        occupancy(candidate, rooms[candidate["room_id"]]["result"]["items"])
        for candidate in hinges(plan, method)
    ]


def select(plan, product):
    groups = {}
    for candidate in choices(plan, product):
        groups.setdefault(candidate["room_id"], []).append(candidate)
    rooms = {row["room_id"]: row for row in product["rooms"]}
    selected, unresolved = [], []
    for room_id, candidates in groups.items():
        valid = [row for row in candidates if row["fits_existing"]]
        attempts = []
        for candidate in valid:
            current = level(plan, candidate["storey"])
            room = rooms[room_id]
            shape = current["rooms"][room["room_index"]]
            anchors, reserved = portals(current, room["room_index"])
            try:
                access(
                    shape.difference(candidate["open"]),
                    anchors,
                    room["result"]["items"],
                    reserved,
                )
            except ValueError as error:
                attempts.append(
                    {"hinge_side": candidate["leaf"].hinge_side, "reason": str(error)}
                )
                continue
            selected.append(candidate)
            break
        else:
            unresolved.append(
                {
                    "room_id": room_id,
                    "reason": (
                        "No candidate passes bounded open-door access"
                        if valid
                        else "Neither inward hinge clears existing room, furniture and activity areas"
                    ),
                    "attempts": attempts,
                }
            )
    return {"selected": tuple(selected), "unresolved": unresolved}
