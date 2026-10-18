"""Every remaining source polygon is either built or explicitly deferred once."""

import shapely


def verify_parts(remaining, result, source_part_id):
    actual = shapely.from_wkb(result["adjusted_footprint_wkb"])
    if not actual.equals_exact(remaining, 0):
        raise ValueError("saved whole adjusted source footprint changed")
    expected = {
        i: p for i, p in enumerate(shapely.get_parts(remaining)) if not p.is_empty
    }
    seen = set()
    for kind, parts in (
        ("built", result["products"]),
        ("unbuilt", result.get("unbuilt_parts", [])),
    ):
        for part in parts:
            index = part["output_part"]
            if type(index) is not int or index not in expected or index in seen:
                raise ValueError(
                    "remaining building part omitted, duplicated or invented"
                )
            polygon = shapely.from_wkb(part["footprint_wkb"])
            if not polygon.equals_exact(expected[index], 0):
                raise ValueError("saved remaining building part geometry changed")
            if kind == "unbuilt" and (
                part.get("source_part_id") != source_part_id
                or not isinstance(part.get("reason"), str)
                or not part["reason"]
            ):
                raise ValueError(
                    "unbuilt part requires original identity and explicit reason"
                )
            seen.add(index)
    if seen != set(expected):
        raise ValueError("remaining building part omitted, duplicated or invented")
