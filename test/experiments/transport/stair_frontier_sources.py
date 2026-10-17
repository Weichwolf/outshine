"""Explicit ordinary-ground eligibility and actual cached-interior seed binding."""

from collections import defaultdict

import numpy as np
import shapely
from outshine_ref.transport.frontiers import discover_source_modules
from stair_module_inputs import reference_profiles
from stair_module_roots import root_profile


def reason(source, cached, domain, profiles):
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
    band = source["shape"].buffer(
        half, cap_style="flat", join_style="mitre", mitre_limit=2
    )
    if not domain.covers(band):
        return "outside_current_complete_terrain"
    return ""


def records_for(state, sources):
    xy = state["plan"]["xy"]
    domain = shapely.box(*xy.min(axis=0), *xy.max(axis=0))
    cached = set(state["report"]["candidate_source_ids"])
    profiles = reference_profiles()
    records = []
    for key in sorted(sources):
        source = sources[key]
        why = reason(dict(source, source_id=key), cached, domain, profiles)
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


def root_seeds(state, sources, records):
    endpoints = defaultdict(list)
    for row in records:
        if row["eligible"]:
            for end in (0, -1):
                endpoints[tuple(row["nodes"][end])].append(row["source_id"])
    roots, deferred = [], []
    for key in sorted(state.get("cached_module_rows", {})):
        for vertex, node in enumerate(sources[key]["mvt_xy"][1:-1], 1):
            if tuple(node) not in endpoints:
                continue
            port = {"source_id": key, "vertex": vertex}
            root_id = key + "@" + str(vertex)
            if len(roots) + len(deferred) >= 64:
                deferred.append(
                    {"root_id": root_id, "reason": "verified_seed_probe_budget"}
                )
                continue
            try:
                root_profile(state, {"port": port, "contact_mode": "profile"}, sources)
            except ValueError as error:
                deferred.append({"root_id": root_id, "reason": str(error)})
                continue
            roots.append({"root_id": root_id, "node": node, "port": port})
    return roots, deferred


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
    profiles = {
        key: "unclassified"
        for key in keys
        if sources[key]["properties"].get("class") == "minor"
    }
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
        "interior_contacts": contact_bindings(state, sources, root, keys),
        "scope": "Automatically proposed whole original ground sources at a verified cached interior profile; whole geometry admission remains mandatory.",
    }


def proposals(state, sources, reference, *, method="index"):
    records = records_for(state, sources)
    roots, deferred = root_seeds(state, sources, records)
    plan = discover_source_modules(records, roots, method=method)
    by_root = {r["root_id"]: r for r in roots}
    recipes = []
    for proposal in plan["proposals"]:
        if not proposal["source_ids"]:
            continue
        try:
            recipe = module_recipe(
                reference, state, sources, by_root[proposal["root_id"]], proposal
            )
        except ValueError as error:
            deferred.append({"root_id": proposal["root_id"], "reason": str(error)})
            continue
        recipes.append(recipe)
    return plan, records, roots, deferred, recipes
