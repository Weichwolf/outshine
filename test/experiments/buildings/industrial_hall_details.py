"""Actual through-openings, a visible roller leaf and existing personnel/window parts."""

from functools import lru_cache

import numpy as np
import shapely
from building_mesh import solid
from demand import displacement_error
from door_mesh import expand as door_mesh
from door_mesh import template as door_template
from facade_entry_geometry import basis_for, expand
from industrial_hall_shell import metric, ramp
from opening_modules import place, template
from outshine_ref.buildings.interiors.doors import DoorLeaf
from outshine_ref.buildings.modelling import Shape
from outshine_ref.buildings.roller_gates import leaf_surface


def region_solid(region, depth, material):
    raw = Shape.from_polygon(region).extrude(depth).emit()
    raw["vertices"] = raw["vertices"] @ np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]])
    return metric(raw, material)


def placed(raw, gate, *, offset_m=0):
    t = np.asarray(gate["tangent"])
    basis = np.array([[*t, 0], [0, 0, 1], [t[1], -t[0], 0]])
    origin = (
        np.r_[np.asarray(gate["centre"]) - t * gate["width_m"] / 2, gate["bottom_m"]]
        + offset_m * basis[2]
    )
    return dict(
        raw,
        vertices=raw["vertices"] @ basis + origin,
        normals=raw["normals"] @ basis,
        facade_extent=np.zeros_like(raw["vertices"]),
    )


@lru_cache(maxsize=32)
def gate_fixed(width, height):
    outer = shapely.box(-0.08, 0, width + 0.08, height + 0.08)
    region = outer.difference(shapely.box(0, -0.01, width, height))
    frame = region_solid(region, 0.08, 10)
    housing = region_solid(
        shapely.box(0, height + 0.08, width, height + 0.38), 0.36, 10
    )
    return frame, housing


def gate_points(plan):
    gate, definition = plan["gate"], plan["definition"]
    t = np.asarray(gate["tangent"])
    return np.array(
        [
            [*(np.asarray(gate["centre"]) + t * u), v]
            for u in (-definition.width_m / 2, definition.width_m / 2)
            for v in (0, definition.height_m)
        ]
    )


def roller_leaf(plan, opened, detailed):
    gate, definition = plan["gate"], plan["definition"]
    leaf = leaf_surface(definition, opened, detail=detailed)
    return [placed(leaf, gate, offset_m=0.004)] if len(leaf["faces"]) else []


def roller(plan, opened, view):
    gate, definition = plan["gate"], plan["definition"]
    frame, housing = gate_fixed(gate["width_m"], gate["height_m"])
    output = [placed(frame, gate), placed(housing, gate)]
    detailed = (
        displacement_error(gate_points(plan), view, definition.rib_m)
        > view.detail_error_px
    )
    output.extend(roller_leaf(plan, opened, detailed))
    return output, detailed


def personnel_leaf(entry, material):
    door = entry["door"]
    basis = basis_for(entry["frame"])
    leaf = DoorLeaf(
        entry["id"],
        tuple(door["centre"]),
        tuple(door["tangent"]),
        tuple(-basis[2, :2]),
        0.16,
        wall_face_m=0.24,
    )
    raw = door_mesh(leaf, door_template(leaf), entry["angle_deg"])
    raw["materials"][:] = 10
    return expand(raw, material)


def personnel_fixed(entry, material):
    door = entry["door"]
    result = []
    framing = place(
        template("entrance", 1.1, 2.15), door["centre"], door["tangent"], 0.16
    )
    for part in np.unique(framing["parts"]):
        result.append(
            expand(
                dict(
                    framing,
                    faces=framing["faces"][framing["parts"] == part],
                    materials=framing["materials"][framing["parts"] == part],
                ),
                material,
            )
        )
    storage = [], [], [], []
    solid(
        storage,
        door["landing"].difference(shapely.from_wkb(entry["source_footprint_wkb"])),
        0,
        0.16,
        4,
    )
    if storage[1]:
        result.append(
            metric(
                {"vertices": np.asarray(storage[0]), "faces": np.asarray(storage[1])}, 4
            )
        )
    return result


def personnel(entry, material):
    return [personnel_leaf(entry, material), *personnel_fixed(entry, material)]


def windows(plan, material):
    output, templates = [], {}
    for window in plan["windows"]:
        key = window["width_m"], window["height_m"]
        if key not in templates:
            templates[key] = template("window", *key)
        raw = place(
            templates[key], window["centre"], window["tangent"], window["bottom_m"]
        )
        for part in np.unique(raw["parts"]):
            output.append(
                expand(
                    dict(
                        raw,
                        faces=raw["faces"][raw["parts"] == part],
                        materials=raw["materials"][raw["parts"] == part],
                    ),
                    material,
                )
            )
        glass = metric(
            Shape.from_polygon(shapely.box(0, 0, *key)).extrude(0.004).emit(), 3
        )
        output.append(placed(glass, window, offset_m=-0.14))
    return output


def products(plan, material, opened, view):
    entry = plan["entry"] | {"source_footprint_wkb": plan["footprint_wkb"]}
    output, detailed = roller(plan, opened, view)
    return [
        ramp(plan),
        *personnel(entry, material),
        *windows(plan, material),
        *output,
    ], detailed
