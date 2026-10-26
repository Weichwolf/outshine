"""Use existing repeat/recess products for bounded display fronts and retain all other cells."""

import numpy as np
from demand import clip
from facade_entry_geometry import basis_for, local_quads, products
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface


def retained_ranges(frame, excluded):
    ranges = []
    start = 0
    for column in sorted(excluded):
        if column > start:
            ranges.append((start, 0, column, 1))
        start = column + 1
    if start < frame["columns"]:
        ranges.append((start, 0, frame["columns"], 1))
    if frame["rows"] > 1:
        ranges.append((0, 1, frame["columns"], frame["rows"]))
    return ranges


def demand(plan, view):
    frame, entry = plan["frame"], plan["entry"]
    basis = basis_for(frame)
    width, height = entry["cell_width_m"], entry["floor_m"]
    a = np.r_[frame["a"], 0.0]
    low, high = (
        min(c["column"] for c in plan["cells"]),
        max(c["column"] for c in plan["cells"]) + 1,
    )
    points = np.array(
        [
            a + basis[0] * u + basis[1] * z
            for u, z in (
                (low * width, 0),
                (high * width, 0),
                (high * width, height),
                (low * width, height),
            )
        ]
    )
    if (view.eye - points.mean(axis=0)) @ basis[2] + view.movement_m <= 0:
        return None
    visible = clip(points, view)
    if len(visible) < 3:
        return None
    nearest = float(((visible - view.eye) @ view.basis.T)[:, 2].min()) - view.movement_m
    if nearest <= 0 or view.focal_px * height / nearest >= 0.5:
        from tile_facade_recess import recess_error

        return (
            "recess"
            if recess_error(visible, basis[2], view) > view.detail_error_px
            else "compact"
        )
    return None


def retained_flat(frame, entry, columns, material):
    width, height = entry["cell_width_m"], entry["floor_m"]
    rectangles = [
        (x0 * width, y0 * height, x1 * width, y1 * height, 0.0)
        for x0, y0, x1, y1 in retained_ranges(frame, set(columns) | {entry["column"]})
    ]
    return local_quads(
        rectangles,
        basis_for(frame),
        np.r_[frame["a"], 0.0],
        [width, height],
        [frame["columns"], frame["rows"], 0.0],
        material,
    )


def shop_cells(plan, mode, material, plain_wall_material, glass_material):
    frame, entry = plan["frame"], plan["entry"]
    basis, origin = basis_for(frame), np.r_[frame["a"], 0.0]
    width, height = entry["cell_width_m"], entry["floor_m"]
    output, records = [], []
    for cell in plan["cells"]:
        c = cell["column"]
        identifier = material(cell["fractions"])
        if mode == "compact":
            raw = local_quads(
                [(c * width, 0, (c + 1) * width, height, 0.0)],
                basis,
                origin,
                [width, height],
                [frame["columns"], frame["rows"], 0.0],
                identifier,
            )
        else:
            repeat = FacadeRepeat(
                frame["width_m"],
                frame["height_m"],
                frame["columns"],
                frame["rows"],
                *cell["fractions"],
            )
            surface = recess_surface(RecessPlan(repeat), (c, 0, c + 1, 1), max_cells=1)
            materials = np.array([identifier, glass_material(), 10], dtype=np.uint16)[
                surface["roles"]
            ]
            returns = (surface["roles"] == 0) & (
                surface["normals"][surface["faces"][:, 0], 2] == 0
            )
            materials[returns] = plain_wall_material
            raw = {
                "vertices": surface["vertices"] @ basis + origin,
                "faces": surface["faces"],
                "normals": surface["normals"] @ basis,
                "uv": surface["uv"],
                "facade_extent": np.tile(
                    [frame["columns"], frame["rows"], 0.0],
                    (len(surface["vertices"]), 1),
                ),
                "materials": materials,
            }
        output.append(raw)
        records.append(
            {
                "id": cell["id"],
                "kind": "shopfront",
                "facade_id": frame["id"],
                "cell": [c, 0],
                "mode": mode,
                "fractions": cell["fractions"],
                "cells": int(mode == "recess"),
                "triangles": len(raw["faces"]),
                "depth_m": 0.18 if mode == "recess" else 0.0,
            }
        )
    return output, records


