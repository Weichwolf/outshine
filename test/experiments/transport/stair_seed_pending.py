"""Bounded original neighbour bindings before freezing an independent road seed."""

import numpy as np
from stair_seed_axes import axis_form


def ordinary_profile(source, profiles):
    props = source["properties"]
    if (
        props.get("brunnel")
        or props.get("indoor")
        or props.get("layer", 0) != 0
        or props.get("level", 0) != 0
    ):
        raise ValueError("pending junction requires ordinary exterior ground sources")
    if props.get("class") == "minor" and not props.get("subclass"):
        return profiles["unclassified"]
    kind = props.get("subclass")
    if props.get("class") != "path" or kind not in ("footway", "cycleway"):
        raise ValueError("pending junction lacks a declared ordinary source profile")
    return profiles[kind]


def original_node(source, vertex):
    form = axis_form(source)
    xy = np.asarray(source["xy_m"], float)
    count = len(source["mvt_xy"])
    if (
        type(vertex) is not int
        or not 0 <= vertex < count
        or xy.shape != (count, 2)
        or not np.isfinite(xy).all()
        or (form == "closed" and vertex == count - 1)
    ):
        raise ValueError("one canonical finite original pending vertex required")
    return source["mvt_xy"][vertex], xy[vertex]


def bind_pending(rows, recipe, sources, profiles):
    declarations = recipe.get("pending_junctions", [])
    if not isinstance(declarations, list) or len(declarations) > 4:
        raise ValueError("at most four original pending junctions required")
    if not declarations:
        return
    if (
        recipe.get("contact_mode") != "seed"
        or recipe.get("junction_mode") != "level_box"
    ):
        raise ValueError("pending junctions belong to independent level-box seeds")
    owned, seen = {row["source_id"]: row for row in rows}, set()
    for declaration in declarations:
        if not isinstance(declaration, dict) or set(declaration) != {
            "source_id",
            "vertex",
            "neighbor_source_id",
            "neighbor_vertex",
        }:
            raise ValueError("complete original pending source/node binding required")
        key, other = declaration["source_id"], declaration["neighbor_source_id"]
        if key not in owned or other not in sources or other in owned:
            raise ValueError("pending neighbour must remain an original unbuilt source")
        vertex, other_vertex = declaration["vertex"], declaration["neighbor_vertex"]
        node, point = original_node(sources[key], vertex)
        other_node, other_point = original_node(sources[other], other_vertex)
        token = (key, tuple(node))
        if (
            token in seen
            or node != other_node
            or not np.array_equal(point, other_point)
        ):
            raise ValueError(
                "distinct pending nodes need the same original node and frame"
            )
        seen.add(token)
        own_profile = ordinary_profile(sources[key], profiles)
        other_profile = ordinary_profile(sources[other], profiles)
        row = owned[key]
        if row["width_m"] != own_profile["widthM"]:
            raise ValueError("pending junction source width differs from its profile")
        target = float(row["prior"][vertex])
        height = float(np.float32(target))
        if not np.isfinite(height) or abs(height - target) > 0.005:
            raise ValueError("pending level quantization exceeds its 5 mm budget")
        reach = max(own_profile["widthM"], other_profile["widthM"]) / 2 + 0.75
        row.setdefault("pending_junctions", []).append(
            {
                **declaration,
                "mvt_xy": node,
                "height_m": height,
                "bounds_xy_m": [*(point - reach), *(point + reach)],
            }
        )
