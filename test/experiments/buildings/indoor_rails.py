"""Whole local railing candidates; base stairs/plates and source ledger stay owned."""

from indoor_mesh_checks import closed_mesh
from indoor_rail_guards import opening_guards
from indoor_rail_instances import reference_support, support_templates
from indoor_rail_recipe import RailLimits, handrails
from indoor_rail_supports import SupportLimits, handrail_supports


def compose_rails(
    batch, floor_reservations, *, method="shared", limits=None, support_limits=None
):
    if method not in ("shared", "eager"):
        raise ValueError("Known support representation required")
    if any(s["frame"] != batch["frame"] for s in batch["stairs"]) or any(
        f.frame != batch["frame"] for f in batch["floors"]
    ):
        raise ValueError("rail_common_frame_mismatch")
    limits = RailLimits() if limits is None else limits
    support_limits = SupportLimits() if support_limits is None else support_limits
    rails = handrails(batch, floor_reservations, limits=limits)
    plans, support_tests = handrail_supports(
        rails["handrails"], batch, floor_reservations, limits, limits=support_limits
    )
    guards = opening_guards(
        batch, floor_reservations, limits, support_limits=support_limits
    )
    failures = {
        r["source_id"]: r["reason"] for r in rails["ledger"] if r["status"] == "pending"
    }
    for rail, plan in zip(rails["handrails"], plans, strict=True):
        if plan["status"] != "candidate":
            failures[rail["source_id"]] = plan["reason"]
    for guard in guards["ledger"]:
        if guard["status"] == "pending":
            for owner in guard["source_ids"]:
                failures.setdefault(owner, guard["reason"])
    for _ in batch["stairs"]:
        previous = len(failures)
        for guard in guards["ledger"]:
            if any(s in failures for s in guard["source_ids"]):
                for owner in guard["source_ids"]:
                    failures.setdefault(owner, "shared_opening_guard_incomplete")
        if len(failures) == previous:
            break
    selected, supports = [], []
    for rail, plan in zip(rails["handrails"], plans, strict=True):
        if rail["source_id"] not in failures:
            selected.append(rail)
            supports.extend(plan["posts"])
            supports.extend(plan["brackets"])
    selected_guards = [
        g for g in guards["guards"] if not any(s in failures for s in g["source_ids"])
    ]
    for guard in selected_guards:
        supports.extend(guard["posts"])
    templates = support_templates(limits, support_limits) if method == "shared" else {}
    for support in supports:
        if method == "eager":
            support["mesh"] = reference_support(support, limits, support_limits)
            closed_mesh(support["mesh"])
    ledger = [
        {
            "source_id": s["source_id"],
            "source_ids": s["source_ids"],
            "status": "pending" if s["source_id"] in failures else "candidate",
            "reason": failures.get(s["source_id"], ""),
        }
        for s in batch["stairs"]
    ]
    meshes = [r["mesh"] for r in (*selected, *selected_guards)]
    meshes.extend(s["mesh"] for s in supports if "mesh" in s)
    meshes.extend(templates.values())
    return {
        "handrails": tuple(selected),
        "guards": tuple(selected_guards),
        "supports": tuple(supports),
        "templates": templates,
        "ledger": ledger,
        "opening_ledger": guards["ledger"],
        "frame": batch["frame"],
        "limits": limits,
        "support_limits": support_limits,
        "work": {
            "rail_meshes_built": len(rails["handrails"]) + len(guards["guards"]),
            "support_meshes_built": len(supports)
            if method == "eager"
            else len(templates),
            "support_instances": len(supports),
            "support_vertices_written": sum(
                len(m["vertices"])
                for m in (
                    list(templates.values())
                    if method == "shared"
                    else [s["mesh"] for s in supports]
                )
            ),
            "traffic_candidates": rails["traffic_tests"]
            + support_tests
            + guards["traffic_tests"],
            "mesh_payload_bytes": sum(a.nbytes for m in meshes for a in m.values()),
            "transform_payload_bytes": sum(s["transform"].nbytes for s in supports),
        },
        "scope": "Local railing candidates in the caller's common source frame; independent actual-output qualification required. No native World, statics or safety-standard admission.",
    }
