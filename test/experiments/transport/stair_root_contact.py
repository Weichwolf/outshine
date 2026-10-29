"""Derive new through-root eligibility from the original bound source occurrences."""

import numpy as np


def new_through_root(recipe, sources):
    port = recipe.get("port", {})
    if recipe.get("contact_mode") != "profile" or "end" not in port:
        return False
    if set(port) != {"source_id", "end"} or port["end"] not in (0, -1):
        raise ValueError("explicit original outer endpoint required")
    old = sources[port["source_id"]]
    anchor = old["mvt_xy"][port["end"]]
    through = False
    for key in recipe["source_ids"]:
        source = sources[key]
        found = [i for i, node in enumerate(source["mvt_xy"]) if node == anchor]
        if not any(0 < i < len(source["mvt_xy"]) - 1 for i in found):
            continue
        if (
            key == port["source_id"]
            or len(found) != 1
            or sum(node == anchor for node in old["mvt_xy"]) != 1
            or np.array_equal(old["xy_m"][0], old["xy_m"][-1])
        ):
            raise ValueError(
                "unique original old end and new through occurrence required"
            )
        if not np.array_equal(source["xy_m"][found[0]], old["xy_m"][port["end"]]):
            raise ValueError(
                "original through root belongs to a different source frame"
            )
        through = True
    return through


def profile_interiors(recipe, sources):
    through = new_through_root(recipe, sources)
    return (
        recipe.get("contact_mode") == "seed"
        or "vertex" in recipe.get("port", {})
        or bool(recipe.get("endpoint_contacts"))
        or through
    )
