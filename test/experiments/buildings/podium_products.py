"""Validated source programmes with complete near structure retained for later demand."""

from dataclasses import dataclass

import numpy as np
from opening_details import slots
from podium_plan import programme
from recipe_hall_operators import sealed
from stepped_envelope_geometry import geometry
from stepped_facade_details import present
from stepped_openings import apply as openings
from stepped_source import exterior, near
from verify_vertical import connected_spaces

GLASS_SLOT = 32768


@dataclass(frozen=True, slots=True)
class PodiumAsset:
    plan: object
    physical: tuple
    near_draw: object
    compact_draw: object
    compact_openings: tuple


def bound(product, wall_material, glass_material):
    output = dict(product)
    output["materials"] = product["materials"].copy()
    output["materials"][product["materials"] == 1] = wall_material
    output["materials"][product["materials"] == GLASS_SLOT] = glass_material
    return output


def freeze_asset(plan, physical, compact, records):
    physical = sealed(tuple(physical))
    near_draw = sealed(present(physical[0], 1, GLASS_SLOT))
    for mesh in (near_draw, compact):
        if not np.isfinite(mesh["vertices"]).all():
            raise ValueError("Podium prepared product is nonfinite")
    return PodiumAsset(
        sealed(plan), physical, near_draw, sealed(compact), sealed(tuple(records))
    )


def prepare(row, kind):
    prepared = []

    def admitted(plan):
        if len(slots(plan)) > 512:
            raise ValueError("Podium opening capacity exceeded before mesh work")
        mesh, _envelope, complete, _method = exterior(plan, 1)
        geometry(plan, complete)
        physical = near(plan, 1)
        compact, records = openings(plan, mesh, GLASS_SLOT)
        plan["form_recipe"]["admission"] = (
            "complete compact/near source products and sampled access"
        )
        prepared[:] = [freeze_asset(plan, physical, compact, records)]

    plan = programme(row, kind, validate=admitted)
    connected_spaces(plan)
    if len(prepared) != 1:
        raise ValueError("Podium programme has no validated physical product")
    return prepared[0]
