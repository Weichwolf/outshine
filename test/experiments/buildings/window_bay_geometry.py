"""Shared wall panel, physical bay parts and a simple adjoining floor/ceiling pocket."""

import numpy as np
from building_mesh import solid
from facade_entry_geometry import basis_for, expand, local_quads
from facade_surface import project
from outshine_ref.buildings.opening_panels import OpeningPanel, panel_surface


def products(current, owner, material, mode, *, glass=None):
    frame, plan = current["frame"], current["prototype"]
    basis = basis_for(frame)
    origin = np.r_[current["centre"], current["base_m"]]
    local = np.array([basis[0], -basis[2], basis[1]])
    width = current["cell_width_m"]
    cell_origin = origin - basis[0] * width / 2
    output = []
    if mode in ("compact", "flat"):
        output.append(
            local_quads(
                [(0, 0, width, plan.height_m, 0)],
                basis,
                cell_origin,
                [1, 1],
                [0, 0, 0],
                material,
            )
        )
    elif mode == "through":
        half = plan.width_m / 2 - plan.wall_m
        opening = (
            width / 2 - half,
            plan.floor_m,
            width / 2 + half,
            plan.height_m - plan.ceiling_m,
        )
        panel = panel_surface(OpeningPanel(width, plan.height_m, opening))
        output.append(
            expand(
                {
                    "vertices": panel["vertices"] @ basis + cell_origin,
                    "faces": panel["faces"],
                    "materials": np.ones(len(panel["faces"]), dtype=np.uint8),
                },
                material,
            )
        )
    else:
        raise ValueError("Declared bay representation required")
    fixed = owner.template(plan, mode)
    part_ids = fixed.get("parts", np.zeros(len(fixed["faces"]), dtype=np.uint8))
    points = fixed["vertices"] @ local + origin
    materials = np.array([1, 0, 2, 3], dtype=np.uint8)[fixed["roles"]]
    for part in np.unique(part_ids):
        rows = part_ids == part
        raw = {
            "vertices": points,
            "faces": fixed["faces"][rows],
            "materials": materials[rows],
        }
        if glass is not None and np.any(raw["materials"] == 3):
            raw, _ = project(raw, tile_size_m=(1.0, 1.0), material=3)
        expanded = expand(raw, material)
        if glass is not None:
            expanded["materials"][expanded["materials"] == 3] = glass()
            if "uv" in raw:
                expanded["uv"] = raw["uv"][raw["faces"]].reshape(-1, 2)
        output.append(expanded)
    if mode == "through":
        pocket = current["pocket"].difference(current["full_footprint"])
        for low, high, component in (
            (0, plan.floor_m, 0),
            (plan.height_m - plan.ceiling_m, plan.height_m, 2),
        ):
            storage = [], [], [], []
            solid(
                storage,
                pocket,
                current["base_m"] + low,
                current["base_m"] + high,
                component,
            )
            output.append(
                expand(
                    {
                        "vertices": np.asarray(storage[0]),
                        "faces": np.asarray(storage[1]),
                        "materials": np.asarray(storage[2]),
                    },
                    material,
                )
            )
    return output
