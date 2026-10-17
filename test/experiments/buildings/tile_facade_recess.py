"""Near recesses on the existing rectangular facade frames; far products stay compact."""

import numpy as np
from demand import clip
from facade_geometry import recess_fits
from facade_plan import Repeat
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface


def recess_error(points, normal, view, depth=0.18):
    coordinates = (points - view.eye) @ view.basis.T
    displacement = (-depth * normal) @ view.basis.T
    nearest = float(coordinates[:, 2].min()) - view.movement_m
    if nearest <= 0 or nearest + displacement[2] <= 0:
        return float("inf")
    numerator = (
        displacement[:2] * coordinates[:, 2:3] - coordinates[:, :2] * displacement[2]
    )
    maximum = np.linalg.norm(
        numerator, axis=1
    ).max() + view.movement_m * np.linalg.norm(displacement)
    return float(view.focal_px * maximum / (nearest * (nearest + displacement[2])))


def demand(frame, bottom, view, depth=0.18):
    a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
    tangent = (b - a) / frame["width_m"]
    normal = np.array([tangent[1], -tangent[0], 0.0])
    centre = np.r_[(a + b) / 2, bottom + frame["height_m"] / 2]
    if (view.eye - centre) @ normal + view.movement_m <= 0:
        return None
    points = np.array(
        [
            [*a, bottom],
            [*b, bottom],
            [*b, bottom + frame["height_m"]],
            [*a, bottom + frame["height_m"]],
        ]
    )
    visible = clip(points, view)
    if len(visible) < 3:
        return None
    error = recess_error(visible, normal, view, depth)
    return (normal, tangent, float(error)) if error > view.detail_error_px else None


def selection(triangles, normals, frame, bottom):
    from facade_entry_geometry import basis_for

    basis = basis_for(frame)
    relative = triangles - np.r_[frame["a"], bottom]
    u, w = relative @ basis[0], relative @ basis[2]
    selected = (
        (abs(w).max(axis=1) < 1e-7)
        & (u.min(axis=1) >= -1e-7)
        & (u.max(axis=1) <= frame["width_m"] + 1e-7)
        & (normals @ basis[2] > 0.999999)
    )
    if "architecture_scope" in frame:
        if bottom != frame["base_m"]:
            raise ValueError("A scoped facade requires its actual vertical origin")
        selected &= (triangles[:, :, 2].min(axis=1) >= bottom - 1e-7)
        selected &= (triangles[:, :, 2].max(axis=1) <= frame["top_m"] + 1e-7)
    local = np.stack((u[selected], relative[selected, :, 2]), axis=2)
    area = (
        np.cross(
            np.c_[local[:, 1] - local[:, 0], np.zeros(len(local))],
            np.c_[local[:, 2] - local[:, 0], np.zeros(len(local))],
        )[:, 2].sum()
        / 2
    )
    return selected, basis, abs(area - frame["width_m"] * frame["height_m"]) <= 1e-7


