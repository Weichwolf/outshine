"""Explicit bounded original cycle bridge/path instances beyond the ground-only case."""

import numpy as np


def shared_cycle_path(props):
    return (
        props.get("subclass") == "path"
        and props.get("bicycle") == "designated"
        and props.get("foot") == "designated"
    )


def branch_rows(recipe, available, existing, profiles, sample):
    declarations = recipe.get("exterior_source_extensions", [])
    if not isinstance(declarations, list) or len(declarations) > 8:
        raise ValueError("at most eight explicit exterior source extensions required")
    result, seen = [], set(existing)
    for item in declarations:
        key, role = item["source_id"], item["recipe"]
        if not isinstance(key, str) or key in seen or key not in available:
            raise ValueError("unique additional original source identity required")
        if role not in (
            "exterior_cycle_bridge",
            "exterior_cycle_path",
            "exterior_shared_cycle_bridge",
            "exterior_shared_cycle_path",
        ):
            raise ValueError("known explicit cycle bridge/path recipe required")
        source = available[key]
        props = source["properties"]
        bridge = role.endswith("_bridge")
        kind_matches = (
            shared_cycle_path(props)
            if role.startswith("exterior_shared_")
            else props.get("subclass") == "cycleway"
        )
        if (
            not kind_matches
            or props.get("indoor")
            or (bridge and props.get("brunnel") != "bridge")
            or (not bridge and props.get("brunnel"))
            or props.get("level", 0) not in ((0, 1) if bridge else (0,))
            or props.get("layer", 0) not in ((0, 1) if bridge else (0,))
        ):
            raise ValueError("original source does not match its exterior recipe")
        profile = profiles["cycleway"]
        xy = np.array(source["xy_m"])
        result.append(
            {
                "source_id": key,
                "xy": xy,
                "prior": sample(xy),
                "width_m": profile["widthM"],
                "gradient": profile["maxGradient"],
                "source_recipe": role,
                "original": {
                    "bridge": bridge,
                    "source": {"feature": source["original_feature"]},
                },
            }
        )
        seen.add(key)
    if len(seen) > 32:
        raise ValueError("bounded whole exterior component source count")
    return result


def surface_sampling(rows, mode):
    if mode not in ("end_neighbourhood", "source_knots"):
        raise ValueError("known explicit surface cross section mode required")
    if mode == "end_neighbourhood":
        for row in rows:
            row.pop("surface_cross_section_mode", None)
        return
    for row in rows:
        props = row["original"]["source"]["feature"]["properties"]
        if props.get("subclass") != "steps":
            row["surface_cross_section_mode"] = mode


def surface_welding(rows, tolerance):
    if tolerance is not None and (
        isinstance(tolerance, bool)
        or not isinstance(tolerance, (int, float))
        or not np.isfinite(tolerance)
        or not 1e-8 <= tolerance <= 1e-7
    ):
        raise ValueError(
            "explicit numeric surface weld bound in [1e-8, 1e-7] metres required"
        )
    for row in rows:
        if tolerance is None:
            row.pop("surface_weld_tolerance_m", None)
        else:
            row["surface_weld_tolerance_m"] = float(tolerance)
