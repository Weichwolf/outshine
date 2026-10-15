"""Corrupt the actual source-bound programme and compiled furniture, not just labels."""

import manifold3d as manifold
import numpy as np
from boolean_building_bands import construct
from furniture_collision import furnishing_solid
from furniture_mesh import build, template
from programme_verification import admission, instance_bounds
from room_programme import STORAGE
from structural_shell_geometry import assembly
from verify_structural_shell import access as building_access


def rejected(name, action):
    try:
        action()
    except (ValueError, AssertionError) as error:
        return {"case": name, "status": "rejected", "reason": str(error)}
    raise AssertionError("Invalid furnishing control passed: " + name)


def controls(plan, product):
    if product["status"] != "complete-control":
        raise ValueError(
            "Programme corruption requires a complete actual positive control"
        )
    mesh, description = build(product)
    lifted = dict(mesh, vertices=mesh["vertices"] + [0.0, 0.0, 0.12])
    unsupported = dict(product, floors=(*product["floors"], plan["storeys"]))
    missing = dict(
        product, rooms=tuple(r for r in product["rooms"] if r["kind"] != "sanitary")
    )
    room = next(r for r in product["rooms"] if r["kind"] == "sanitary")
    weakened_room = dict(room, required=room["required"][:1])
    weakened = dict(
        product,
        rooms=tuple(weakened_room if r is room else r for r in product["rooms"]),
    )
    checks = [
        rejected(
            "actual-height-disagrees-with-declared-template",
            lambda: instance_bounds(plan, product, lifted, description),
        ),
        rejected("missing-sanitary-programme", lambda: admission(plan, missing)),
        rejected("weakened-required-function", lambda: admission(plan, weakened)),
        rejected("unsupported-source-storey", lambda: admission(plan, unsupported)),
    ]
    obstacle = template(STORAGE)
    door = plan["entrance"]
    tangent = np.asarray(door["tangent"])
    front = np.array([-tangent[1], tangent[0]])
    matrix = np.array(
        [[tangent[0], front[0], 0.0], [tangent[1], front[1], 0.0], [0.0, 0.0, 1.0]]
    )
    obstacle["vertices"] = obstacle["vertices"] @ matrix.T + [
        *door["centre"],
        plan["floor_z_m"],
    ]
    _, shell = assembly(plan, construct(plan), frames=True)
    blocked = shell + furnishing_solid(obstacle)
    if blocked.status() != manifold.Error.NoError:
        raise ValueError(
            "Blocked doorway control did not produce a valid compiled solid"
        )
    checks.append(
        rejected("actual-shelf-blocks-entrance", lambda: building_access(plan, blocked))
    )
    return checks
