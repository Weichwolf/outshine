"""Bounded original contacts at actual cached interior and outer Ground profiles."""

from collections import Counter, defaultdict

from stair_module_roots import root_profile
from stair_root_contact import new_through_root


def candidates(state, sources, records):
    endpoints, through = defaultdict(list), defaultdict(list)
    for row in records:
        if not row["eligible"]:
            continue
        for end in (0, -1):
            endpoints[tuple(row["nodes"][end])].append(row["source_id"])
        counts = Counter(map(tuple, row["nodes"]))
        for node in sorted(set(map(tuple, row["nodes"][1:-1]))):
            if counts[node] == 1:
                through[node].append(row["source_id"])
    cached = sorted(state.get("cached_module_rows", {}))
    for key in cached:
        for vertex, node in enumerate(sources[key]["mvt_xy"][1:-1], 1):
            if tuple(node) in endpoints:
                yield {
                    "root_id": key + "@" + str(vertex),
                    "node": node,
                    "port": {"source_id": key, "vertex": vertex},
                }
    for key in cached:
        for end in (0, -1):
            node = sources[key]["mvt_xy"][end]
            for new in sorted(through[tuple(node)]):
                yield {
                    "root_id": key + "@" + str(end) + "/" + new,
                    "node": node,
                    "port": {"source_id": key, "end": end},
                    "endpoint": True,
                    "initial_source_id": new,
                }


def root_seeds(state, sources, records, *, max_profiles=64):
    if type(max_profiles) is not int or not 0 <= max_profiles <= 64:
        raise ValueError("bounded actual profile probe capacity required")
    roots, deferred = [], []
    for root in candidates(state, sources, records):
        reason = ""
        if len(roots) + len(deferred) >= max_profiles:
            reason = "verified_seed_probe_budget"
        elif len(root["root_id"]) > 1024:
            reason = "verified_seed_identity_budget"
        if reason:
            deferred.append({"root_id": root["root_id"], "reason": reason})
            continue
        recipe = {"port": root["port"], "contact_mode": "profile"}
        try:
            if root.get("endpoint"):
                recipe["source_ids"] = [root["initial_source_id"]]
                if not new_through_root(recipe, sources):
                    raise ValueError("unique original new through root required")
            root_profile(state, recipe, sources)
        except ValueError as error:
            deferred.append({"root_id": root["root_id"], "reason": str(error)})
            continue
        roots.append(root)
    return roots, deferred