def target_parts(
    mesh,
    source,
    parameters,
    view,
    plan,
    mode,
    glass_material,
    wall_material,
    plain_wall_material,
    shop_material,
    remaining,
):
    from facade_entrances import demand as entry_demand
    from facade_entry_clip import remainder
    from tile_facade_recess import demand as recess_demand
    from tile_facade_recess import selection, window_parts

    frame, entry = plan["frame"], plan["entry"]
    triangles = mesh["vertices"][mesh["faces"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    selected, _, rectangular = selection(
        triangles, normals, frame, parameters["minHeight"]
    )
    columns = tuple(c["column"] for c in plan["cells"])
    reserved = len(columns) if mode == "recess" else 0
    if reserved > remaining:
        raise ValueError("Shopfront exceeds remaining recess cell capacity")
    parts, records, count = [], [], 0
    request = recess_demand(frame, parameters["minHeight"], view)
    if rectangular and request is not None:
        try:
            parts, count = window_parts(
                frame,
                source,
                parameters["minHeight"],
                remaining - reserved,
                glass_material,
                wall_material,
                plain_wall_material,
                entry,
                columns,
            )
        except ValueError:
            parts = []
    if not parts:
        retained = (
            retained_flat(frame, entry, columns, wall_material)
            if rectangular
            else remainder(mesh, selected, entry, excluded_columns=columns)[0]
        )
        parts.append(retained)
    if count:
        records.append(
            {
                "id": frame["id"],
                "cells": count,
                "depth_m": 0.18,
                "projected_recess_error_px": request[2],
                "triangles": sum(len(p["faces"]) for p in parts),
                "scope": "Same repeat excluding entrance and shopfront ground cells",
            }
        )
    entry_mode = entry_demand(entry, view) or "compact"
    doorway = products(entry, plain_wall_material, entry_mode)
    parts.extend(doorway)
    records.append(
        {
            "id": entry["id"],
            "kind": "entrance",
            "facade_id": frame["id"],
            "cell": [entry["column"], 0],
            "mode": entry_mode,
            "cells": 0,
            "angle_deg": entry["angle_deg"],
            "triangles": sum(len(p["faces"]) for p in doorway),
            "scope": "Same existing entrance beside display fronts",
        }
    )
    shops, descriptions = shop_cells(
        plan, mode, shop_material, plain_wall_material, glass_material
    )
    return selected, parts + shops, records + descriptions


def attach(
    mesh,
    source,
    parameters,
    facades,
    view,
    glass_material,
    wall_material,
    *,
    plain_wall_material,
    max_cells,
    entrance,
    shopfront,
    shop_material,
):
    from tile_facade_recess import assemble
    from tile_facade_recess import attach as original

    mode = demand(shopfront, view)
    if mode is None:
        return original(
            mesh,
            source,
            parameters,
            facades,
            view,
            glass_material,
            wall_material,
            plain_wall_material=plain_wall_material,
            max_cells=max_cells,
            entrance=entrance,
        )
    other = [f for f in facades if f["id"] != shopfront["frame"]["id"]]
    mesh, records, rejected = original(
        mesh,
        source,
        parameters,
        other,
        view,
        glass_material,
        wall_material,
        plain_wall_material=plain_wall_material,
        max_cells=max_cells,
        entrance=entrance,
    )
    remaining = max_cells - sum(r["cells"] for r in records)
    try:
        selected, parts, descriptions = target_parts(
            mesh,
            source,
            parameters,
            view,
            shopfront,
            mode,
            glass_material,
            wall_material,
            plain_wall_material,
            shop_material,
            remaining,
        )
    except ValueError as error:
        mesh, old, failures = original(
            mesh,
            source,
            parameters,
            [shopfront["frame"]],
            view,
            glass_material,
            wall_material,
            plain_wall_material=plain_wall_material,
            max_cells=remaining,
            entrance=entrance,
        )
        return (
            mesh,
            records + old,
            rejected
            + failures
            + [{"id": shopfront["frame"]["id"], "reason": str(error)}],
        )
    return assemble(mesh, selected, parts), records + descriptions, rejected
