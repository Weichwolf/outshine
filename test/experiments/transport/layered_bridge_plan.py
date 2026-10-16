"""A failed optional profile leaves its old source decisions available to the caller."""

import time

import numpy as np
import shapely
from cycle_bridge_profile import solve
from layered_bridge_audit import audit
from layered_bridge_geometry import build
from layered_bridge_problem import domain_of, prepare
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance


def lower_bound(fixture, lower):
    problem, heights, _ribbon, _note = lower
    domain = domain_of([r for r in fixture["sources"] if r["profile"] == "motorway"])
    overlap = domain.intersection(problem["domain"])
    if overlap.area <= 1e-8:
        return []
    stations = (shapely.get_coordinates(overlap) - problem["origin"]) @ problem["axis"]
    probes = np.r_[
        stations,
        problem["stations"][
            (problem["stations"] > stations.min())
            & (problem["stations"] < stations.max())
        ],
    ]
    maximum = float(np.interp(probes, problem["stations"], heights).max())
    # The existing path recipe includes horse access. Retain the shared 4.5 m envelope.
    value = maximum + 0.5 + 4.5 + 0.02
    return [
        {
            "source_id": "profile:path:traffic-envelope",
            "kind": "minimum",
            "value_m": value,
            "range_m": [float(stations.min()), float(stations.max())],
        }
    ]


def candidate(fixture, profile, *, lower=None):
    began = time.perf_counter()
    note = {"profile": profile, "valid": False, "reason": "", "methods": {}}
    try:
        p = prepare(
            fixture, profile, lower_bound(fixture, lower) if lower is not None else ()
        )
        heights = None
        for method in ("chain", "lp"):
            result, proof = solve(
                *(p[k] for k in ("stations", "lower", "upper", "rate")), method=method
            )
            note["methods"][method] = proof
            if result is None:
                raise ValueError("profile_" + proof["reason"])
            if heights is not None and not np.allclose(
                result, heights, rtol=0, atol=1e-8
            ):
                raise ValueError("independent fixed-chain reference differs")
            if heights is None:
                heights = result
        field = Mesh(
            np.array(fixture["field_vertices"]), np.array(fixture["field_faces"])
        )
        ribbon, geometry = build(p, heights, field)
        physical = audit(ribbon, np.array(geometry["table"]), p, fixture)
        if lower is not None:
            separation = vertical_clearance(
                lower[2].upper, ribbon.lower, seconds=4, max_faces=8192
            )
            if (
                not separation["valid"]
                or separation["minimum_clearance_m"] < 4.5 - 1e-7
            ):
                raise ValueError(
                    "chosen lower traffic envelope is not clear of the upper structure"
                )
            physical["lower_traffic_clearance"] = separation
            cast_lower = Mesh(
                lower[2].upper.vertices.astype(np.float32).astype(float),
                lower[2].upper.triangles,
            )
            cast_upper = Mesh(
                ribbon.lower.vertices.astype(np.float32).astype(float),
                ribbon.lower.triangles,
            )
            cast_clearance = vertical_clearance(
                cast_lower, cast_upper, seconds=4, max_faces=8192
            )
            if (
                not cast_clearance["valid"]
                or cast_clearance["minimum_clearance_m"] < 4.5 - 1e-7
            ):
                raise ValueError("actual Float32 lower traffic envelope is not clear")
            physical["float32_lower_traffic_clearance"] = cast_clearance
        note.update(
            valid=True,
            geometry=geometry,
            physical=physical,
            derivative_patches=len(p["patches"]),
            stations=len(p["stations"]),
            seconds=time.perf_counter() - began,
        )
        return (p, heights, ribbon, note), note
    except ValueError as error:
        note.update(
            reason=str(error),
            seconds=time.perf_counter() - began,
            retained_previous_source_decisions=True,
        )
        return None, note
