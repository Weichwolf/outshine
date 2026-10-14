"""One connected flat deck over a shared ground plan with explicit port limits."""

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from shared_bridge_geometry import layout
from surface_height_methods import checked_fit


def check_sources(sources):
    if not 2 <= len(sources) <= 64:
        raise ValueError("bounded nonempty deck and ground component required")
    identifiers = [s["source_id"] for s in sources]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError("one original per unique source identity required")
    decks, ground = [], []
    for source in sources:
        role = source["role"]
        props = source["properties"]
        if role == "deferred" and props.get("indoor") and source.get("omission_reason"):
            continue
        level = props.get("layer", 1 if role == "deck" else 0)
        if type(level) is not int or props.get("indoor"):
            raise ValueError("explicit compatible source levels required")
        if role == "deck" and props.get("brunnel") == "bridge":
            decks.append(source)
        elif role == "ground" and props.get("brunnel") not in ("bridge", "tunnel"):
            if props.get("subclass") == "steps":
                raise ValueError("stairs require a separate physical recipe")
            ground.append(source)
        else:
            raise ValueError("unsupported component source role")
    if not decks or not ground:
        raise ValueError("both source levels required")
    deck_levels = {s["properties"].get("layer", 1) for s in decks}
    ground_levels = {s["properties"].get("layer", 0) for s in ground}
    if (
        len(deck_levels) != 1
        or len(ground_levels) != 1
        or any(
            s["properties"].get("layer", 0) >= next(iter(deck_levels)) for s in ground
        )
    ):
        raise ValueError("one deck above every ground source required")
    return decks, ground


def port_report(sources, deck, ground):
    samplers = {"deck": SurfaceSampler(deck), "ground": SurfaceSampler(ground)}
    result = []
    for source in sources:
        if source["role"] == "deferred":
            continue
        xy = np.asarray(source["xy"])[[0, -1]]
        contacts = samplers[source["role"]].sample(xy)
        if not np.isfinite(contacts.points_enu_m).all():
            raise ValueError("original endpoint lacks a chosen-surface contact")
        for end in range(2):
            result.append(
                {
                    "source_id": source["source_id"],
                    "endpoint": end,
                    "point_enu_m": contacts.points_enu_m[end].tolist(),
                    "prior_height_m": source["end_heights_m"][end],
                    "height_change_m": float(
                        contacts.points_enu_m[end, 2] - source["end_heights_m"][end]
                    ),
                    "surface_role": source["role"],
                    "outside_component_connection": "not_planned",
                }
            )
    return result


def build(sources, *, method="cone", port_lift_m=8.0, ground_change_m=2.0, seconds=2.0):
    report = {
        "valid": False,
        "reason": "",
        "ground_method": method,
        "deferred_sources": [
            {"source_id": s["source_id"], "reason": s.get("omission_reason", "")}
            for s in sources
            if s["role"] == "deferred"
        ],
    }
    report["all_source_contacts_resolved"] = not report["deferred_sources"]
    report["source_level_order"] = [
        {
            "source_id": s["source_id"],
            "relative_layer": s["properties"].get(
                "layer", 1 if s["role"] == "deck" else 0
            ),
            "assumed": "layer" not in s["properties"],
        }
        for s in sources
        if s["role"] != "deferred"
    ]
    try:
        if (
            method not in ("cone", "polygon")
            or not all(np.isfinite(v) for v in (port_lift_m, ground_change_m, seconds))
            or not 0 <= port_lift_m <= 20
            or not 0 <= ground_change_m <= 20
            or not 0 < seconds <= 8
        ):
            raise ValueError("bounded port, ground and solver limits required")
        decks, ground_sources = check_sources(sources)
        flat, deck_domain, deck_layout = layout(decks)
        prior, _, ground_layout = layout(ground_sources)
        heights, fit = checked_fit(
            method,
            prior.vertices[:, :2],
            prior.triangles,
            prior.vertices[:, 2],
            maximum_grade=0.025,
            seconds=seconds,
        )
        report.update(
            ground_layout=ground_layout, deck_layout=deck_layout, ground_fit=fit
        )
        if heights is None:
            return (None, None), dict(report, reason=fit["reason"])
        ground = Mesh(np.c_[prior.vertices[:, :2], heights], prior.triangles)
        if fit["maximum_height_change_m"] > ground_change_m + 1e-7:
            return (None, None), dict(report, reason="ground_prior_change_budget")
        zero = Mesh(flat.vertices * [1, 1, 0], flat.triangles)
        bound = vertical_clearance(ground, zero)
        if not bound["valid"]:
            return (None, ground), dict(report, reason=bound["reason"])
        targets = np.array([s["end_heights_m"] for s in decks])
        clearance, depth, guard = 4.5, 0.35, 0.02
        height = max(
            -bound["minimum_clearance_m"] + clearance + depth + guard, targets.max()
        )
        lift = height - targets
        report.update(
            deck_height_m=float(height),
            port_lift_limit_m=port_lift_m,
            maximum_deck_port_lift_m=float(lift.max()),
            requested_clearance_m=clearance,
            deck_depth_m=depth,
            ground_change_limit_m=ground_change_m,
            lower_bound=bound,
            deck_domain_area_m2=deck_domain.area,
        )
        if np.any(lift > port_lift_m + 1e-7):
            return (None, ground), dict(report, reason="deck_port_height_budget")
        deck = Mesh(zero.vertices + [0, 0, height], zero.triangles)
        gap = vertical_clearance(
            ground, Mesh(deck.vertices - [0, 0, depth], deck.triangles)
        )
        if not gap["valid"] or gap["minimum_clearance_m"] < clearance - 1e-7:
            return (None, ground), dict(
                report, reason=gap["reason"] or "full_surface_clearance"
            )
        report.update(
            valid=True,
            reason="",
            clearance=gap,
            ports=port_report(sources, deck, ground),
            unique_used_sources=len(decks) + len(ground_sources),
            upper_profile="One constant shared deck; zero vertical grade at every source endpoint",
            ground_grade_recipe="Isotropic gradient norm <= 0.025; no authored crossfall",
            scope="Connected same-level corridor surfaces and clearance against explicit ground surfaces only. Deferred indoor floors have unknown heights and remain uncertified; not connected rail routes, neighboring world, or outside ramps. End heights are soft DEM priors; original source axes and complete buffered coverage retained. Flat deck is a deliberate structural recipe, without piers/load/track hardware or horizontal curve proof.",
        )
        return (deck, ground), report
    except ValueError as error:
        return (None, None), dict(
            report, reason="invalid_component_recipe", detail=str(error)
        )
