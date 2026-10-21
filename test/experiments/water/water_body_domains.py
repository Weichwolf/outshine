"""Bounded candidate domains, never an observed hydraulic/source admission."""

from collections import defaultdict
from dataclasses import dataclass
from itertools import combinations

import numpy as np
import shapely
from shapely.errors import GEOSException


@dataclass(frozen=True)
class Body:
    sources: tuple
    geometry: object
    policy: str


class Components:
    def __init__(self, indices):
        self.parent = {i: i for i in indices}

    def root(self, i):
        while self.parent[i] != i:
            self.parent[i] = self.parent[self.parent[i]]
            i = self.parent[i]
        return i

    def join(self, a, b):
        x, y = self.root(a), self.root(b)
        if x != y:
            self.parent[max(x, y)] = min(x, y)

    def groups(self):
        groups = defaultdict(list)
        for i in self.parent:
            groups[self.root(i)].append(i)
        return [tuple(sorted(v)) for v in groups.values()]


def merge_key(info, source, assume_ground):
    if info is None or source is None:
        return None, "missing_input"
    if (
        info.get("bridge") is not False
        or info.get("tunnel") is not False
        or info.get("crossing") != ""
    ):
        return None, "crossing_unresolved"
    if not assume_ground:
        return None, "ground_unresolved"
    if (
        source.get("origin") != 0
        or source.get("kind") != 1
        or not source.get("provider")
        or not source.get("revision")
    ):
        return None, "provenance_unresolved"
    if info.get("layer") != "water_polygons":
        return None, "unsupported_layer"
    base = (
        source["origin"],
        source["kind"],
        source["provider"],
        source["revision"],
        info["layer"],
        info.get("kind"),
    )
    if info.get("kind") == "river":
        return base, "procedural_ground_flow"
    if info.get("kind") not in ("lake", "pond", "swimming_pool", "dock", "ocean"):
        return None, "unsupported_class"
    if info.get("provider_feature_id") is None:
        return None, "feature_identity_unresolved"
    return base + (info["provider_feature_id"],), "procedural_ground_same_feature"


def candidate_pairs(indices, polygons, limit, work):
    """One scalar query allocates at most len(indices) hits; retain at most limit pairs."""
    tree = shapely.STRtree([polygons[i] for i in indices])
    pairs = []
    for a, i in enumerate(indices):
        hits = tree.query(polygons[i])
        work["tree_queries"] += 1
        work["max_query_hits"] = max(work["max_query_hits"], len(hits))
        work["max_query_payload_bytes"] = max(
            work["max_query_payload_bytes"], hits.nbytes
        )
        for b in sorted(int(x) for x in hits if x > a):
            work["candidate_pairs_seen"] += 1
            if len(pairs) >= limit:
                return None
            pairs.append((i, indices[b]))
            work["max_retained_pairs"] = max(work["max_retained_pairs"], len(pairs))
    return pairs


def box_pairs(indices, polygons, limit, work):
    bounds = {i: polygons[i].bounds for i in indices}
    pairs = []
    for a, b in combinations(indices, 2):
        work["box_pair_tests"] += 1
        x, y = bounds[a], bounds[b]
        if x[0] > y[2] or y[0] > x[2] or x[1] > y[3] or y[1] > x[3]:
            continue
        work["candidate_pairs_seen"] += 1
        if len(pairs) >= limit:
            return None
        pairs.append((a, b))
        work["max_retained_pairs"] = max(work["max_retained_pairs"], len(pairs))
    return pairs


def exact_component(indices, pairs, polygons, policy, remaining, work):
    if len(pairs) > remaining:
        return [], [{"source": i, "reason": "relation_budget"} for i in indices]
    components = Components(indices)
    try:
        for a, b in pairs:
            work["relation_tests"] += 1
            cut = polygons[a].intersection(polygons[b])
            if cut.area > 0 or cut.length > 0:
                components.join(a, b)
                work["positive_relations"] += 1
            elif not cut.is_empty:
                work["point_only_contacts"] += 1
    except GEOSException:
        return [], [{"source": i, "reason": "relation_geometry_error"} for i in indices]
    bodies, rejected = [], []
    for members in components.groups():
        try:
            body = (
                polygons[members[0]]
                if len(members) == 1
                else shapely.union_all([polygons[i] for i in members])
            )
            if (
                body.is_empty
                or not body.is_valid
                or body.geom_type not in ("Polygon", "MultiPolygon")
            ):
                raise ValueError("invalid union")
            bodies.append(Body(members, body, policy))
            work["union_sources"] += len(members) if len(members) > 1 else 0
        except (GEOSException, ValueError):
            rejected.extend(
                {"source": i, "reason": "union_geometry_error"} for i in members
            )
    return bodies, rejected


