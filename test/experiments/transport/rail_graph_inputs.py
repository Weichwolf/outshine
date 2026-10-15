"""Pinned noded axes and conservative local switch/crossing hardware gaps."""

import hashlib
import json
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely


def read_window(root, name):
    root = Path(root)
    pins = json.loads((root / "generation.json").read_text())["files"]

    def checked(relative):
        path = root / relative
        if hashlib.sha256(path.read_bytes()).hexdigest() != pins[relative]:
            raise ValueError("captured rail input changed: " + relative)
        return path

    prefix = f"results/{name}/components/"
    source = json.loads(checked(f"inputs/{name}-input.json").read_text())
    raw = {r["source_id"]: r for r in source["roads"]}
    ledger = json.loads(checked(prefix + "source-usage.json").read_text())
    valid_ids = {r["id"] for r in ledger if r["status"] != "not_used"}
    ids = json.loads(checked(prefix + "source-ids.json").read_text())
    with np.load(checked(prefix + "graph.npz")) as data:
        arrays = {k: data[k] for k in data.files}
    if (
        sum(len(v) for k, v in arrays.items() if k.endswith("_edges")) > 65536
        or sum(len(v) for k, v in arrays.items() if k.endswith("_line_coordinates"))
        > 262144
    ):
        raise ValueError("rail_input_graph_budget")
    bed = shapely.from_wkb(checked(prefix + "domain.wkb").read_bytes())
    graphs = []
    for index in sorted({k.split("_")[0] for k in arrays}):
        graph = {
            k.removeprefix(index + "_"): v
            for k, v in arrays.items()
            if k.startswith(index + "_")
        }
        cuts, xy = graph["line_offsets"], graph["line_coordinates"]
        graph["lines"] = np.array(
            [shapely.LineString(xy[a:b]) for a, b in pairwise(cuts)],
            object,
        )
        offsets, owners = graph["owner_offsets"], graph["owner_sources"]
        graph["sources"] = [
            tuple(ids[j] for j in owners[a:b]) for a, b in pairwise(offsets)
        ]
        graph["eligible"] = np.array(
            [
                any(s in valid_ids for s in sources)
                and all(
                    raw[s]["properties"].get("class") == "rail"
                    and raw[s]["properties"].get("subclass", "rail")
                    in ("rail", "light_rail")
                    for s in sources
                )
                for sources in graph["sources"]
            ]
        )
        graph["rail_graph"] = all(
            raw[s]["properties"].get("class") in ("rail", "transit")
            for sources in graph["sources"]
            for s in sources
        )
        graphs.append(graph)
    return (
        source,
        raw,
        graphs,
        bed,
        {
            "generation_sha256": hashlib.sha256(
                (root / "generation.json").read_bytes()
            ).hexdigest(),
            "input_sha256": pins[f"inputs/{name}-input.json"],
        },
    )


def rail_chains(graph):
    edges, lines = graph["edges"], graph["lines"]
    adjacency = [[] for _ in graph["vertices"]]
    for index in np.flatnonzero(graph["eligible"]):
        for node in edges[index]:
            adjacency[node].append(int(index))
    visited, chains = set(), []

    def walk(start, first):
        node, edge, points, members = start, first, [], []
        while edge not in visited:
            visited.add(edge)
            members.append(edge)
            a, b = edges[edge]
            xy = shapely.get_coordinates(lines[edge])
            if node == b:
                xy = xy[::-1]
            points.extend(xy if not points else xy[1:])
            node = int(b if node == a else a)
            if len(adjacency[node]) != 2:
                break
            edge = next(e for e in adjacency[node] if e != edge)
        sources = sorted({s for e in members for s in graph["sources"][e]})
        return {"xy": np.array(points), "edges": members, "sources": sources}

    for node, incident in enumerate(adjacency):
        if len(incident) != 2:
            for edge in incident:
                if edge not in visited:
                    chains.append(walk(node, edge))
    for edge in np.flatnonzero(graph["eligible"]):
        if edge not in visited:
            chains.append(walk(int(edges[edge, 0]), int(edge)))
    branches = graph["vertices"][[i for i, e in enumerate(adjacency) if len(e) > 2]]
    return chains, branches


