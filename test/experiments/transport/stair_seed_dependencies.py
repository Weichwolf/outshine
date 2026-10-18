"""Bounded whole-source dependencies for procedural ground walking crossings."""

import shapely
from stair_seed_crossings import crossing_point
from stair_surface_access import contact_access


def crossing_kind(source):
    props = source["properties"]
    if props.get("class") == "minor" and not props.get("subclass"):
        return "road"
    if props.get("class") == "path" and props.get("subclass") == "footway":
        return "foot"
    return ""


def prepare(records, sources, method):
    if method not in ("index", "scan") or len(records) > 20000:
        raise ValueError("known bounded seed dependency search required")
    if sum(len(r["nodes"]) for r in records) > 200000:
        raise ValueError("seed dependency original node budget exhausted")
    keys = sorted(
        r["source_id"]
        for r in records
        if r["eligible"] and crossing_kind(sources[r["source_id"]])
    )
    if len(set(keys)) != len(keys):
        raise ValueError("unique seed dependency identities required")
    lines = [shapely.LineString(sources[k]["xy_m"]) for k in keys]
    return {
        "keys": keys,
        "lines": dict(zip(keys, lines, strict=True)),
        "kinds": {k: crossing_kind(sources[k]) for k in keys},
        "sources": sources,
        "tree": shapely.STRtree(lines) if method == "index" else None,
    }


def declaration(road, foot):
    return {
        "source_id": road,
        "footway_source_id": foot,
        "kind": "minor_footway_crossing",
        "modes": ["walk"],
        "access_assumption": "procedural_walk_default",
    }


def expand(keys, prepared, *, max_sources=8, max_contacts=4, max_visits=2000000):
    if (
        type(max_sources) is not int
        or not 1 <= max_sources <= 8
        or type(max_contacts) is not int
        or not 1 <= max_contacts <= 4
        or type(max_visits) is not int
        or not 1 <= max_visits <= 2000000
        or not 1 <= len(keys) <= max_sources
        or len(set(keys)) != len(keys)
    ):
        raise ValueError("bounded unique whole seed dependencies required")
    selected, pairs, contacts, rejected = list(keys), set(), [], []
    work = {"source_queries": 0, "candidate_visits": 0, "axis_tests": 0}
    result = {
        "initial_source_ids": list(keys),
        "source_ids": selected,
        "geometric_junctions": contacts,
        "rejected_pairs": rejected,
        "work": work,
        "reason": "",
    }
    for key in selected:
        if key not in prepared["lines"]:
            continue
        work["source_queries"] += 1
        tree = prepared["tree"]
        candidates = (
            sorted(prepared["keys"][int(i)] for i in tree.query(prepared["lines"][key]))
            if tree is not None
            else prepared["keys"]
        )
        for other in candidates:
            if work["candidate_visits"] >= max_visits:
                result["reason"] = "seed_dependency_work_budget"
                return result
            work["candidate_visits"] += 1
            if key == other or (
                prepared["kinds"][key] == prepared["kinds"][other] == "road"
            ):
                continue
            road, foot = (
                (key, other)
                if prepared["kinds"][key] == "road"
                else (other, key)
                if prepared["kinds"][other] == "road"
                else tuple(sorted((key, other)))
            )
            if (road, foot) in pairs:
                continue
            if work["axis_tests"] >= 65536:
                result["reason"] = "seed_dependency_pair_budget"
                return result
            work["axis_tests"] += 1
            if not prepared["lines"][road].crosses(prepared["lines"][foot]):
                continue
            if len(pairs) >= 64:
                result["reason"] = "seed_dependency_pair_evidence_budget"
                return result
            pairs.add((road, foot))
            contact = declaration(road, foot)
            if prepared["kinds"][road] == "foot":
                contact["kind"] = "footway_footway_crossing"
            try:
                crossing_point(prepared["sources"][road], prepared["sources"][foot])
                contact_access(
                    contact, prepared["sources"][road], prepared["sources"][foot], {}
                )
            except ValueError as error:
                rejected.append(
                    {"source_id": road, "footway_source_id": foot, "reason": str(error)}
                )
                continue
            if len(contacts) >= max_contacts:
                result["reason"] = "seed_dependency_contact_budget"
                return result
            contacts.append(contact)
            if other not in selected:
                if len(selected) >= max_sources:
                    result["reason"] = "seed_dependency_source_budget"
                    return result
                selected.append(other)
    return result


def recipes_with_dependencies(recipes, records, sources, method):
    if len(recipes) > 32:
        raise ValueError("bounded seed dependency recipe list required")
    prepared = prepare(records, sources, method)
    accepted, ledger, seen = [], [], set()
    for index, recipe in enumerate(recipes):
        proof = expand(recipe["source_ids"], prepared)
        ledger.append({"recipe_index": index, **proof})
        if proof["reason"]:
            continue
        keys = proof["source_ids"]
        contacts = sorted(
            proof["geometric_junctions"],
            key=lambda c: (c["source_id"], c["footway_source_id"]),
        )
        identity = (recipe["seed"]["source_id"], tuple(sorted(keys)))
        if identity in seen:
            continue
        seen.add(identity)
        accepted.append(
            {
                **recipe,
                "source_ids": keys,
                "source_profiles": {
                    k: "unclassified"
                    for k in keys
                    if crossing_kind(sources[k]) == "road"
                },
                **(
                    {
                        "geometric_junctions": contacts,
                        "junction_height_mode": "rounded_joint",
                        "terrain_layout_mode": "free_region",
                    }
                    if contacts
                    else {}
                ),
            }
        )
    return accepted, ledger