def assemble(
    polygons,
    inputs,
    sources,
    *,
    assume_ground=False,
    source_limit=1024,
    coordinate_limit=131072,
    candidate_limit=16384,
    relation_limit=16384,
    broad_phase="pairs",
):
    """Indices always address the caller's complete original polygon table.

    Potential envelope components are atomic for the expensive-relation budget.
    Exceeding a semantic bucket's candidate cap defers that bucket before any
    exact relations. Unrelated buckets and affordable components continue.
    """
    for n in (source_limit, coordinate_limit, candidate_limit, relation_limit):
        if type(n) is not int or n < 0:
            raise ValueError("nonnegative integer work budgets required")
    if (
        source_limit > 1024
        or coordinate_limit > 131072
        or candidate_limit > 32768
        or relation_limit > 65536
    ):
        raise ValueError("reference work limits exceed the supported local domain")
    if type(assume_ground) is not bool:
        raise ValueError("explicit boolean ground assumption required")
    if broad_phase not in ("tree", "pairs"):
        raise ValueError("unknown broad phase")
    count = len(polygons)
    if count > source_limit or len(inputs) != count or len(sources) != count:
        raise ValueError("bounded matching polygon/input/provenance tables required")
    if shapely.get_num_coordinates(polygons).sum() > coordinate_limit:
        raise ValueError("source coordinate budget exceeded")
    buckets = defaultdict(list)
    policies = {}
    bodies = []
    rejected = []
    work = {
        "tree_queries": 0,
        "box_pair_tests": 0,
        "max_query_hits": 0,
        "max_query_payload_bytes": 0,
        "candidate_pairs_seen": 0,
        "max_retained_pairs": 0,
        "candidate_pairs": 0,
        "relation_tests": 0,
        "positive_relations": 0,
        "point_only_contacts": 0,
        "union_sources": 0,
    }
    for i, polygon in enumerate(polygons):
        if (
            polygon.geom_type not in ("Polygon", "MultiPolygon")
            or polygon.is_empty
            or not polygon.is_valid
            or not np.isfinite(polygon.area)
            or not np.isfinite(shapely.get_coordinates(polygon)).all()
        ):
            rejected.append({"source": i, "reason": "invalid_source_geometry"})
            continue
        key, policy = merge_key(inputs[i], sources[i], assume_ground)
        if key is None:
            bodies.append(Body((i,), polygon, policy))
        else:
            buckets[key].append(i)
            policies[key] = policy
    for key, indices in buckets.items():
        if len(indices) == 1:
            bodies.append(Body(tuple(indices), polygons[indices[0]], policies[key]))
            continue
        pairs = (candidate_pairs if broad_phase == "tree" else box_pairs)(
            indices, polygons, candidate_limit, work
        )
        if pairs is None:
            rejected.extend(
                {"source": i, "reason": "candidate_bucket_budget"} for i in indices
            )
            continue
        work["candidate_pairs"] += len(pairs)
        broad = Components(indices)
        for a, b in pairs:
            broad.join(a, b)
        by_root = defaultdict(list)
        for pair in pairs:
            by_root[broad.root(pair[0])].append(pair)
        for group in sorted(broad.groups()):
            new, missing = exact_component(
                group,
                by_root[broad.root(group[0])],
                polygons,
                policies[key],
                relation_limit - work["relation_tests"],
                work,
            )
            bodies.extend(new)
            rejected.extend(missing)
    bodies.sort(key=lambda b: b.sources)
    rejected.sort(key=lambda r: r["source"])
    assigned = [i for body in bodies for i in body.sources] + [
        row["source"] for row in rejected
    ]
    if sorted(assigned) != list(range(count)):
        raise ValueError("incomplete/duplicated source partition")
    return (
        tuple(bodies),
        tuple(rejected),
        dict(
            work,
            source_count=count,
            assume_ground=assume_ground,
            scope="candidate domains; no observed hydraulic/height admission",
            broad_phase=broad_phase,
        ),
    )