def bounded_pairs(tree, shapes, **options):
    batches, count = [], 0
    for begin in range(0, len(shapes), 64):
        pairs = tree.query(shapes[begin : begin + 64], **options)
        count += pairs.shape[1]
        if count > 100000:
            raise ValueError("rail_pair_budget")
        pairs[0] += begin
        batches.append(pairs)
    return np.concatenate(batches, axis=1) if batches else np.empty((2, 0), int)


def safe_pieces(chains, branches, land, reach):
    if len(chains) > 2048:
        raise ValueError("rail_chain_budget")
    lines = np.array([shapely.LineString(c["xy"]) for c in chains], object)
    radius = reach + 0.05
    carriers = shapely.buffer(lines, radius / np.cos(np.pi / 32), quad_segs=8)
    pairs = bounded_pairs(shapely.STRtree(carriers), carriers, predicate="intersects")
    a, b = pairs[:, pairs[0] < pairs[1]]
    conflicts = shapely.union_all(shapely.intersection(carriers[a], carriers[b]))
    conflicts = conflicts.buffer(radius / np.cos(np.pi / 32), quad_segs=8)
    if len(branches):
        conflicts = conflicts.union(
            shapely.union_all(
                shapely.buffer(shapely.points(branches), 6.0, quad_segs=8)
            )
        )
    land_lines = (
        np.concatenate([g["lines"] for g in land]) if land else np.empty(0, object)
    )
    half = np.concatenate([g["half_widths"] for g in land]) if land else np.empty(0)
    hits = bounded_pairs(
        shapely.STRtree(land_lines),
        lines,
        predicate="dwithin",
        distance=float(half.max(initial=0)) + radius + 0.5,
    )
    chosen = np.unique(hits[1])
    obstacles = shapely.buffer(
        land_lines[chosen],
        (half[chosen] + radius + 0.5) / np.cos(np.pi / 32),
        quad_segs=8,
    )
    conflicts = conflicts.union(shapely.union_all(obstacles))
    output, ledger = [], []
    for index, line in enumerate(lines):
        kept = shapely.get_parts(line.difference(conflicts))
        kept = kept[(shapely.get_type_id(kept) == 1) & ~shapely.is_empty(kept)]
        length = float(shapely.length(kept).sum())
        ledger.append(
            {
                "chain": index,
                "sources": chains[index]["sources"],
                "original_length_m": line.length,
                "candidate_length_m": length,
                "excluded_length_m": max(0.0, line.length - length),
                "exclusion_reason": "unsupported_switch_or_crossing_hardware"
                if length < line.length - 1e-7
                else "",
            }
        )
        for piece in kept:
            if len(output) >= 2048:
                raise ValueError("rail_piece_budget")
            output.append(
                {
                    "chain": index,
                    "xy": shapely.get_coordinates(piece),
                    "sources": chains[index]["sources"],
                }
            )
    return (
        output,
        ledger,
        {
            "chain_pair_candidates": pairs.shape[1],
            "land_pair_candidates": hits.shape[1],
            "switch_gap_radius_m": 6.0,
            "carrier_reach_m": reach,
        },
    )


def chain_recipe(sources, raw):
    values = []
    for source in sources:
        gauge = raw[source]["properties"].get("gauge")
        if gauge is not None:
            try:
                value = float(gauge) / 1000
            except (ValueError, TypeError):
                return None, "rail_gauge_not_supported"
            if not np.isfinite(value) or not 0.5 <= value <= 2.0:
                return None, "rail_gauge_not_supported"
            values.append(value)
    if values and max(values) - min(values) > 1e-9:
        return None, "rail_gauge_transition_not_planned"
    return values[
        0
    ] if values else 1.435, "tag_bound" if values else "procedural_default"
