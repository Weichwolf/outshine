"""Compact opt-in hip request; expensive skeletons belong to near expansion."""

from dataclasses import dataclass, replace

import shapely
from outshine_ref.buildings.modelling import SkeletonLimits

LIMITS = SkeletonLimits(vertices=128, nodes=1024, python_events=4096, seconds=1.0)


@dataclass(frozen=True)
class HipCrown:
    footprint_wkb: bytes
    domain_wkb: bytes
    top_m: float
    deck_m: float
    method: str
    limits: object
    fallback: object
    guard_m: float = 0.0
    kind: str = "hip"


def with_hip(plan, *, method="auto", limits=LIMITS):
    if method not in ("auto", "contour") or not isinstance(limits, SkeletonLimits):
        raise ValueError("Hip crowns require auto/contour and explicit skeleton limits")
    if plan.crown is None or plan.access is not None:
        return plan
    if isinstance(plan.crown, HipCrown):
        return plan
    crown = plan.crown
    if (
        len(shapely.get_coordinates(shapely.from_wkb(crown.domain_wkb)))
        > limits.vertices
    ):
        return replace(
            plan, reason="hip unserved: source boundary exceeds vertex budget"
        )
    request = HipCrown(
        crown.footprint_wkb,
        crown.domain_wkb,
        crown.top_m,
        crown.deck_m,
        method,
        limits,
        crown,
    )
    return replace(
        plan,
        crown=request,
        reason="deferred source-height hip crown; original roof retained on bounded failure",
    )
