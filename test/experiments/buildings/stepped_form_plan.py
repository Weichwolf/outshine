"""Bounded whole-storey setbacks sharing the existing core and real terrace portals."""

import numpy as np
import shapely
from building_layout import frame
from building_setbacks import apply as setbacks
from building_terraces import apply as terraces


def outlines(plan, axis, side, distances):
    polygon = plan["footprint"]
    origin = np.asarray(polygon.centroid.coords[0])
    points = shapely.get_coordinates(polygon) - origin
    tangent = np.array([-axis[1], axis[0]])
    projection = points @ axis
    lo, hi = projection.min(), projection.max()
    cross = points @ tangent
    low, high = cross.min() - 1.0, cross.max() + 1.0
    result = []
    for distance in distances:
        if distance == 0:
            result.append(polygon)
            continue
        a, b = (lo + distance, hi + 1.0) if side < 0 else (lo - 1.0, hi - distance)
        vertices = [
            origin + axis * x + tangent * y
            for x, y in ((a, low), (b, low), (b, high), (a, high))
        ]
        result.append(polygon.intersection(shapely.Polygon(vertices)))
    return result


def choose(plan, widths=(1.2, 1.8, 2.8), validate=None):
    if plan["storeys"] < 3 or plan.get("circulation_status") != "common-access":
        raise ValueError(
            "Stepped form requires at least three connected common-access storeys"
        )
    transform, _ = frame(plan["footprint"])
    axes = (np.asarray(transform[:2]), np.asarray(transform[2:4]))
    attempts = []
    accepted = []
    for width, tiers in [(w, t) for w in widths for t in (1, 2)]:
        distances = [0.0] * (plan["storeys"] - tiers) + [
            width * (i + 1) for i in range(tiers)
        ]
        requests = [("all-sides", None, 0, None)]
        for index, axis in enumerate(axes):
            for side in (-1, 1):
                requests.append(
                    (
                        "axis-" + str(index),
                        axis,
                        side,
                        outlines(plan, axis, side, distances),
                    )
                )
        for method, axis, side, proposed in requests:
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
                    dict(
                        method=method,
                        side=side,
                        tiers=tiers,
                        width_m=width,
                        status="pass",
                        terrace_area_m2=area,
                    )
                )
            except ValueError as error:
                attempts.append(
                    dict(
                        method=method,
                        side=side,
                        tiers=tiers,
                        width_m=width,
                        status="rejected",
                        reason=str(error),
                    )
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
                dict(
                    method=method,
                    side=side,
                    width_m=width,
                    status="rejected-detail",
                    reason=str(error),
                )
            )
            continue
        return current | dict(
            form_recipe=dict(
                method=method,
                side=side,
                width_m=width,
                terrace_area_m2=area,
                attempts=attempts,
                origin="procedural upper-storey interpretation; supplied geometry/use unproven",
            )
        )
    raise ValueError(
        "No planned stepped alternative survives actual opening-detail clearance"
    )
