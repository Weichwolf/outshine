"""Independent saved support contacts, air spans and original source decisions."""

import json

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, replay_contact
from foot_ramp_audit import shell_ownership
from foot_ramp_geometry import Ramp
from layered_bridge_supports import air_intervals, axes, excluded
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


def pier_proof(fixture, value, chains, bodies, contacts, fine, lower=None):
    p, _h, ribbon, note = value
    ground, deck = ContactSurface(fine), ContactSurface(ribbon.lower)
    forbidden = excluded(fixture, p["profile"], lower)
    shapes, errors = [], []
    for body, (lo, hi) in zip(bodies, contacts, strict=True):
        shape = surface_domain(lo)
        shell_ownership(Ramp(body, lo, hi, shape))
        volume = audit_closed(body)
        xyz = hi.vertices[hi.triangles]
        area = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])[:, 2] / 2
        heights = hi.vertices[:, 2] - lo.vertices[:, 2]
        expected = float(np.sum(area * heights[hi.triangles].mean(axis=1)))
        if heights.min() < 0.1 - 1e-7 or volume <= 0 or abs(volume - expected) > 1e-7:
            raise ValueError("saved layered pier height/closed volume invalid")
        if not ribbon.shape.buffer(1e-8).covers(shape) or shape.intersects(forbidden):
            raise ValueError("saved layered pier violates deck/retained traffic")
        errors.extend([replay_contact(lo, ground), replay_contact(hi, deck)])
        a = body.vertices[body.triangles]
        b = body.vertices.astype(np.float32).astype(float)[body.triangles]
        if np.any(
            np.einsum(
                "ij,ij->i",
                np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]),
                np.cross(b[:, 1] - b[:, 0], b[:, 2] - b[:, 0]),
            )
            <= 0
        ):
            raise ValueError("actual Float32 saved pier collapses/inverts")
        if any(shape.intersection(other).area > 1e-8 for other in shapes):
            raise ValueError("saved layered piers overlap")
        shapes.append(shape)
    spans, bound = [], 36.0 if p["profile"] == "motorway" else 12.0
    assigned = set()
    for row in axes(fixture, p, chains).values():
        line = shapely.LineString(row["xy"])
        width = 6 if p["profile"] == "motorway" else 0.75
        stations = []
        for i, shape in enumerate(shapes):
            if line.buffer(width + 1e-7, cap_style="flat").covers(shape):
                stations.append(line.project(shape.centroid))
                assigned.add(i)
        for a, b in air_intervals(line, p, np.array(note["geometry"]["table"])):
            split = sorted({a, b, *[s for s in stations if a < s < b]})
            spans.append(float(max(np.diff(split))))
    worst = max(spans, default=0)
    if not bodies or len(assigned) != len(bodies) or worst > bound + 1e-6:
        raise ValueError("saved physical piers lack bounded continuous air spans")
    return {
        "piers": len(bodies),
        "whole_contact_error_m": max(errors),
        "maximum_air_span_m": worst,
        "maximum_allowed_span_m": bound,
    }


def source_proof(args, fixture, admitted, reports):
    old = json.loads((args.baseline / "source-usage.json").read_text())
    rows = json.loads((args.output / "source-usage.json").read_text())
    groups = {r["source_id"]: r["profile"] for r in fixture["sources"]}
    expected = []
    for original in old:
        row = dict(original)
        name = groups.get(row["source_id"])
        if name in admitted:
            row.update(status="used", reason="", height_owner="layered_bridge/" + name)
        elif name is not None and row["status"] == "not_used":
            if not reports[name].get("reason"):
                raise ValueError("rejected selected source has no actual reason")
            row.update(
                previous_reason=row["reason"],
                reason="layered_profile: " + reports[name]["reason"],
            )
        expected.append(row)
    if rows != expected or len({r["source_id"] for r in rows}) != len(rows):
        raise ValueError(
            "saved ledger changed/lost original source or height ownership"
        )
    return {
        "sources": len(rows),
        "used_sources": sum(r["status"] == "used" for r in rows),
    }
