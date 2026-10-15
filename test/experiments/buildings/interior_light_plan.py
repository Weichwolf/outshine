"""Derive camera-independent ceiling fixtures from existing room and storey identities."""

import hashlib

import numpy as np


def lights(plan, state="all-lit", *, circulation_portals=False):
    if state not in ("all-lit", "mixed", "off"):
        raise ValueError("Unknown diagnostic room-light state")
    result = []
    for floor, identifiers in enumerate(plan["storey_room_ids"]):
        current = plan["storey_plans"][floor] if "storey_plans" in plan else plan
        ceiling = float(current["ceiling_z_m"])
        if "storey_plans" not in plan:
            ceiling += floor * (plan["body_top_m"] - plan["base_z_m"]) / plan["storeys"]
        for index, (room, identifier) in enumerate(
            zip(current["rooms"], identifiers, strict=True)
        ):
            centre = room.representative_point()
            clearance = room.boundary.distance(centre)
            size = min(0.8, float(clearance * np.sqrt(2.0) * 0.8))
            enabled = (
                state == "all-lit"
                or state == "mixed"
                and int(hashlib.sha256(identifier.encode()).hexdigest()[:8], 16) % 3
                != 0
            )
            reserved = current["room_kinds"][index] == "service-reservation"
            result.append(
                {
                    "room_id": identifier,
                    "storey": floor,
                    "position": [centre.x, centre.y, ceiling - 0.04],
                    "size": [size, size],
                    "enabled": enabled and size > 0.02 and not reserved,
                    "radiance": [18.0, 12.0, 6.0],
                    "omitted_reason": "service reservation remains unlit"
                    if reserved
                    else "insufficient fixture clearance"
                    if size <= 0.02
                    else "",
                    "origin": "procedural ceiling fixture inside cached room; shared across cameras",
                }
            )
            if circulation_portals and current["room_kinds"][index] == "corridor":
                from corridor_lights import additional

                result.extend(additional(current, index, result[-1]))
    return result