def window_parts(
    frame,
    source,
    bottom,
    remaining,
    glass_material,
    wall_material,
    plain_wall_material,
    entry,
    excluded_columns=(),
    excluded_cells=(),
):
    from facade_entry_geometry import basis_for

    repeat = FacadeRepeat(
        frame["width_m"],
        frame["height_m"],
        frame["columns"],
        frame["rows"],
        *frame["fractions"],
    )
    count = (
        repeat.opening_count
        - int(entry is not None)
        - len(excluded_columns)
        - len(excluded_cells)
    )
    if count > remaining:
        raise ValueError("cell capacity or non-rectangular facade")
    basis = basis_for(frame)
    fitting = {
        "a": np.asarray(frame["a"]),
        "b": np.asarray(frame["b"]),
        "normal": basis[2],
        "repeat": Repeat(
            repeat.width_m,
            repeat.height_m,
            repeat.columns,
            repeat.rows,
            repeat.opening_width,
            repeat.opening_height,
            0.18,
        ),
    }
    if not recess_fits(
        source["polygon"], fitting, (0, 0, repeat.width_m, repeat.height_m)
    ):
        raise ValueError("recess crosses source footprint")
    plan = RecessPlan(repeat)
    bounds = [None]
    if entry:
        c = entry["column"]
        bounds = [
            (0, 0, c, 1),
            (c + 1, 0, repeat.columns, 1),
            (0, 1, repeat.columns, repeat.rows),
        ]
    if excluded_columns:
        from tile_shopfronts import retained_ranges

        excluded = set(excluded_columns) | ({entry["column"]} if entry else set())
        bounds = retained_ranges(frame, excluded)
    if excluded_cells:
        from outshine_ref.buildings.facade_cells import cell_complement

        excluded = {(c, 0) for c in excluded_columns} | set(excluded_cells)
        if entry:
            excluded.add((entry["column"], 0))
        bounds = cell_complement(repeat, tuple(sorted(excluded)))
    output = []
    for bound in bounds:
        if plan.count(bound) == 0:
            continue
        product = recess_surface(plan, bound, max_cells=remaining)
        materials = np.array([wall_material, glass_material(), 10], dtype=np.uint16)[
            product["roles"]
        ]
        returns = (product["roles"] == 0) & (
            product["normals"][product["faces"][:, 0], 2] == 0
        )
        materials[returns] = plain_wall_material
        points = product["vertices"] @ basis + np.r_[frame["a"], bottom]
        output.append(
            {
                "vertices": points,
                "normals": product["normals"] @ basis,
                "uv": product["uv"],
                "facade_extent": np.tile(
                    [repeat.columns, repeat.rows, 0.0], (len(points), 1)
                ),
                "materials": materials,
                "faces": product["faces"],
            }
        )
    return output, count


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
    max_cells=4096,
    entrance=None,
    shopfront=None,
    shop_material=None,
    balconies=None,
    bays=None,
):
    if balconies is not None and bays is not None:
        raise ValueError("One upper projection programme per facade required")
    if balconies is not None or bays is not None:
        from tile_upper_cells import attach as upper_cells

        return upper_cells(
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
            shopfront=shopfront,
            shop_material=shop_material,
            projections=bays if bays is not None else balconies,
        )
    if shopfront is not None:
        from tile_shopfronts import attach as shopfronts

        return shopfronts(
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
            shopfront=shopfront,
            shop_material=shop_material,
        )
    return attach_recesses(
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


def attach_recesses(
    mesh,
    source,
    parameters,
    facades,
    view,
    glass_material,
    wall_material,
    *,
    plain_wall_material,
    max_cells=4096,
    entrance=None,
):
    from facade_entrances import demand as entry_demand
    from facade_entry_geometry import flat_remainder, products

    records, rejected, additions = [], [], []
    mode = entry_demand(entrance, view) if entrance else None
    requests = [
        (
            f,
            demand(f, parameters["minHeight"], view),
            entrance if mode and entrance["frame"]["id"] == f["id"] else None,
        )
        for f in facades
    ]
    requests = [(f, r, e) for f, r, e in requests if r is not None or e is not None]
    if not requests:
        return mesh, records, rejected
    remove = np.zeros(len(mesh["faces"]), dtype=bool)
    triangles = mesh["vertices"][mesh["faces"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    bottom, remaining = parameters["minHeight"], max_cells
    for frame, request, entry in requests:
        selected, _, rectangular = selection(triangles, normals, frame, bottom)
        if not rectangular and not entry:
            rejected.append(
                {"id": frame["id"], "reason": "cell capacity or non-rectangular facade"}
            )
            continue
        parts, count = [], 0
        dropped = 0.0
        if not rectangular:
            from facade_entry_clip import remainder

            try:
                retained, dropped = remainder(mesh, selected, entry)
                parts.append(retained)
            except ValueError as error:
                rejected.append({"id": frame["id"], "reason": str(error)})
                continue
        elif request is not None:
            try:
                parts, count = window_parts(
                    frame,
                    source,
                    bottom,
                    remaining,
                    glass_material,
                    wall_material,
                    plain_wall_material,
                    entry,
                )
            except ValueError as error:
                rejected.append({"id": frame["id"], "reason": str(error)})
                if not entry:
                    continue
        window_triangles = sum(len(p["faces"]) for p in parts)
        if entry:
            if not parts:
                parts.append(flat_remainder(entry, wall_material))
            added = products(entry, plain_wall_material, mode)
            parts.extend(added)
            records.append(
                {
                    "id": entry["id"],
                    "kind": "entrance",
                    "facade_id": frame["id"],
                    "cell": [entry["column"], 0],
                    "mode": mode,
                    "remainder": "repeat-rectangle"
                    if rectangular
                    else "preserved-source",
                    "dropped_area_m2": dropped,
                    "cells": 0,
                    "angle_deg": entry["angle_deg"],
                    "triangles": sum(len(p["faces"]) for p in added),
                    "scope": "Shared entrance/leaf and provisional first-floor arrival; upper routes and native controller unproved",
                }
            )
        if np.any(remove & selected):
            raise ValueError("Near facade has multiple source owners")
        remove |= selected
        additions.extend(parts)
        remaining -= count
        if count:
            records.append(
                {
                    "id": frame["id"],
                    "cells": count,
                    "depth_m": 0.18,
                    "projected_recess_error_px": request[2],
                    "triangles": window_triangles,
                    "scope": "Inward render recess from the same inferred facade repeat; entrance cell excluded when present",
                }
            )
    if not additions:
        return mesh, records, rejected
    return assemble(mesh, remove, additions), records, rejected


def assemble(mesh, remove, additions):
    keep = mesh["faces"][~remove]
    chunks = [
        {
            key: mesh[key][keep].reshape(-1, mesh[key].shape[1])
            for key in ("vertices", "normals", "uv", "facade_extent")
        }
        | {
            "materials": mesh["materials"][~remove],
            "faces": np.arange(keep.size, dtype=np.uint32).reshape(-1, 3),
        }
    ] + additions
    offsets = np.cumsum([0] + [len(c["vertices"]) for c in chunks[:-1]])
    result = {
        key: np.concatenate([c[key] for c in chunks])
        for key in ("vertices", "normals", "uv", "facade_extent", "materials")
    }
    result["faces"] = np.concatenate(
        [c["faces"] + offset for c, offset in zip(chunks, offsets, strict=True)]
    ).astype(np.uint32)
    return result
