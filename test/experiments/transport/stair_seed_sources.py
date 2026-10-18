"""Bounded whole original ground components separated from existing road bodies."""

import numpy as np
from outshine_ref.transport.frontiers import connected_prefixes, discover_source_modules
from stair_frontier_sources import records_for
from stair_growth_sources import roots_first
from stair_local_context import radius
from stair_module_inputs import reference_profiles
from stair_seed_axes import axis_form


def separated_records(view, assets, manifest, sources):
    bounds = np.array([p.bounds_xy_m for p in view.patches])
    state = {
        "plan": {"xy": bounds.reshape(-1, 2)},
        "report": {"candidate_source_ids": manifest["source_ids"]},
    }
    records = records_for(state, sources)
    profiles, queries, hits = reference_profiles(), 0, 0
    guard = radius(view)
    for row in records:
        if not row["eligible"]:
            continue
        source = sources[row["source_id"]]
        try:
            axis_form(source)
        except ValueError:
            row.update(
                eligible=False, reason="seed_repeated_original_node_recipe_missing"
            )
            continue
        kind = source["properties"].get("subclass") or "unclassified"
        band = source["shape"].buffer(
            profiles[kind]["widthM"] / 2,
            cap_style="flat",
            join_style="mitre",
            mitre_limit=2,
        )
        for asset in assets:
            if queries >= 65536 or hits >= 1000000:
                row.update(eligible=False, reason="seed_spatial_probe_budget")
                break
            found = asset.tree.query(
                band, predicate="dwithin", distance=0.2 + guard + asset.xy_rounding_m
            )
            queries += 1
            hits += len(found)
            if hits > 1000000:
                row.update(eligible=False, reason="seed_spatial_probe_budget")
                break
            if asset.data["ground"][found].any():
                row.update(
                    eligible=False, reason="seed_requires_explicit_old_ground_contact"
                )
                break
    return records, {
        "old_asset_queries": queries,
        "candidate_old_facets": hits,
        "xy_guard_m": guard,
    }


def select(
    view,
    assets,
    manifest,
    sources,
    reference,
    *,
    method="index",
    strategy="prefix",
    schedule="roots_first",
    grid_m=2**-11,
):
    if grid_m not in (0.0, 2**-11):
        raise ValueError("known independent seed grid required")
    if strategy not in ("whole", "prefix"):
        raise ValueError("known whole or connected-prefix seed strategy required")
    records, work = separated_records(view, assets, manifest, sources)
    eligible = sorted(
        (r for r in records if r["eligible"]),
        key=lambda r: (-r["length_m"], r["source_id"]),
    )
    roots = [
        {
            "root_id": "seed-" + str(i).zfill(5),
            "node": r["nodes"][0],
            "source_id": r["source_id"],
        }
        for i, r in enumerate(eligible[:64])
    ]
    plan = discover_source_modules(records, roots, method=method)
    recipes = []
    by_root = {r["root_id"]: r for r in roots}
    for proposal in plan["proposals"]:
        if not proposal["source_ids"]:
            continue
        choices = (
            connected_prefixes(proposal)
            if strategy == "prefix"
            else (proposal["source_ids"],)
        )
        for keys in choices:
            first = sources[keys[0]]["mvt_xy"]
            end = 0 if first[0] == by_root[proposal["root_id"]]["node"] else -1
            recipes.append(
                {
                    "version": 1,
                    **reference,
                    "contact_mode": "seed",
                    "junction_mode": "level_box",
                    "seed": {"source_id": keys[0], "end": end},
                    "source_ids": list(keys),
                    "source_profiles": {
                        k: "unclassified"
                        for k in keys
                        if sources[k]["properties"].get("class") == "minor"
                    },
                    "traffic_mode": "source_clearance",
                    "soil_bridge_reserve_m": 0.0,
                    **({"seed_grid_m": grid_m} if grid_m else {}),
                    "scope": "Independent whole original ground component; no cached profile or old surface reuse. Whole physical admission remains mandatory.",
                }
            )
    if schedule == "roots_first":
        recipes = roots_first(recipes)
    elif schedule != "prefix_first":
        raise ValueError("known bounded seed attempt schedule required")
    return {
        "plan": plan,
        "roots": roots,
        "deferred_profiles": [],
        "recipes": recipes,
        "seed_work": work,
        "profile_work": [],
        "schedule": schedule,
        "family": "seed",
        "scope": "Bounds/old-ground separation prefilter; independent whole-source/terrain/bridge/pier admission required. No world graph/cache/runtime claim.",
    }
