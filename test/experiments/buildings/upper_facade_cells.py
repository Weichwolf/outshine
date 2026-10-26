"""One bounded placement pass shared by balconies and enclosed window bays."""

import numpy as np
from outshine_ref.buildings.interiors.geometry import rectangle


def propose(
    row,
    entry,
    family,
    factory,
    states,
    entrances,
    rejected,
    kind,
    *,
    max_levels=3,
    max_stacks=2,
):
    if (
        type(max_levels) is not int
        or not 1 <= max_levels <= 11
        or type(max_stacks) is not int
        or not 1 <= max_stacks <= 2
    ):
        raise ValueError(
            "Upper facade placement requires bounded integer levels/stacks"
        )
    if family != "residential" or entry is None or row.get("clipped"):
        return []
    frame = entry["frame"]
    if (
        frame["rows"] < 2
        or entry["cell_width_m"] < 2.0
        or not 60 <= row["polygon"].area <= 1600
    ):
        return []
    columns = (
        sorted({round(t * (frame["columns"] - 1)) for t in (0.25, 0.75)})
        if frame["columns"] >= 6
        else [entry["column"]]
    )
    tangent = np.asarray(entry["door"]["tangent"])
    planned = []
    for column in columns[:max_stacks]:
        for level in range(1, min(frame["rows"], max_levels + 1)):
            try:
                current = factory(entry, column, level, states)
                low, high = current["height_interval_m"]
                if current["footprint"].intersection(row["polygon"]).area > 1e-8:
                    raise ValueError(f"{kind} intersects another wing of its source")
                centre = (
                    np.asarray(frame["a"])
                    + (column + 0.5) * entry["cell_width_m"] * tangent
                )
                wall = rectangle(centre, tangent, entry["cell_width_m"], 0.0, 0.24)
                if (
                    wall.difference(row["polygon"]).area > 1e-8
                    or current["pocket"].difference(row["polygon"]).area > 1e-8
                    or not row["polygon"]
                    .buffer(-0.24, join_style=2)
                    .covers(current["clear_pocket"])
                ):
                    raise ValueError(
                        f"{kind} wall cell or arrival pocket leaves its source"
                    )
                # All current projections are at most1.2m deep, including opposing cells.
                envelope = current["footprint"].buffer(1.2)
                if not entrances.clear(
                    row, envelope, high, low_m=low
                ) or not entrances.clear(
                    row, current["pocket"], current["floor_z_m"] + 2.15, low_m=low
                ):
                    raise ValueError(
                        f"{kind} conflicts with a known height interval or opposing-deck margin"
                    )
                planned.append(current)
            except ValueError as error:
                rejected.append(
                    {
                        "source_id": row["id"],
                        "cell": [column, level],
                        "reason": str(error),
                    }
                )
    return planned
