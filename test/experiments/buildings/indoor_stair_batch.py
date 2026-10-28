"""Bounded composition of compatible source groups, bodies and shared openings."""

import shapely
from indoor_stair_recipe import StairLimits
from indoor_stair_site import (
    apply_floor_openings,
    build_stair,
    check_stair_ports,
    compile_floor_openings,
    floor_opening_domains,
    prepare_stair,
)
from outshine_ref.contracts import SourceFeature


def assemble(stairs, floors, requests, headroom_m, work):
    work["assembly_attempts"] += 1
    surfaces = tuple(s for stair in stairs for s in stair["surfaces"])
    cuts = {}
    for stair in stairs:
        for opening in stair["openings"]:
            cuts.setdefault(opening["floor_id"], []).append(
                shapely.from_wkb(bytes.fromhex(opening["wkb_hex"]))
            )
    cuts = {key: shapely.union_all(parts) for key, parts in cuts.items()}
    plates, openings = apply_floor_openings(
        floors, cuts, surfaces, headroom_m, requests
    )
    work["joint_port_tests"] += check_stair_ports(stairs, plates)
    meshes, openings = compile_floor_openings(plates, openings)
    work["joint_plate_meshes_qualified"] += len(meshes)
    return {"floors": plates, "modified_floor_meshes": meshes, "openings": openings}


def source_rows(group, status, reason, owner=None):
    return [
        {
            "source_id": b["source_id"],
            "original": b["original"],
            "frame": b["request"].frame.identifier,
            "axis_wkb_hex": b["request"].axis.wkb_hex,
            "status": status
            if b["source_id"] == group["primary"]["source_id"] or status != "ready"
            else "shared",
            "reason": reason,
            "generated_owner_id": owner,
        }
        for b in group["members"]
    ]


def compose_stairs(
    groups,
    floors,
    requests,
    *,
    limits=None,
    method="shared",
    headroom_m=2.1,
    max_bodies=16,
    max_groups=64,
):
    limits = StairLimits() if limits is None else limits
    if method not in ("eager", "shared"):
        raise ValueError("Known stair composition procedure required")
    if any(type(v) is not int or not 1 <= v <= 256 for v in (max_bodies, max_groups)):
        raise ValueError("Explicit bounded batch capacities required")
    groups = sorted(groups, key=lambda g: g["primary"]["source_id"])
    accepted, ledger = [], []
    work = {
        "construction_calls": 0,
        "footprint_pair_tests": 0,
        "individual_plate_meshes_qualified": 0,
        "individual_opening_domain_passes": 0,
        "joint_plate_meshes_qualified": 0,
        "assembly_attempts": 0,
        "joint_port_tests": 0,
        "fallback_candidates": 0,
    }
    constructor = build_stair if method == "eager" else prepare_stair
    by_owner = {}
    for group in groups:
        binding = group["primary"]
        if binding["reason"]:
            ledger.extend(source_rows(group, "pending", binding["reason"]))
            continue
        if work["construction_calls"] >= max_groups or len(accepted) >= max_bodies:
            ledger.extend(
                source_rows(group, "pending", "stair_batch_construction_budget")
            )
            continue
        work["construction_calls"] += 1
        if method == "eager":
            result = constructor(
                binding, floors, requests, limits=limits, headroom_m=headroom_m
            )
        else:
            result = constructor(binding, floors, limits=limits, headroom_m=headroom_m)
        if result["status"] not in ("candidate", "ready"):
            ledger.extend(source_rows(group, "pending", result["reason"]))
            continue
        if method == "shared":
            try:
                plates, openings = floor_opening_domains(
                    floors, result["surfaces"], headroom_m, limits.clearance_m, requests
                )
                check_stair_ports((result,), plates)
            except ValueError as error:
                ledger.extend(source_rows(group, "pending", str(error)))
                continue
            result = result | {"openings": openings}
        work["individual_opening_domain_passes"] += 1
        work["individual_plate_meshes_qualified"] += len(
            result.get("modified_floor_meshes", {})
        )
        footprint = shapely.convex_hull(
            shapely.multipoints(result["mesh"]["vertices"][:, :2])
        )
        conflict = None
        for prior in accepted:
            work["footprint_pair_tests"] += 1
            if footprint.intersection(prior["footprint"]).area > 0:
                conflict = prior["source_id"]
                break
        if conflict is not None:
            ledger.extend(
                source_rows(group, "pending", "joint_stair_prism_overlap_unplanned")
            )
            continue
        owner = binding["source_id"]
        result = {
            k: v
            for k, v in result.items()
            if k not in ("floors", "modified_floor_meshes")
        }
        accepted.append(
            result
            | {
                "source_id": owner,
                "footprint": footprint,
                "source_ids": tuple(b["source_id"] for b in group["members"]),
            }
        )
        by_owner[owner] = group
    try:
        output = assemble(accepted, floors, requests, headroom_m, work)
    except ValueError:
        subset = []
        output = assemble((), floors, requests, headroom_m, work)
        for stair in accepted:
            work["fallback_candidates"] += 1
            try:
                proposed = assemble(
                    (*subset, stair), floors, requests, headroom_m, work
                )
            except ValueError as error:
                ledger.extend(
                    source_rows(by_owner[stair["source_id"]], "pending", str(error))
                )
            else:
                subset.append(stair)
                output = proposed
        accepted = subset
    for stair in accepted:
        owner = stair["source_id"]
        ledger.extend(source_rows(by_owner[owner], "ready", "", owner))
    # Expose only the final jointly qualified product, never the candidates'
    # individual floor products or candidate/independent-ready statuses.
    products = tuple(
        {
            k: v
            for k, v in stair.items()
            if k
            not in (
                "floors",
                "modified_floor_meshes",
                "openings",
                "status",
                "qualification",
            )
        }
        | {
            "status": "ready",
            "final_port_tests": 2,
            "qualification": "jointly_qualified_snapshot_stair",
        }
        for stair in accepted
    )
    features = tuple(
        SourceFeature(
            b["source_id"], b["request"].axis, b["original"].get("properties", {})
        )
        for stair in accepted
        for b in by_owner[stair["source_id"]]["members"]
    )
    return output | {
        "stairs": products,
        "source_ledger": sorted(ledger, key=lambda r: r["source_id"]),
        "source_features": features,
        "work": work,
        "reservations": tuple(r for stair in accepted for r in stair["reservations"]),
        "frame": floors["frame"] if "frame" in floors else floors["floors"][0].frame,
        "qualification": "Fixed-snapshot jointly qualified stair bodies, final whole-width ports, shared plates and conservative footprint separation. Overlapping projections stay unplanned, not proven 3D collisions. No original OSM topology/nativeWorld/public-route/statics or device certificate.",
    }
