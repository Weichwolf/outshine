"""Coupled source-height vertical parapets and bodies before physical expansion."""

import math
from dataclasses import dataclass

import shapely
from outshine_ref.buildings.modelling import PrismVolume
from provider_contract import attributes
from roof_crown_plan import service_rectangle
from terraced_mass_frame import local_plan
from terraced_mass_plan import floors
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_frame import resolved

ROOF_MATERIAL = 60003


@dataclass(frozen=True)
class RoofPiece:
    role: str
    volume: object


@dataclass(frozen=True)
class VerticalCrown:
    footprint_wkb: bytes
    domain_wkb: bytes
    deck_wkb: bytes
    coping_underside_wkb: bytes
    top_m: float
    deck_m: float
    guard_m: float
    kind: str
    parts: tuple


@dataclass(frozen=True)
class RoofedPlan:
    body: object
    crown: object
    source_top_m: float
    attempts: int
    reason: str


def roof(body, top, kind, guard):
    local = local_plan(body)
    original = shapely.from_wkb(local.storeys[-1].footprint_wkb)
    polygon = resolved(original)
    inner = polygon.buffer(-0.20, join_style="mitre")
    inside_coping = polygon.buffer(-0.24, join_style="mitre")
    host = polygon.buffer(-0.28, join_style="mitre")
    if inner.is_empty or inside_coping.is_empty or host.area < 10:
        raise ValueError("Highest roof lacks a contained usable deck")
    service = service_rectangle(host) if kind == "parapet-service" else None
    if service is None:
        kind = "parapet"
    edge_top = top if service is None else body.top_m + guard
    values = [
        ("slab", polygon, body.top_m - 0.08, body.top_m, ROOF_MATERIAL),
        ("wall", resolved(polygon.difference(inner)), body.top_m, edge_top - 0.045, 12),
        (
            "coping",
            resolved(polygon.difference(inside_coping)),
            edge_top - 0.045,
            edge_top,
            10,
        ),
    ]
    if service is not None:
        values.extend(
            [
                ("service", service, body.top_m, top - 0.08, 12),
                ("service-cap", service, top - 0.08, top, ROOF_MATERIAL),
            ]
        )
    deck = inner if service is None else inner.difference(service)
    return VerticalCrown(
        original.wkb,
        polygon.wkb,
        deck.wkb,
        inner.difference(inside_coping).wkb,
        top,
        body.top_m,
        edge_top - body.top_m,
        kind,
        tuple(
            RoofPiece(
                role,
                PrismVolume(
                    body.identifier + "/highest-roof/" + role, p, lo, hi, material
                ),
            )
            for role, p, lo, hi, material in values
        ),
    )


def plan(row, kind):
    if any(
        k.startswith("roof") and v is not None for k, v in row["properties"].items()
    ):
        raise ValueError("Supplied roof fields retain their existing roof programme")
    parameters = attributes(row["properties"])
    count, _supplied, _recipe = floors(row, parameters)
    top = parameters["buildingHeight"]
    budget = max(0.0, math.floor((top - count * 2.64 - 1e-8) * 1000) / 1000)
    if budget < 0.06:
        return RoofedPlan(
            mass_plan(row, kind), None, top, 1, "no six-centimetre roof height reserve"
        )
    guard = min(1.1, budget)
    programme = "parapet-service" if budget >= 1.6 else "parapet"
    reserve = min(3.0, budget) if programme == "parapet-service" else guard
    body = mass_plan(row, kind, roof_height_m=reserve)
    crown = roof(body, top, programme, guard)
    attempts = 1
    if programme == "parapet-service" and crown.kind != programme:
        body = mass_plan(row, kind, roof_height_m=guard)
        crown = roof(body, top, "parapet", guard)
        attempts += 1
    if abs(crown.deck_m - body.top_m) > 1e-9:
        raise ValueError("Roof deck and highest body ceiling disagree")
    return RoofedPlan(
        body,
        crown,
        top,
        attempts,
        "procedural contained vertical parapet; highest roof has no access programme",
    )
