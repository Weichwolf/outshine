"""Validate normal/projected whole-cell observations, without inventing face IDs."""


def validate_row(row, batch, source_count):
    capture = batch["capture"]
    if (
        not row["accepted"]
        or row["unsupported"]
        or len(row["details"]) != source_count
        or row["footprints"] != source_count
        or any(d not in (0, 1, 2) for d in row["details"])
        or len(row["mesh_calls"]) + len(row["projection_calls"]) != len(batch["calls"])
    ):
        raise RuntimeError("incomplete/unsupported original cell selection")
    normal, projected = [], []
    calls = {
        "Mesh": iter(row["mesh_calls"]),
        "EmitProjected": iter(row["projection_calls"]),
    }
    for ranges in batch["calls"]:
        kind = ranges["emitter"]
        if kind not in calls:
            raise RuntimeError("unknown observed emitter")
        try:
            call = next(calls[kind])
        except StopIteration as error:
            raise RuntimeError("missing independent native call") from error
        if not call["accepted"]:
            raise RuntimeError("failed native emission")
        if kind == "EmitProjected":
            if not call["members"]:
                raise RuntimeError("empty projected membership")
            for member in call["members"]:
                i = member["source"]
                if (
                    type(i) is not int
                    or not 0 <= i < source_count
                    or member["detail"] != 1
                    or member["openings"]
                    or row["details"][i] != 1
                ):
                    raise RuntimeError("unsupported projected source/detail")
                projected.append(i)
        elif call["error"] is not None:
            raise RuntimeError("failed native mesh call")
        elif call["detail"] == 2:
            if call["source"] is not None:
                raise RuntimeError("unexpected invented mass source")
        else:
            i = call["source"]
            if (
                type(i) is not int
                or not 0 <= i < source_count
                or call["detail"] != row["details"][i]
            ):
                raise RuntimeError("native source range/detail mismatch")
            normal.append(i)
        ranges["native_call"] = call
    if any(list(remaining) for remaining in calls.values()):
        raise RuntimeError("unobserved native emission")
    if sorted(normal + projected) != [
        i for i, d in enumerate(row["details"]) if d != 2
    ]:
        raise RuntimeError("missing or duplicated original source")
    if row["lumped"] != row["details"].count(2) + len(projected) or row[
        "blocks"
    ] != len(row["projection_calls"]) + sum(
        c["detail"] == 2 for c in row["mesh_calls"]
    ):
        raise RuntimeError("mass/projected accounting mismatch")
    if (
        capture["walls"]["count"] != row["wall_vertices"]
        or capture["roofs"]["count"] != row["roof_vertices"]
        or sum(s["bytes"] for s in capture.values()) != row["built_bytes"]
        or (capture["wall_indices"]["count"] + capture["roof_indices"]["count"]) // 3
        != row["fallback_triangles"]
    ):
        raise RuntimeError("native JSON does not validate cell vector boundaries")
