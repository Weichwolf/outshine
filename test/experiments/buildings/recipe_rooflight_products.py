"""State/view-dependent hall details from retained v2 products, without rebuilding CSG."""

from facade_parts import append
from industrial_hall_details import products
from outshine_ref.buildings.rooflight_panels import closed
from recipe_rooflight_operators import GLAZING, PANELS, SHELL


def assemble(run, view, *, opened=0.0, staff_angle=0.0):
    programme = run.outputs["programme"]
    body, glazing = run.outputs["shell"], run.outputs["rooflights"]
    if body.type_id != SHELL or glazing.type_id != GLAZING:
        raise ValueError("Glazed assembly needs explicit v2 shell and glazing products")
    plan = programme.value.plan
    plan = dict(plan, entry=dict(plan["entry"], angle_deg=staff_angle))
    chunks, detailed = products(plan, 1, opened, view)
    if "panels" in run.outputs:
        if run.outputs["panels"].type_id != PANELS:
            raise ValueError("Unexpected rooflight panel product version")
        panes = run.outputs["panels"].value
    else:
        panes = closed(glazing.value)
    parts = (*chunks, *panes)
    return (
        append(body.value.body, parts),
        parts,
        {
            "recipe": "buildings.industrial-hall/2",
            "room_id": plan["room_id"],
            "fields": len(glazing.value),
            "detailed_leaf": detailed,
            "scope": "Full retained glazed shell and directly emitted details; no whole-building far LOD or native admission",
        },
    )
