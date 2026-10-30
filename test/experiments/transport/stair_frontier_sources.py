"""Explicit ordinary-ground eligibility and bounded actual cached-profile recipes."""

from collections import defaultdict

import numpy as np
import shapely
from outshine_ref.transport.frontiers import connected_prefixes, discover_source_modules
from stair_frontier_endpoints import endpoint_bindings
from stair_frontier_roots import root_seeds
from stair_module_inputs import reference_profiles


def reason(source, cached, domain, profiles, *, requested=None, work=None):
    if source["source_id"] in cached:
        return "already_cached"
    props = source["properties"]
    if props.get("indoor"):
        return "indoor_recipe_missing"
    if props.get("brunnel"):
        return "vertical_structure_recipe_missing"
    if props.get("layer", 0) != 0 or props.get("level", 0) != 0:
        return "non_ground_level_recipe_missing"
    kind = props.get("subclass")
    ordinary = props.get("class") == "minor" and not kind
    if kind not in ("footway", "cycleway") and not ordinary:
        return "ordinary_source_profile_missing"
    xy = np.array(source["xy_m"])
    if not 2 <= len(xy) <= 256 or np.any(
        np.linalg.norm(np.diff(xy, axis=0), axis=1) < 1e-6
    ):
        return "whole_axis_geometry_budget_or_degeneracy"
    half = profiles["unclassified" if ordinary else kind]["widthM"] / 2
    if requested is not None:
        bounds, query = source["shape"].bounds, requested.bounds
        guard = 64 * np.finfo(float).eps * max(1.0, *map(abs, bounds))
        # GEOS clips a limited mitre on a segment centred at 2r with half-length r.
        # Its endpoints can reach sqrt(5)r, beyond the nominal mitre limit 2r.
        reach = np.hypot(2.0, 1.0) * half + guard
        if work is not None:
            work["request_bounds_tests"] += 1
        if (
            bounds[2] + reach < query[0]
            or bounds[3] + reach < query[1]
            or bounds[0] - reach > query[2]
            or bounds[1] - reach > query[3]
        ):
            if work is not None:
                work["request_bounds_rejections"] += 1
            return "outside_requested_source_bounds"
    if work is not None:
        work["corridor_builds"] += 1
    band = source["shape"].buffer(
        half, cap_style="flat", join_style="mitre", mitre_limit=2
    )
    if requested is not None and not requested.intersects(band):
        return "outside_requested_source_bounds"
    if not domain.covers(band):
        return "outside_current_complete_terrain"
    return ""


def records_for(state, sources, *, requested_bounds=None, work=None):
    requested = None
    if requested_bounds is not None:
        bounds = np.asarray(requested_bounds, float)
        if (
            bounds.shape != (4,)
            or not np.isfinite(bounds).all()
            or abs(bounds).max() > 1e6
            or np.any(bounds[:2] >= bounds[2:])
        ):
            raise ValueError("bounded finite nonempty requested source bounds required")
        requested = shapely.box(*bounds)
    if work is not None:
        work.update(
            source_headers=len(sources),
            request_bounds_tests=0,
            request_bounds_rejections=0,
            corridor_builds=0,
        )
    xy = state["plan"]["xy"]
    domain = shapely.box(*xy.min(axis=0), *xy.max(axis=0))
    cached = set(
        state.get("resident_source_ids", state["report"]["candidate_source_ids"])
    )
    profiles = reference_profiles()
    records = []
    for key in sorted(sources):
        source = sources[key]
        why = reason(
            dict(source, source_id=key),
            cached,
            domain,
            profiles,
            requested=requested,
            work=work,
        )
        records.append(
            {
                "source_id": key,
                "nodes": source["mvt_xy"],
                "length_m": float(source["shape"].length),
                "eligible": not bool(why),
                "reason": why,
            }
        )
    return records


def contact_bindings(state, sources, root, keys):
    cached_nodes = defaultdict(list)
    for key in state.get("cached_module_rows", {}):
        for vertex, node in enumerate(sources[key]["mvt_xy"]):
            cached_nodes[tuple(node)].append((key, vertex))
    seen, contacts = set(), []
    for key in keys:
        for end in (0, -1):
            node = tuple(sources[key]["mvt_xy"][end])
            if node == tuple(root["node"]) or node in seen:
                continue
            choices = [
                (old, vertex)
                for old, vertex in cached_nodes[node]
                if 0 < vertex < len(sources[old]["mvt_xy"]) - 1
            ]
            if not choices:
                continue
            old, _vertex = min(choices)
            contacts.append({"source_id": key, "end": end, "cached_source_id": old})
            seen.add(node)
    if len(contacts) > 4:
        raise ValueError("cached secondary profile contact budget")
    return contacts


def module_recipe(reference, state, sources, root, proposal):
    keys = proposal["source_ids"]
    if root.get("initial_source_id", keys[0]) != keys[0]:
        raise ValueError("whole-source prefix must retain its explicit first source")
    profiles = {
        key: "unclassified"
        for key in keys
        if sources[key]["properties"].get("class") == "minor"
    }
    interiors = contact_bindings(state, sources, root, keys)
    endpoints = endpoint_bindings(state, sources, root, keys)
    if len(interiors) + len(endpoints) > 4:
        raise ValueError("cached secondary profile contact budget")
    return {
        "version": 1,
        **reference,
        "port": root["port"],
        "source_ids": keys,
        "source_profiles": profiles,
        "soil_bridge_reserve_m": 0.0,
        "contact_mode": "profile",
        "traffic_mode": "source_clearance",
        "junction_mode": "level_box",
        **({"terrain_layout_mode": "free_region"} if root.get("endpoint") else {}),
        "interior_contacts": interiors,
        **({"endpoint_contacts": endpoints} if endpoints else {}),
        "scope": (
            "Automatically proposed whole original ground sources at a verified cached outer profile; whole geometry admission remains mandatory."
            if root.get("endpoint")
            else "Automatically proposed whole original ground sources at a verified cached interior profile; whole geometry admission remains mandatory."
        ),
    }


def proposals(state, sources, reference, *, method="index", strategy="whole"):
    if strategy not in ("whole", "prefix"):
        raise ValueError("known bounded whole or connected-prefix strategy required")
    records = records_for(state, sources)
    roots, deferred = root_seeds(state, sources, records)
    plan = discover_source_modules(records, roots, method=method)
    recipes = recipes_for(plan, roots, state, sources, reference, strategy, deferred)
    return plan, records, roots, deferred, recipes


def recipes_for(plan, roots, state, sources, reference, strategy, deferred):
    by_root = {r["root_id"]: r for r in roots}
    recipes = []
    for proposal in plan["proposals"]:
        if not proposal["source_ids"]:
            continue
        choices = (
            connected_prefixes(proposal)
            if strategy == "prefix"
            else (proposal["source_ids"],)
        )
        for keys in choices:
            try:
                recipe = module_recipe(
                    reference,
                    state,
                    sources,
                    by_root[proposal["root_id"]],
                    dict(proposal, source_ids=list(keys)),
                )
            except ValueError as error:
                deferred.append(
                    {
                        "root_id": proposal["root_id"],
                        "source_ids": list(keys),
                        "reason": str(error),
                    }
                )
                continue
            if strategy == "prefix":
                recipe["source_fallback"] = {
                    "root_id": proposal["root_id"],
                    "full_source_ids": proposal["source_ids"],
                    "omitted_source_ids": proposal["source_ids"][len(keys) :],
                    "scope": "Whole connected source prefix; omitted sources remain original candidates for later recipes, not individually disproven.",
                }
            recipes.append(recipe)
    return recipes
