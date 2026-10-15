"""Near recesses on the existing rectangular facade frames; far products stay compact."""

import numpy as np
from demand import clip
from facade_geometry import recess_fits
from facade_plan import Repeat
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface


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
    minimum = (
        ((visible - view.eye) @ view.basis.T)[:, 2].min() - view.movement_m - depth
    )
    factor = np.sqrt(
        1
        + (view.width / (2 * view.focal_px)) ** 2
        + (view.height / (2 * view.focal_px)) ** 2
    )
    error = view.focal_px * depth * factor / max(minimum, 0.1)
    return (normal, tangent, float(error)) if error > view.detail_error_px else None


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
):
    records, rejected, additions = [], [], []
    requests = [
        (frame, demand(frame, parameters["minHeight"], view)) for frame in facades
    ]
    requests = [(frame, request) for frame, request in requests if request is not None]
    if not requests:
        return mesh, records, rejected
    remove = np.zeros(len(mesh["faces"]), dtype=bool)
    triangles = mesh["vertices"][mesh["faces"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    bottom = parameters["minHeight"]
    remaining = max_cells
    for frame, request in requests:
        normal, tangent, error = request
        a = np.asarray(frame["a"])
        relative = triangles - np.r_[a, bottom]
        u = relative[:, :, :2] @ tangent
        w = relative @ normal
        selected = (
            (abs(w).max(axis=1) < 1e-7)
            & (u.min(axis=1) >= -1e-7)
            & (u.max(axis=1) <= frame["width_m"] + 1e-7)
            & (normals @ normal > 0.999999)
        )
        count = frame["columns"] * frame["rows"]
        local = np.stack((u[selected], relative[selected, :, 2]), axis=2)
        area = (
            np.cross(
                np.c_[local[:, 1] - local[:, 0], np.zeros(len(local))],
                np.c_[local[:, 2] - local[:, 0], np.zeros(len(local))],
            )[:, 2].sum()
            / 2
            if len(local)
            else 0.0
        )
        if count > remaining or abs(area - frame["width_m"] * frame["height_m"]) > 1e-7:
            rejected.append(
                {"id": frame["id"], "reason": "cell capacity or non-rectangular facade"}
            )
            continue
        repeat = FacadeRepeat(
            frame["width_m"],
            frame["height_m"],
            frame["columns"],
            frame["rows"],
            *frame["fractions"],
        )
        fitting = {
            "a": a,
            "b": np.asarray(frame["b"]),
            "normal": normal,
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
            rejected.append(
                {"id": frame["id"], "reason": "recess crosses source footprint"}
            )
            continue
        try:
            plan = RecessPlan(repeat)
        except ValueError as error:
            rejected.append({"id": frame["id"], "reason": str(error)})
            continue
        product = recess_surface(plan, max_cells=remaining)
        materials = np.array([wall_material, glass_material(), 10], dtype=np.uint16)[
            product["roles"]
        ]
        returns = (product["roles"] == 0) & (
            product["normals"][product["faces"][:, 0], 2] == 0
        )
        materials[returns] = plain_wall_material
        basis = np.array([np.r_[tangent, 0.0], [0.0, 0.0, 1.0], normal])
        points = product["vertices"] @ basis + np.r_[a, bottom]
        values = {
            "vertices": points,
            "normals": product["normals"] @ basis,
            "uv": product["uv"],
            "facade_extent": np.tile(
                [repeat.columns, repeat.rows, 0.0], (len(points), 1)
            ),
            "materials": materials,
            "faces": product["faces"],
        }
        if np.any(remove & selected):
            raise ValueError("Near facade has multiple source owners")
        remove |= selected
        additions.append(values)
        remaining -= count
        records.append(
            {
                "id": frame["id"],
                "cells": count,
                "depth_m": 0.18,
                "projected_recess_error_px": error,
                "triangles": len(product["faces"]),
                "scope": "Inward render recess from the same inferred facade repeat; no new room/door programme",
            }
        )
    if not additions:
        return mesh, records, rejected
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
        key: np.concatenate([chunk[key] for chunk in chunks])
        for key in ("vertices", "normals", "uv", "facade_extent", "materials")
    }
    result["faces"] = np.concatenate(
        [c["faces"] + offset for c, offset in zip(chunks, offsets, strict=True)]
    ).astype(np.uint32)
    return result, records, rejected
