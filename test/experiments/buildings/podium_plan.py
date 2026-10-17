"""Source-contained podiums with one repeated upper profile and real terrace portals."""

from numbers import Integral

import numpy as np
import shapely
from building_layout import AGENT_RADIUS_M, WALL_M, canonical_geometry, frame, rectangle
from building_setbacks import candidate
from building_terraces import apply as terraces
from building_vertical import level
from outshine_ref.buildings.modelling.scope import scalar
from stepped_form_profiles import outlines
from stepped_source import base_plan, terrace_portals

XY_FIELDS = ("footprint", "rooms", "walls", "windows", "walkable")
DOOR_APPROACH_M = 0.45


class PodiumUnavailable(ValueError):
    def __init__(self, attempts):
        self.attempts = tuple(attempts)
        last = next(
            (r["reason"] for r in reversed(attempts) if "reason" in r),
            "no eligible base/upper split",
        )
        super().__init__(
            "No podium programme retains rooms, core and accessible terraces: " + last
        )


def reserved(plan, base_storeys):
    clearance = AGENT_RADIUS_M + WALL_M
    pads = [
        rectangle(
            door["centre"],
            np.asarray(door["tangent"]),
            2 * clearance,
            DOOR_APPROACH_M + clearance,
            DOOR_APPROACH_M + clearance,
        )
        for door in level(plan, base_storeys)["doors"]
    ]
    core = shapely.union_all([plan["core_envelope"], plan["corridor_zone"]]).buffer(
        WALL_M, join_style="mitre"
    )
    return shapely.union_all([core, *pads])


def proposal(plan, kind, width_m, axis=0):
    width_m = scalar(width_m)
    if width_m <= 0:
        raise ValueError("Podium width must be positive metres")
    if kind == "podium":
        return plan["footprint"].buffer(-width_m, join_style="mitre")
    if kind != "wing" or axis not in (0, 1):
        raise ValueError("Podium programme needs podium or wing on axis0/1")
    transform, _ = frame(plan["footprint"])
    direction = np.asarray(transform[:2] if axis == 0 else transform[2:4])
    a = outlines(plan, direction, -1, (width_m,))[0]
    b = outlines(plan, direction, 1, (width_m,))[0]
    return a.intersection(b)


def constant_upper(plan, proposed, base_storeys, *, reuse=True):
    count = plan["storeys"]
    if (
        plan.get("vertical_status") != "connected-storeys"
        or plan.get("circulation_status") != "common-access"
        or "storey_plans" in plan
        or isinstance(base_storeys, bool)
        or not isinstance(base_storeys, Integral)
        or not 1 <= base_storeys <= count - 2
    ):
        raise ValueError("Podium needs a connected base and at least two upper storeys")
    if (
        not isinstance(proposed, shapely.Geometry)
        or proposed.geom_type not in ("Polygon", "MultiPolygon")
        or shapely.get_coordinate_dimension(proposed) != 2
        or not np.isfinite(shapely.get_coordinates(proposed)).all()
        or not proposed.is_valid
        or proposed.is_empty
    ):
        raise ValueError("Podium needs a finite valid nonempty upper proposal")
    reserve = reserved(plan, base_storeys)
    upper = canonical_geometry(proposed.union(reserve).intersection(plan["footprint"]))
    upper = shapely.set_precision(upper, 0.0).intersection(
        shapely.set_precision(plan["footprint"], 0.0)
    )
    if upper.geom_type != "Polygon" or upper.is_empty or not upper.is_valid:
        raise ValueError("Podium upper profile is empty or disconnected")
    template, reason = candidate(level(plan, base_storeys), upper)
    if template is None:
        raise ValueError("Podium upper profile: " + reason)
    terrace = canonical_geometry(plan["footprint"].difference(upper))
    floors = []
    for index in range(count):
        source = level(plan, index)
        if index < base_storeys:
            current = source
        else:
            actual = template
            if not reuse and index != base_storeys:
                actual, reason = candidate(source, upper)
                if actual is None:
                    raise ValueError("Independent upper-storey rebuild: " + reason)
            current = source | {key: actual[key] for key in XY_FIELDS}
        floors.append(
            current
            | {"terrace": terrace if index == base_storeys else shapely.Polygon()}
        )
    return terraces(
        plan
        | {
            "storey_plans": tuple(floors),
            "setback_decisions": [
                {
                    "storey": i,
                    "reason": None,
                    "footprint_area_m2": f["footprint"].area,
                    "terrace_area_m2": f["terrace"].area,
                }
                for i, f in enumerate(floors)
            ],
        }
    )


def choose(plan, kind, *, validate=None, reuse=True):
    if kind not in ("podium", "wing"):
        raise ValueError("Podium programme needs podium or wing")
    attempts, accepted = [], []
    for base in (1, 2):
        if plan["storeys"] - base < 2:
            continue
        for width in (1.8, 2.8, 4.0):
            for axis in range(1 if kind == "podium" else 2):
                request = {
                    "kind": kind,
                    "base_storeys": base,
                    "width_m": width,
                    "axis": axis,
                }
                try:
                    current = constant_upper(
                        plan, proposal(plan, kind, width, axis), base, reuse=reuse
                    )
                    positive = [
                        r for r in current["terrace_access"] if r["area_m2"] > 1e-7
                    ]
                    if not positive or any(
                        r["reason"] for r in current["terrace_access"]
                    ):
                        raise ValueError(
                            "Every podium terrace needs a connected room portal"
                        )
                    area = sum(r["area_m2"] for r in positive)
                    if area < plan["footprint"].area * 0.04:
                        raise ValueError(
                            "Podium terrace is too small to establish a distinct mass"
                        )
                    attempts.append(
                        request | {"status": "pass", "terrace_area_m2": area}
                    )
                    accepted.append((area, base, current, request))
                except ValueError as error:
                    attempts.append(
                        request | {"status": "rejected", "reason": str(error)}
                    )
    for area, _base, current, request in sorted(accepted, key=lambda r: (-r[0], -r[1])):
        current["form_recipe"] = request | {
            "terrace_area_m2": area,
            "attempts": attempts,
            "origin": "procedural source-contained podium and repeated upper storeys",
        }
        try:
            current = terrace_portals(current)
            if validate is not None:
                validate(current)
            return current
        except (ValueError, AssertionError) as error:
            attempts.append(
                request | {"status": "rejected-product", "reason": str(error)}
            )
    raise PodiumUnavailable(attempts)


def programme(row, kind, *, validate=None, reuse=True):
    if kind not in ("podium", "wing"):
        raise ValueError("Podium programme needs podium or wing")
    try:
        with np.errstate(divide="raise", invalid="raise", over="raise"):
            return choose(base_plan(row), kind, validate=validate, reuse=reuse)
    except (FloatingPointError, RuntimeWarning) as error:
        raise ValueError(
            "Podium source planning rejected numeric geometry: " + str(error)
        ) from error
