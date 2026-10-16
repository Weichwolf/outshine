"""Whole declared band interactions and vertex links, independent of the LP."""

import itertools
from collections import defaultdict

import shapely
from shared_bridge_geometry import corridor
from stair_context_experiment import crossings
from stair_junction_layout import landings


def interactions(rows, nodes):
    domains = [corridor(r) for r in rows]
    _points, owners, masks = landings(rows, domains, "width", 0.75, nodes)
    pairs = crossings(rows, nodes)
    result, areas = [], {}
    for i, j in itertools.combinations(range(len(rows)), 2):
        overlap = domains[i].intersection(domains[j])
        common = shapely.union_all(
            [
                mask
                for mask, ids in zip(masks, owners, strict=True)
                if i in ids and j in ids
            ]
        )
        expected = overlap.difference(common)
        if expected.area <= 1e-7:
            continue
        matching = [p for p in pairs if set(p[:2]) == {i, j}]
        cover = shapely.union_all(
            [
                region
                for region in shapely.get_parts(overlap)
                if any(region.distance(shapely.Point(p[2])) < 1e-7 for p in matching)
            ]
        )
        if expected.difference(cover).area > 1e-7 or not matching:
            raise ValueError(
                "non-axis band overlap requires an explicit separation recipe"
            )
        result.extend(matching)
        areas[f"{matching[0][0]}:{matching[0][1]}"] = float(expected.area)
    return result, {"noncontact_overlap_areas_m2": areas}


def vertex_links(vertices, faces):
    links = defaultdict(list)
    for a, b, c in faces:
        links[int(a)].append((int(b), int(c)))
        links[int(b)].append((int(c), int(a)))
        links[int(c)].append((int(a), int(b)))
    if len(links) != len(vertices):
        raise ValueError("closed body contains unused vertices")
    for edges in links.values():
        adjacency = defaultdict(list)
        for a, b in edges:
            adjacency[a].append(b)
            adjacency[b].append(a)
        if any(len(v) != 2 for v in adjacency.values()):
            raise ValueError("body vertex link is not cyclic")
        seen, todo = set(), [next(iter(adjacency))]
        while todo:
            v = todo.pop()
            if v not in seen:
                seen.add(v)
                todo.extend(adjacency[v])
        if len(seen) != len(adjacency):
            raise ValueError("body has a pinched disconnected vertex link")
    return {"cyclic_connected_vertex_links": len(links)}
