"""Refine only the explicitly supported office floors; preserve source and access ancestry."""

import time

import shapely
from building_layout import AGENT_RADIUS_M, polygons
from building_plan import walkable
from building_vertical import level
from office_support_geometry import candidates
from room_programme import office_scope


def apply_candidate(current, identifiers, candidate):
    index = len(current["rooms"])
    parent = identifiers[candidate["host"]]
    child = parent + "/support-" + candidate["kind"]
    door = candidate["door"] | {"id": child + "/door", "kind": "corridor-door"}
    result = current | {
        "rooms": candidate["rooms"],
        "room_kinds": [*current["room_kinds"], "generic-room"],
        "doors": [*current["doors"], door],
        "windows": candidate["windows"],
        "walls": current["footprint"].difference(shapely.union_all(candidate["rooms"])),
        "programme_kinds": current.get("programme_kinds", {})
        | {index: candidate["kind"]},
    }
    free = walkable(result)
    reached = next(
        (
            p
            for p in polygons(free)
            if p.covers(result["entrance"]["landing"].representative_point())
        ),
        None,
    )
    if reached is None or any(
        room.buffer(-AGENT_RADIUS_M, join_style="mitre").difference(reached).area > 1e-6
        for room in result["rooms"]
    ):
        raise ValueError(
            "Actual partition/door wall bands disconnect a previous or support space"
        )
    result["walkable"] = free
    return (
        result,
        [*identifiers, child],
        {
            "id": child,
            "parent_room_id": parent,
            "kind": candidate["kind"],
            "room_index": index,
            "door_id": door["id"],
            "area_m2": result["rooms"][index].area,
            "parent_original_area_m2": current["rooms"][candidate["host"]].area,
            "parent_remaining_area_m2": result["rooms"][candidate["host"]].area,
            "new_wall_area_m2": result["walls"].area - current["walls"].area,
        },
    )


def support_step(states, kind, method, floor, rejected, door_aware=False):
    proposals = []
    for state in states:
        current = state["plan"]
        for index, room_kind in enumerate(current["room_kinds"]):
            if room_kind != "generic-room" or index in current.get(
                "programme_kinds", {}
            ):
                continue
            admitted, failures = candidates(current, index, kind, method)
            proposals.extend(
                (state["score"] + row["score"], state, row) for row in admitted
            )
            rejected.extend({"storey": floor, **row} for row in failures)
    following = []
    fingerprints = set()
    door_trials = 0
    for score, state, candidate in sorted(
        proposals,
        key=lambda r: (r[0], r[2]["host"], r[2]["candidate"], tuple(r[1]["ids"])),
    ):
        if door_aware and door_trials >= 24:
            break
        try:
            current, ids, record = apply_candidate(
                state["plan"], state["ids"], candidate
            )
            key = tuple(room.wkb for room in current["rooms"])
            if key in fingerprints:
                continue
            fingerprints.add(key)
            if door_aware:
                from door_furnishing import fit

                door_trials += 1
                chosen, attempts = fit(current, record | {"storey": floor})
                if chosen is None:
                    rejected.append(
                        {
                            "storey": floor,
                            "host": candidate["host"],
                            "kind": kind,
                            "reason": "Partition has no bounded door/furniture/activities layout",
                            "door_attempts": attempts,
                        }
                    )
                    continue
            following.append(
                {
                    "plan": current,
                    "ids": ids,
                    "records": [*state["records"], record],
                    "score": score,
                }
            )
            if len(following) == 4:
                break
        except ValueError as error:
            rejected.append(
                {
                    "storey": floor,
                    "host": candidate["host"],
                    "kind": kind,
                    "reason": str(error),
                }
            )
    return following or states


def refine(plan, method="wall-ends", *, door_aware=False):
    if method not in ("wall-ends", "sliding-wall"):
        raise ValueError("Unknown compact support partition method")
    if "support_refinement" in plan:
        raise ValueError(
            "Refine the original parent plan once; support IDs already exist"
        )
    began = time.perf_counter()
    floors, evidence = office_scope(plan)
    storeys = [level(plan, i).copy() for i in range(plan["storeys"])]
    identifiers = [list(ids) for ids in plan["storey_room_ids"]]
    records, rejected, unresolved = [], [], []
    for floor in floors:
        current = storeys[floor]
        if (
            current.get("circulation_status") != "common-access"
            or current["room_kinds"][0] != "corridor"
        ):
            unresolved.append(
                {
                    "storey": floor,
                    "reason": "Support rooms require an existing shared corridor",
                }
            )
            continue
        states = [
            {"plan": current, "ids": identifiers[floor], "records": [], "score": 0.0}
        ]
        for kind in ("sanitary", "tea"):
            states = support_step(states, kind, method, floor, rejected, door_aware)
        chosen = min(
            states, key=lambda s: (-len(s["records"]), s["score"], tuple(s["ids"]))
        )
        storeys[floor], identifiers[floor] = chosen["plan"], chosen["ids"]
        records.extend({"storey": floor, **row} for row in chosen["records"])
        missing = {"sanitary", "tea"}.difference(r["kind"] for r in chosen["records"])
        unresolved.extend(
            {
                "storey": floor,
                "kind": kind,
                "reason": "No bounded compact support partition preserves existing windows and access",
            }
            for kind in sorted(missing)
        )
    result = plan | {
        "storey_plans": storeys,
        "storey_room_ids": identifiers,
        "support_refinement": {
            "method": method,
            "evidence": evidence,
            "status": "complete-2d-control"
            if floors and not unresolved
            else "incomplete",
            "spaces": records,
            "rejected": rejected,
            "unresolved": unresolved,
            "maximum_floor_states": 4,
            "maximum_candidates_per_room": 48,
            "door_aware": door_aware,
            "maximum_door_layout_trials_per_step": 24 if door_aware else 0,
            "elapsed_ms": (time.perf_counter() - began) * 1000,
            "scope": "Procedural compact side rooms; original rooms remain identifiable parents. No furnishing, door privacy, regulatory or native admission claim.",
        },
    }
    result["storey_portals"] = [
        [
            {
                "id": f"{plan['id']}:floor-{i}:door-{n}",
                "rooms": [identifiers[i][r] for r in door["rooms"]],
                "outside": door["kind"] == "entrance",
            }
            for n, door in enumerate(current["doors"])
        ]
        for i, current in enumerate(storeys)
    ]
    return result
