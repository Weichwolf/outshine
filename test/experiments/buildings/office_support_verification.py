"""Independent lineage, source, aperture and whole-floor admission of support subdivisions."""

import itertools

import numpy as np
import shapely
from building_layout import AGENT_RADIUS_M
from building_plan import wall_at
from building_vertical import level
from room_programme import office_scope


def unchanged_opening(before, after):
    for key, value in before.items():
        if key == "rooms":
            continue
        other = after[key]
        if isinstance(value, shapely.Geometry):
            if value.wkb != other.wkb:
                raise ValueError(
                    "Support subdivision changed an existing opening polygon"
                )
        elif value != other:
            raise ValueError(
                "Support subdivision changed an existing opening attribute"
            )


def preserved_openings(before, after):
    by_door = {d["id"]: d for d in after["doors"]}
    for door in before["doors"]:
        if door["id"] not in by_door:
            raise ValueError("Support subdivision removed an existing door")
        unchanged_opening(door, by_door[door["id"]])
        if door["rooms"] != by_door[door["id"]]["rooms"]:
            raise ValueError("Support subdivision redirected an existing door")
    by_window = {w["id"]: w for w in after["windows"]}
    if set(by_window) != {w["id"] for w in before["windows"]}:
        raise ValueError("Support subdivision added or removed an exterior window")
    for window in before["windows"]:
        unchanged_opening(window, by_window[window["id"]])


def full_radius_paths(after):
    body_obstacles = shapely.union_all(
        [wall_at(after, after["floor_z_m"] + z) for z in (0.4, 1.2, 1.9)]
    )
    free = (
        after["footprint"]
        .union(after["entrance"]["landing"])
        .difference(body_obstacles)
        .buffer(-AGENT_RADIUS_M, join_style="mitre")
    )
    landing = (
        after["entrance"]["landing"].representative_point()
        if any(door["kind"] == "entrance" for door in after["doors"])
        else after["rooms"][0].representative_point()
    )
    connected = next((p for p in shapely.get_parts(free) if p.covers(landing)), None)
    if connected is None or any(
        room.buffer(-AGENT_RADIUS_M, join_style="mitre").difference(connected).area
        > 1e-6
        for room in after["rooms"]
    ):
        raise ValueError("Support walls block a full-radius path from the entrance")
    for door in after["doors"]:
        centre, tangent = np.asarray(door["centre"]), np.asarray(door["tangent"])
        normal = np.array([-tangent[1], tangent[0]])
        body = shapely.LineString(
            [centre - normal * 0.45, centre + normal * 0.45]
        ).buffer(AGENT_RADIUS_M, cap_style="flat")
        if body.intersection(body_obstacles).area > 1e-7:
            raise ValueError("Actual full-width support portal is obstructed")


def admission(original, plan):
    for key in (
        "id",
        "source_properties",
        "source_attributes",
        "source_top_m",
        "body_top_m",
        "base_z_m",
        "storeys",
    ):
        if original[key] != plan[key]:
            raise ValueError(
                "Support subdivision changed a supplied envelope or source attribute"
            )
    if original["footprint"].wkb != plan["footprint"].wkb:
        raise ValueError("Support subdivision changed the original footprint")
    if original.get("stair") is not plan.get("stair"):
        raise ValueError(
            "Support subdivision replaced the immutable existing stair plan"
        )
    floors, _ = office_scope(original)
    spaces = plan["support_refinement"]["spaces"]
    seen = set()
    results = []
    for floor in range(plan["storeys"]):
        before, after = level(original, floor), level(plan, floor)
        old_ids, ids = (
            original["storey_room_ids"][floor],
            plan["storey_room_ids"][floor],
        )
        if (
            ids[: len(old_ids)] != old_ids
            or len(ids) != len(after["rooms"])
            or len(set(ids)) != len(ids)
        ):
            raise ValueError(
                "Support rooms lost or duplicated an original room identity"
            )
        rows = [r for r in spaces if r["storey"] == floor]
        if len(rows) != len(after["rooms"]) - len(before["rooms"]):
            raise ValueError("Support subdivision has unaccounted added rooms")
        if floor not in floors and (
            rows
            or any(
                a.wkb != b.wkb
                for a, b in zip(before["rooms"], after["rooms"], strict=True)
            )
        ):
            raise ValueError("Support subdivision changes an unsupported office floor")
        owners = {r["parent_room_id"] for r in rows}
        for index, room in enumerate(before["rooms"]):
            new = after["rooms"][index]
            if old_ids[index] not in owners and new.wkb != room.wkb:
                raise ValueError(
                    "Support subdivision changed a non-parent room or reservation"
                )
            if new.difference(room).area > 1e-6:
                raise ValueError(
                    "Support subdivision enlarges a parent outside its original room"
                )
        for row in rows:
            index = row["room_index"]
            kind = row["kind"]
            if (
                row["id"] in seen
                or row["id"] != ids[index]
                or row["parent_room_id"] not in old_ids
            ):
                raise ValueError("Support subdivision has invalid room ancestry")
            seen.add(row["id"])
            room = after["rooms"][index]
            limit = 7.0 if kind == "sanitary" else 9.0
            if not 4.5 <= room.area <= limit:
                raise ValueError(
                    "Support room is outside its stated compact area interval"
                )
            parent = before["rooms"][old_ids.index(row["parent_room_id"])]
            if room.difference(parent).area > 1e-6:
                raise ValueError("Support child leaves its actual original parent room")
            doors = [d for d in after["doors"] if index in d["rooms"]]
            if (
                len(doors) != 1
                or set(doors[0]["rooms"]) != {0, index}
                or doors[0]["id"] != row["door_id"]
            ):
                raise ValueError(
                    "Support room requires one independent common-corridor doorway"
                )
            if kind == "sanitary" and any(
                index in w["rooms"] for w in after["windows"]
            ):
                raise ValueError(
                    "Support sanitary has an unmodified clear exterior window"
                )
        for a, b in itertools.combinations(after["rooms"], 2):
            if a.intersection(b).area > 1e-7:
                raise ValueError("Support room floors overlap")
        preserved_openings(before, after)
        union = shapely.union_all(after["rooms"])
        expected_walls = after["footprint"].difference(union)
        if expected_walls.symmetric_difference(after["walls"]).area > 1e-6:
            raise ValueError("Support walls and floor partition disagree")
        full_radius_paths(after)
        results.append(
            {
                "storey": floor,
                "rooms": len(ids),
                "support_rooms": len(rows),
                "doors": len(after["doors"]),
                "additional_wall_area_m2": after["walls"].area - before["walls"].area,
            }
        )
    return {
        "status": "pass",
        "floors": results,
        "source_footprint_unchanged": True,
        "source_height_unchanged": True,
        "original_access_ids_retained": True,
        "scope": "2D full-radius walls, openings and lineage; furniture, active door privacy and native continuous movement remain separate",
    }
