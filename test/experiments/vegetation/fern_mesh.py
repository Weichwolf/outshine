"""Cached canonical pinnae become complete or selected spatial fern products."""

import numpy as np
from fern_mesh_ops import STEM, combine, repeat, source
from outshine_ref.vegetation import FrondGuide, PinnatePattern
from outshine_ref.vegetation.axis_surface import axis_mesh


def pinna_mesh(
    plan, segments=6, across=1, max_triangles=100_000, *, rib_segments=8, rib_sides=6
):
    if (
        type(segments) is not int
        or not 2 <= segments <= 64
        or type(across) is not int
        or not 1 <= across <= 4
        or type(rib_segments) is not int
        or not 2 <= rib_segments <= 64
        or type(rib_sides) is not int
        or not 3 <= rib_sides <= 16
    ):
        raise ValueError("bounded integer pinna detail required")
    count = len(plan["pinnules"]) * (
        4 * across * segments - 2 * across
    ) + 2 * rib_sides * (rib_segments + 1)
    if type(max_triangles) is not int or max_triangles < 0:
        raise ValueError("nonnegative triangle budget required")
    if count > max_triangles:
        return None
    pattern = PinnatePattern.from_recipe(plan["pinna_pattern"])
    parts = []
    prototype = pattern.leaflet_prototype(
        tuple(plan["pinnules"][0]["shape"]), segments, across
    )
    for row in plan["pinnules"]:
        # Prepared shape is constant in this authored study; no hidden shape resampling.
        if tuple(row["shape"]) != tuple(plan["pinnules"][0]["shape"]):
            raise ValueError("one prepared canonical pinnule shape required")
        part = source(prototype, row["id"], 6)
        posed = repeat(
            part,
            np.asarray([row["origin"]]),
            np.asarray([row["frame"]]),
            np.asarray([row["length"]]),
            np.array([0]),
        )
        posed["organ_ids"] = posed["organ_ids"][:, 1:]
        parts.append(posed)
    parts.append(
        source(axis_mesh(1, 0, 0.006, 0.0005, rib_segments, rib_sides), STEM, 5)
    )
    return combine(parts)


def fern_mesh(
    plan,
    pinna,
    *,
    selected=None,
    method="batch",
    max_triangles=200_000,
    rib_segments=24,
):
    if plan["format"] != "nested-fern-study-1" or method not in ("batch", "individual"):
        raise ValueError("prepared fern plan and declared assembly method required")
    if type(rib_segments) is not int or not 2 <= rib_segments <= 64:
        raise ValueError("bounded rachis subdivisions required")
    known = {(r["id"], p["id"]) for r in plan["fronds"] for p in r["pinnae"]} | {
        (r["id"], -1) for r in plan["fronds"]
    }
    selected = known if selected is None else set(selected)
    if not selected <= known or type(max_triangles) is not int or max_triangles < 0:
        raise ValueError(
            "known selected frond/pinna IDs and nonnegative budget required"
        )
    count = sum(
        2 * 6 * (rib_segments + 1) if j == -1 else len(pinna["faces"])
        for _, j in selected
    )
    if not selected or count > min(max_triangles, 2_000_000):
        return None
    parts = []
    for row in plan["fronds"]:
        wanted = [p for p in row["pinnae"] if (row["id"], p["id"]) in selected]
        if not wanted and (row["id"], -1) not in selected:
            continue
        guide = FrondGuide.from_recipe(row["guide"])
        local = []
        if (row["id"], -1) in selected:
            local.append(
                source(
                    axis_mesh(guide.length, 0, 0.0023, 0.00018, rib_segments, 6),
                    STEM,
                    5,
                )
            )
            local[-1]["organ_ids"] = np.full(
                (len(local[-1]["vertices"]), 2), STEM, np.uint32
            )
        if wanted:
            poses = (
                np.asarray([p["origin"] for p in wanted]),
                np.asarray([p["frame"] for p in wanted]),
                np.asarray([p["length"] for p in wanted]),
                np.asarray([p["id"] for p in wanted], np.uint32),
            )
            if method == "batch":
                local.append(repeat(pinna, *poses))
            else:
                local.extend(
                    repeat(pinna, *[v[i : i + 1] for v in poses])
                    for i in range(len(wanted))
                )
        if not local:
            continue
        if method == "batch":
            mapped = guide.deform(combine(local))
        else:
            mapped = combine([guide.deform(part) for part in local])
        parts.append(
            repeat(
                mapped,
                np.asarray([row["origin"]]),
                np.asarray([row["frame"]]),
                np.ones(1),
                np.array([row["id"]], np.uint32),
            )
        )
    return combine(parts)
