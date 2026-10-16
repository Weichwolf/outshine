"""Compose shared upper door cells, shop fronts and all remaining original facade repeats."""

import numpy as np
from balcony_geometry import products as balcony_products
from facade_balconies import demand as balcony_demand
from facade_entry_clip import remainder
from facade_entry_geometry import basis_for, local_quads
from facade_entry_geometry import products as entrance_products
from outshine_ref.buildings.facade_cells import cell_complement
from outshine_ref.buildings.facades import FacadeRepeat
from tile_facade_recess import demand as recess_demand
from tile_facade_recess import selection, window_parts
from tile_shopfronts import demand as shop_demand
from tile_shopfronts import shop_cells


def retained(frame, entry, excluded, material):
    repeat = FacadeRepeat(
        frame["width_m"],
        frame["height_m"],
        frame["columns"],
        frame["rows"],
        *frame["fractions"],
    )
    width, height = entry["cell_width_m"], entry["floor_m"]
    rectangles = [
        (x0 * width, y0 * height, x1 * width, y1 * height, 0.0)
        for x0, y0, x1, y1 in cell_complement(repeat, tuple(sorted(excluded)))
    ]
    return local_quads(
        rectangles,
        basis_for(frame),
        np.r_[frame["a"], 0.0],
        [width, height],
        [frame["columns"], frame["rows"], 0.0],
        material,
    )


def wall_parts(
    mesh,
    source,
    parameters,
    view,
    plan,
    visible,
    shop,
    shop_mode,
    glass,
    wall,
    plain,
    remaining,
):
    frame, entry = plan["frame"], plan["entry"]
    triangles = mesh["vertices"][mesh["faces"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    selected, _, rectangular = selection(
        triangles, normals, frame, parameters["minHeight"]
    )
    cells = tuple((c["column"], c["row"]) for c, _ in visible)
    columns = tuple(c["column"] for c in shop["cells"]) if shop_mode else ()
    reserved = sum(mode == "through" for _, mode in visible) + (
        len(columns) if shop_mode == "recess" else 0
    )
    if reserved > remaining:
        raise ValueError("Balcony cells exceed remaining facade-detail capacity")
    parts, records, count = [], [], 0
    request = recess_demand(frame, parameters["minHeight"], view)
    if rectangular and request is not None:
        try:
            parts, count = window_parts(
                frame,
                source,
                parameters["minHeight"],
                remaining - reserved,
                glass,
                wall,
                plain,
                entry,
                columns,
                cells,
            )
        except ValueError:
            parts = []
    if not parts:
        if rectangular:
            raw = retained(
                frame,
                entry,
                {(entry["column"], 0)} | {(c, 0) for c in columns} | set(cells),
                wall,
            )
        else:
            raw = remainder(
                mesh, selected, entry, excluded_columns=columns, excluded_cells=cells
            )[0]
        parts.append(raw)
    if count:
        records.append(
            {
                "id": frame["id"],
                "cells": count,
                "depth_m": 0.18,
                "projected_recess_error_px": request[2],
                "triangles": sum(len(p["faces"]) for p in parts),
                "scope": "Same repeat retaining every non-overridden cell",
            }
        )
    return selected, parts, records


def append_products(
    parts, records, plan, visible, view, shop, shop_mode, shop_material, plain, glass
):
    from facade_entrances import demand as entrance_demand

    frame, entry = plan["frame"], plan["entry"]
    mode = entrance_demand(entry, view) or "compact"
    added = entrance_products(entry, plain, mode)
    parts.extend(added)
    records.append(
        {
            "id": entry["id"],
            "kind": "entrance",
            "facade_id": frame["id"],
            "cell": [entry["column"], 0],
            "mode": mode,
            "cells": 0,
            "angle_deg": entry["angle_deg"],
            "triangles": sum(len(p["faces"]) for p in added),
            "scope": "Unchanged shared ground-floor entry below balcony cells",
        }
    )
    if shop_mode:
        added, descriptions = shop_cells(shop, shop_mode, shop_material, plain, glass)
        parts.extend(added)
        records.extend(descriptions)
    for current, mode in visible:
        added = balcony_products(current, plan["owner"], plain, mode)
        parts.extend(added)
        records.append(
            {
                "id": current["id"],
                "kind": "balcony",
                "facade_id": frame["id"],
                "cell": [current["column"], current["row"]],
                "door_id": current["door_id"],
                "mode": mode,
                "cells": int(mode == "through"),
                "angle_deg": current["doorway"]["angle_deg"],
                "triangles": sum(len(p["faces"]) for p in added),
                "scope": "Same local deck/guard and upper door; full storey route and native interaction unproved",
            }
        )


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
    balconies,
):
    from tile_facade_recess import assemble
    from tile_facade_recess import attach as original

    visible = [
        (c, mode)
        for c in balconies["cells"]
        if (mode := balcony_demand(c, view)) is not None
    ]
    arguments = {
        "plain_wall_material": plain_wall_material,
        "max_cells": max_cells,
        "entrance": entrance,
        "shopfront": shopfront,
        "shop_material": shop_material,
    }
    if not visible:
        return original(
            mesh,
            source,
            parameters,
            facades,
            view,
            glass_material,
            wall_material,
            **arguments,
        )
    frame = balconies["frame"]
    other = [f for f in facades if f["id"] != frame["id"]]
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
    shop_mode = shop_demand(shopfront, view) if shopfront else None
    try:
        selected, parts, descriptions = wall_parts(
            mesh,
            source,
            parameters,
            view,
            balconies,
            visible,
            shopfront,
            shop_mode,
            glass_material,
            wall_material,
            plain_wall_material,
            remaining,
        )
        append_products(
            parts,
            descriptions,
            balconies,
            visible,
            view,
            shopfront,
            shop_mode,
            shop_material,
            plain_wall_material,
            glass_material,
        )
    except ValueError as error:
        arguments["max_cells"] = remaining
        mesh, old, failures = original(
            mesh,
            source,
            parameters,
            [frame],
            view,
            glass_material,
            wall_material,
            **arguments,
        )
        return (
            mesh,
            records + old,
            rejected + failures + [{"id": frame["id"], "reason": str(error)}],
        )
    return assemble(mesh, selected, parts), records + descriptions, rejected
