"""Whole-width ports, explicit floor openings and source/plate headroom checks."""

from dataclasses import replace
from itertools import pairwise
from math import isfinite

import shapely
from indoor_floor_geometry import mesh_floor
from indoor_stair_geometry import stair_mesh
from indoor_stair_recipe import StairLimits, schedule
from outshine_ref.contracts import HeightInterval, Reservation, SourceFeature


def open_floor_plates(floors, surfaces, headroom_m, clearance_m, requests):
    changed, meshes, openings = [], {}, []
    corridors = {r.identifier: r.corridor for r in requests}
    for floor in floors["floors"]:
        pieces = [
            s["polygon"]
            for s in surfaces
            if s["z_m"] < floor.upper_z_m and s["z_m"] + headroom_m > floor.lower_z_m
        ]
        cut = (
            shapely.union_all(pieces)
            .buffer(clearance_m, join_style="mitre")
            .intersection(floor.polygon)
        )
        if cut.area == 0:
            changed.append(floor)
            continue
        for choice in floors["choices"]:
            if (
                choice.get("floor_id") == floor.identifier
                and corridors[choice["source_id"]].intersection(cut).area > 0
            ):
                raise ValueError("stairwell_conflicts_with_chosen_walkway")
        adjusted = replace(floor, polygon=floor.polygon.difference(cut))
        mesh, audit = mesh_floor(adjusted)
        changed.append(adjusted)
        meshes[floor.identifier] = mesh
        openings.append(
            {
                "floor_id": floor.identifier,
                "wkb_hex": cut.wkb_hex,
                "area_m2": float(cut.area),
                "mesh_audit": audit,
            }
        )
    for surface in surfaces:
        for floor in changed:
            if (
                floor.lower_z_m < surface["z_m"] + headroom_m
                and floor.upper_z_m > surface["z_m"]
                and floor.polygon.intersection(surface["polygon"]).area > 0
            ):
                raise ValueError("stair_headroom_blocked_by_emitted_floor")
    return tuple(changed), meshes, openings


def source_clearance(parts, base, surfaces, headroom_m, limits, method="bands"):
    # Envelopes describe possible interior, not filled solid obstacles. Internal
    # part boundaries cancel in the 3D union; emitted floor slabs remain obstacles.
    if (
        not isfinite(headroom_m)
        or headroom_m <= 0
        or len(parts) > limits.max_source_parts
    ):
        raise ValueError("Bounded source context and positive headroom required")
    if method not in ("bands", "repeated"):
        raise ValueError("Known source-prism coverage procedure required")
    planes = sorted({base + h for p in parts for h in (p.lower_m, p.upper_m)})
    bands, union_inputs, unions = [], 0, 0
    tests = 0
    for surface in surfaces:
        if not bands or method == "repeated":
            bands = []
            for low, high in pairwise(planes):
                active = [
                    p.polygon
                    for p in parts
                    if base + p.lower_m <= low and base + p.upper_m >= high
                ]
                bands.append((low, high, shapely.union_all(active)))
                union_inputs += len(active)
                unions += 1
        bottom = surface.get("body_bottom_z_m", surface["z_m"] - limits.waist_m)
        ceiling = surface["z_m"] + headroom_m + limits.waist_m
        if not planes or bottom < planes[0] or ceiling > planes[-1]:
            raise ValueError("stair_body_headroom_outside_source_envelopes")
        for low, high, domain in bands:
            if low < ceiling and high > bottom:
                tests += 1
                if not domain.covers(surface["polygon"]):
                    raise ValueError("stair_body_headroom_outside_source_envelopes")
    return {
        "source_height_bands": len(bands),
        "source_union_input_parts": union_inputs,
        "source_unions_built": unions,
        "whole_prism_band_coverage_tests": tests,
        "scope": "Exact vertical-stratum union of supplied extruded envelopes with a roof reserve equal to waist thickness; not surveyed structure, statics, rooms or World obstacles.",
    }


def build_stair(
    binding,
    floors,
    requests,
    *,
    limits=None,
    headroom_m=2.1,
    method="enumerated",
    source_method="bands",
):
    limits = StairLimits() if limits is None else limits
    if binding["reason"]:
        return {"status": "pending", "reason": binding["reason"]}
    request = binding["request"]
    ends = binding["ends"]
    if ends[0]["z_m"] > ends[1]["z_m"]:
        ends = ends[::-1]
        axis = shapely.reverse(request.axis)
    else:
        axis = request.axis
    recipe, search = schedule(
        axis.length, ends[1]["z_m"] - ends[0]["z_m"], limits, method
    )
    if recipe is None:
        return {"status": "pending", "reason": search["reason"], "search": search}
    try:
        mesh, surfaces, audit = stair_mesh(
            axis,
            request.original["width_m"],
            ends[0]["z_m"],
            ends[1]["z_m"],
            recipe,
            limits,
        )
        selected = {f.identifier: f for f in floors["floors"]}
        low, high = (selected[e["floor_id"]] for e in ends)
        if low.frame != request.frame or high.frame != request.frame:
            raise ValueError("stair_floor_frame_mismatch")
        if low.source_ids != high.source_ids:
            raise ValueError("stair_source_components_differ")
        for end, floor, surface in zip(
            ends, (low, high), (surfaces[0], surfaces[-1]), strict=True
        ):
            if end["z_m"] != surface["z_m"] or not floor.polygon.covers(
                surface["polygon"]
            ):
                raise ValueError("whole_stair_landing_not_on_chosen_floor")
        plates, plate_meshes, openings = open_floor_plates(
            floors, surfaces, headroom_m, limits.clearance_m, requests
        )
        parts = tuple(
            p for p in floors["adjusted_volumes"] if p.identifier in low.source_ids
        )
        programme = next(
            p for p in floors["programmes"] if tuple(p["source_ids"]) == low.source_ids
        )
        source_audit = source_clearance(
            parts,
            programme["base_z_m"],
            surfaces,
            headroom_m,
            limits,
            source_method,
        )
    except ValueError as error:
        return {
            "status": "pending",
            "reason": str(error),
            "search": search,
            "recipe": recipe,
        }
    return {
        "status": "ready",
        "reason": "",
        "recipe": recipe,
        "search": search,
        "mesh": mesh,
        "surfaces": surfaces,
        "mesh_audit": audit,
        "source_audit": source_audit,
        "floors": plates,
        "modified_floor_meshes": plate_meshes,
        "openings": openings,
        "end_profiles": ends,
        "frame": request.frame,
        "source_features": (
            SourceFeature(
                request.identifier, request.axis, request.original.get("properties", {})
            ),
        ),
        "reservations": tuple(
            Reservation(
                request.identifier,
                s["polygon"],
                HeightInterval(s["z_m"], s["z_m"] + headroom_m),
                "traffic_clearance",
            )
            for s in surfaces
        ),
        "qualification": "Chosen source-snapshot stair, whole landings and emitted plate openings; no rooms/doors/OSM-node/nativeWorld/statics or public-route certificate.",
    }
