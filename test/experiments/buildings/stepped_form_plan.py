"""Bounded whole-storey setbacks sharing the existing core and real terrace portals."""

import numpy as np
from building_layout import frame
from building_setbacks import apply as setbacks
from building_terraces import apply as terraces
from stepped_form_profiles import outlines, requests

__all__ = ["choose", "outlines"]


def choose(plan, widths=(1.2, 1.8, 2.8), validate=None, *, profile="legacy"):
    if profile not in ("legacy", "corner", "diagonal"):
        raise ValueError("Unknown stepped form profile")
    if plan["storeys"] < 3 or plan.get("circulation_status") != "common-access":
        raise ValueError(
            "Stepped form requires at least three connected common-access storeys"
        )
    transform, _ = frame(plan["footprint"])
    axes = (np.asarray(transform[:2]), np.asarray(transform[2:4]))
    attempts = []
    accepted = []
    if profile == "diagonal":
        widths = tuple(2 * w for w in widths)
    for width, tiers in [(w, t) for w in widths for t in (1, 2)]:
        distances = [0.0] * (plan["storeys"] - tiers) + [
            width * (i + 1) for i in range(tiers)
        ]
        for method, axis, side, proposed in requests(plan, axes, distances, profile):
            try:
                stepped = terraces(setbacks(plan, distances, outlines=proposed))
                positive = [r for r in stepped["terrace_access"] if r["area_m2"] > 1e-7]
                if not positive or any(r["reason"] for r in positive):
                    raise ValueError(
                        "Every positive terrace component needs a real connected room portal"
                    )
                if any(r["reason"] for r in stepped["setback_decisions"][1:]):
                    raise ValueError(
                        "Requested upper envelopes failed existing room/clearance checks"
                    )
                area = sum(r["area_m2"] for r in positive)
                if area < plan["footprint"].area * 0.04:
                    raise ValueError(
                        "Accepted setback has no substantial visible terrace area"
                    )
                accepted.append((area, stepped, method, side, width))
                attempts.append(
                    {
                        "method": method,
                        "side": side,
                        "tiers": tiers,
                        "width_m": width,
                        "status": "pass",
                        "terrace_area_m2": area,
                    }
                )
            except ValueError as error:
                attempts.append(
                    {
                        "method": method,
                        "side": side,
                        "tiers": tiers,
                        "width_m": width,
                        "status": "rejected",
                        "reason": str(error),
                    }
                )
    if not accepted:
        raise ValueError(
            "No bounded stepped alternative preserves every room and accessible terrace"
        )
    for area, current, method, side, width in sorted(
        accepted, key=lambda item: -item[0]
    ):
        try:
            if validate is not None:
                validate(current)
        except (ValueError, AssertionError) as error:
            attempts.append(
                {
                    "method": method,
                    "side": side,
                    "width_m": width,
                    "status": "rejected-product",
                    "reason": str(error),
                }
            )
            continue
        return current | {
            "form_recipe": {
                "method": method,
                "side": side,
                "width_m": width,
                "terrace_area_m2": area,
                "attempts": attempts,
                "origin": "procedural upper-storey interpretation; supplied geometry/use unproven",
            }
        }
    raise ValueError(
        "No planned stepped alternative passes actual opening details and complete far geometry"
    )
