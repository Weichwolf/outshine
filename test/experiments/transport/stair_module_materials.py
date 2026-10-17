"""Source-owned presentation slots; geometry and original prefixes stay intact."""

import numpy as np


def surface_slots(rows, owners):
    roads = np.array(
        [r.get("source_recipe") == "minor_ground_carriageway" for r in rows]
    )
    return np.where(np.any(owners & roads, axis=1), 8, 3)


def body_slots(data):
    if "surface_materials" not in data:
        return np.full(len(data["body_faces"]), 3, int)
    top = data["surface_materials"]
    return np.r_[top, top, np.full(len(data["body_faces"]) - 2 * len(top), 6, int)]


def colour_component(shown, first_face, data, palette):
    slots = body_slots(data)
    end = first_face + len(slots)
    shown["materials"][first_face:end] = slots
    shown["face_base_colour_linear_rgba"][first_face:end] = palette[slots]
