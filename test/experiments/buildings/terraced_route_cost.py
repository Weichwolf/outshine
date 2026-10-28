"""Equal-geometry shared/per-terrace route mesh construction and finite work counts."""

from dataclasses import replace
from unittest.mock import patch

import shapely
from terraced_crown_geometry import RoofedProducts
from terraced_terrace_plan import portal_for


def build(plan, mode, *, whole=False):
    if mode not in ("grid", "shared", "fresh"):
        raise ValueError("Unknown measured terrace construction")

    def selected(*args, **kwargs):
        if mode == "fresh":
            kwargs["route_mesh"] = None
        value = portal_for(*args, **kwargs)
        return replace(value, mesh=None if mode == "fresh" else value.mesh)

    with patch("terraced_terrace_plan.portal_for", new=selected):
        owner = RoofedProducts(
            plan, terrace_routing="grid" if mode == "grid" else "triangulated"
        )
        if whole:
            owner.draw(True)
    return owner


def work(owner):
    rows = [dict(row[2]) for row in owner.body.plan.route_work]
    counters = {
        key: sum(row[key] for row in rows)
        for key in (
            "mesh_builds",
            "route_queries",
            "expanded_triangles",
            "containment_queries",
            "mesh_reused",
        )
    }
    counters["constructed_triangles"] = sum(
        row["triangles"] for row in rows if row["mesh_builds"]
    )
    counters["constructed_shared_edges"] = sum(
        row["shared_edges"] for row in rows if row["mesh_builds"]
    )
    return {
        "portals": len(owner.body.plan.portals),
        "terraces": len(owner.body.plan.terraces),
        "unserved": [t.reason for t in owner.body.plan.terraces if t.portal is None],
        "route_points": sum(
            len(shapely.from_wkb(p.core_route_wkb).coords)
            for p in owner.body.plan.portals
        ),
        "counters": counters,
    }
