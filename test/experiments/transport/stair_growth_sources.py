"""Discover original-source modules using separate resident road-profile owners."""

import numpy as np
from outshine_ref.transport.frontiers import discover_source_modules
from stair_frontier_sources import recipes_for, records_for, root_seeds


def asset_state(asset):
    data = asset.data
    bits = np.uint64(1) << np.arange(len(asset.source_ids), dtype=np.uint64)
    owners = data["owners"].astype(np.uint64) @ bits
    return {
        "road": {
            "body_vertices": data["body_vertices"],
            "top_faces": data["top_faces"],
            "face_sources": owners,
        },
        "road_ground": data["ground"],
        "cached_module_rows": {r["source_id"]: r for r in asset.rows},
        "report": {"candidate_source_ids": list(asset.source_ids)},
    }


def roots_first(recipes):
    groups = {}
    for recipe in recipes:
        port = recipe["port"]
        key = port["source_id"], port["vertex"]
        groups.setdefault(key, []).append(recipe)
    return [
        group[depth]
        for depth in range(max(map(len, groups.values()), default=0))
        for group in groups.values()
        if depth < len(group)
    ]


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
):
    if strategy not in ("whole", "prefix"):
        raise ValueError("known whole or connected-prefix selection required")
    bounds = np.array([p.bounds_xy_m for p in view.patches])
    state = {
        "plan": {"xy": bounds.reshape(-1, 2)},
        "report": {"candidate_source_ids": manifest["source_ids"]},
        "cached_module_rows": {r["source_id"]: r for a in assets for r in a.rows},
    }
    records = records_for(state, sources)
    roots, deferred, work, remaining = [], [], [], 64
    for asset in assets:
        local = asset_state(asset)
        found, rejected = root_seeds(local, sources, records, max_profiles=remaining)
        tested = len(found) + sum(
            r["reason"] != "verified_seed_probe_budget" for r in rejected
        )
        work.append(
            {
                "asset": asset.identity,
                "source_columns": len(asset.source_ids),
                "mask_bytes": local["road"]["face_sources"].nbytes,
                "owner_column_conversion_bytes": asset.data["owners"].size * 8,
                "body_arrays_copied_bytes": 0,
                "verified_roots": len(found),
                "actual_profile_probes": tested,
            }
        )
        remaining -= tested
        roots.extend(found)
        deferred.extend(rejected)
    plan = discover_source_modules(records, roots, method=method)
    recipes = recipes_for(plan, roots, state, sources, reference, strategy, deferred)
    if schedule == "roots_first":
        recipes = roots_first(recipes)
    elif schedule != "prefix_first":
        raise ValueError("known bounded geometry attempt schedule required")
    return {
        "plan": plan,
        "roots": roots,
        "deferred_profiles": deferred,
        "recipes": recipes,
        "profile_work": work,
        "schedule": schedule,
        "scope": "Bounds-only source prefilter; exact local geometry/terrain admission remains mandatory. Asset-local masks only; immutable body buffers shared.",
    }
