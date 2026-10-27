"""Provider endpoint candidates bound to explicit chosen floor products (#1553)."""

from fractions import Fraction


def source_nodes(request, receipts):
    original = request.original
    zoom, x, y = receipts[original["source_sha256"]]["tile"]
    extent = original["raw_tile_extent"]
    points = original["raw_tile_coordinates"]
    if (
        type(extent) is not int
        or extent <= 0
        or any(type(c) is not int for p in points for c in p)
    ):
        raise ValueError("Explicit integer MVT grid coordinates required")
    return tuple(
        (
            Fraction(x * extent + p[0], extent * (1 << zoom)),
            Fraction(y * extent + p[1], extent * (1 << zoom)),
        )
        for p in points
    )


def bind_stairs(inputs, floors):
    receipts = {r["sha256"]: r for r in inputs.metadata["source_receipts"]}
    choices = {r["source_id"]: r for r in floors["choices"]}
    nodes = {}
    for request in inputs.constraints:
        if request.purpose != "flat_indoor_walkway":
            continue
        for node in source_nodes(request, receipts):
            nodes.setdefault(node, set()).add(request.identifier)
    result = []
    for request in inputs.constraints:
        if request.purpose != "unplanned_indoor_vertical_connector":
            continue
        endpoints = source_nodes(request, receipts)
        ends = []
        for node in (endpoints[0], endpoints[-1]):
            neighbours = sorted(nodes.get(node, ()))
            candidates = [
                choices[n]
                for n in neighbours
                if n in choices and choices[n]["status"] == "ready"
            ]
            profiles = {(r["floor_id"], float(r["xyz"][0, 2])) for r in candidates}
            if len(profiles) == 1:
                floor_id, z = profiles.pop()
                ends.append(
                    {
                        "floor_id": floor_id,
                        "z_m": z,
                        "neighbour_source_ids": [r["source_id"] for r in candidates],
                    }
                )
            else:
                ends.append(
                    {
                        "reason": "ambiguous_chosen_stair_endpoint"
                        if profiles
                        else "missing_chosen_stair_endpoint",
                        "neighbour_source_ids": neighbours,
                    }
                )
        if any("reason" in end for end in ends):
            reason = "stair_end_profiles_not_bound"
        elif ends[0]["z_m"] == ends[1]["z_m"]:
            reason = "chosen_stair_end_heights_equal"
        else:
            reason = ""
        result.append(
            {
                "source_id": request.identifier,
                "original": request.original,
                "frame": request.frame.identifier,
                "request": request,
                "ends": ends,
                "reason": reason,
                "qualification": "Exact normalized raw-grid contact candidates, not original OSM node or native World topology.",
            }
        )
    return result
