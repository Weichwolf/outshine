"""Explicit original old-end/new-vertex proposals; never infer topology by distance."""

from collections import defaultdict


def endpoint_bindings(state, sources, root, keys):
    cached = defaultdict(list)
    for key in state.get("cached_module_rows", {}):
        nodes = sources[key]["mvt_xy"]
        for vertex in (0, len(nodes) - 1):
            cached[tuple(nodes[vertex])].append((key, vertex))
    seen, contacts = set(), []
    for key in keys:
        for vertex, node in enumerate(sources[key]["mvt_xy"]):
            token = tuple(node)
            if token in seen:
                continue
            for old, old_vertex in sorted(cached[token]):
                if token == tuple(root["node"]) and old == root.get("port", {}).get(
                    "source_id"
                ):
                    continue
                contacts.append(
                    {
                        "source_id": key,
                        "vertex": vertex,
                        "cached_source_id": old,
                        "cached_vertex": old_vertex,
                    }
                )
            seen.add(token)
    if len(contacts) > 4:
        raise ValueError("cached secondary profile contact budget")
    return contacts
