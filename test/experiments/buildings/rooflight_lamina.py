"""Direct two-sided rooflight sheets, selected before any panel mesh expansion."""

import numpy as np
from facade_parts import append
from industrial_hall_details import products
from outshine_ref.buildings.rooflight_panels import (
    DEPTH_M,
    MAX_FIELDS,
    corners,
    place,
)
from outshine_ref.buildings.rooflight_panels import sheet_template as template
from outshine_ref.buildings.rooflight_panels import (
    template as closed_template,
)
from recipe_rooflight_operators import GLAZING, SHELL

DISTANCE_BOUND_M = DEPTH_M / 2


def selection(fields, view):
    if len(fields) > MAX_FIELDS or len({item["id"] for item in fields}) != len(fields):
        raise ValueError("Rooflight selection needs bounded unique fields")
    if not fields:
        return ()
    upper = np.stack([corners(item) for item in fields])
    bounds = np.concatenate([upper, upper - [0, 0, DEPTH_M]], axis=1)
    coordinates = (bounds - view.eye) @ view.basis.T
    nearest = coordinates[:, :, 2].min(axis=1) - view.movement_m
    errors = np.full(len(fields), np.inf)
    valid = nearest > DISTANCE_BOUND_M
    numerator = DISTANCE_BOUND_M * (
        np.linalg.norm(coordinates, axis=2).max(axis=1) + view.movement_m
    )
    errors[valid] = (
        view.focal_px
        * numerator[valid]
        / (nearest[valid] * (nearest[valid] - DISTANCE_BOUND_M))
    )
    return tuple(
        {
            "id": item["id"],
            "lamina": bool(error <= view.detail_error_px),
            "geometry_bound_px": float(error) if np.isfinite(error) else None,
        }
        for item, error in zip(fields, errors, strict=True)
    )


def panels(fields, view):
    selected = selection(fields, view)
    result = []
    for item, chosen in zip(fields, selected, strict=True):
        a, low, b, high = item["bounds"]
        build = template if chosen["lamina"] else closed_template
        result.append(place(item, build(b - a, high - low, item["slope"])))
    return result, selected


def assemble(run, view, *, opened=0.0, staff_angle=0.0):
    body, fields = run.outputs["shell"], run.outputs["rooflights"]
    if body.type_id != SHELL or fields.type_id != GLAZING:
        raise ValueError("Lamina assembly needs retained v2 physical shell/fields")
    plan = run.outputs["programme"].value.plan
    plan = dict(plan, entry=dict(plan["entry"], angle_deg=staff_angle))
    panes, chosen = panels(fields.value, view)
    parts, detailed = products(plan, 1, opened, view)
    return append(body.value.body, [*parts, *panes]), chosen, detailed
