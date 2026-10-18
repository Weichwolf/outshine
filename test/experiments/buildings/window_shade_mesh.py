"""Select stable slots first, then reuse closed profiled or equivalent bar templates."""

import numpy as np
import shapely
from building_mesh import solid
from opening_modules import place

SHADE_MATERIAL = 60004


def profile(slot):
    half = slot.width_m / 2
    cap = slot.cap_bottom_m - slot.bottom_m
    top = slot.top_m - slot.bottom_m
    if slot.programme == "eyebrow":
        return shapely.box(-half, cap, half, top)
    inner = half - slot.thickness_m
    return shapely.Polygon(
        [
            (-half, 0),
            (-inner, 0),
            (-inner, cap),
            (inner, cap),
            (inner, 0),
            (half, 0),
            (half, top),
            (-half, top),
        ]
    )


def template(slot, method):
    polygon = profile(slot)
    if method == "profile":
        sections = [polygon]
    elif method == "bars":
        half = slot.width_m / 2
        cap = slot.cap_bottom_m - slot.bottom_m
        top = slot.top_m - slot.bottom_m
        sections = [shapely.box(-half, cap, half, top)]
        if slot.programme == "returned-hood":
            sections += [
                shapely.box(-half, 0, -half + slot.thickness_m, cap),
                shapely.box(half - slot.thickness_m, 0, half, cap),
            ]
    else:
        raise ValueError("Unknown shade extrusion method")
    storage = [], [], [], []
    for section in sections:
        solid(storage, section, -slot.back_m, slot.depth_m, SHADE_MATERIAL)
    # A proper rotation: profile x/z and extrusion depth become facade x/-y/z.
    vertices = np.asarray(storage[0], dtype=np.float64)[:, [0, 2, 1]]
    vertices *= [1, -1, 1]
    return {
        "vertices": vertices,
        "faces": np.asarray(storage[1], dtype=np.int32),
        "materials": np.asarray(storage[2], dtype=np.uint16),
        "parts": np.asarray(storage[3], dtype=np.int32),
    }


def expand(plan, selected_ids=None, *, method="profile", reuse=True):
    if method not in ("profile", "bars"):
        raise ValueError("Unknown shade extrusion method")
    known = {s.identifier for s in plan.slots}
    selected = known if selected_ids is None else set(selected_ids)
    if not selected <= known:
        raise ValueError("Shade selection contains an unknown slot")
    slots = [(i, s) for i, s in enumerate(plan.slots) if s.identifier in selected]
    arrays = {
        key: [] for key in ("vertices", "faces", "materials", "parts", "slot_indices")
    }
    cache, vertex_offset, part_offset = {}, 0, 0
    for index, slot in slots:
        key = (
            slot.programme,
            slot.width_m,
            slot.depth_m,
            slot.back_m,
            slot.cap_bottom_m - slot.bottom_m,
            slot.top_m - slot.bottom_m,
            slot.thickness_m,
            method,
        )
        if not reuse or key not in cache:
            cache[key] = template(slot, method)
        product = place(cache[key], slot.centre, slot.tangent, slot.bottom_m)
        arrays["vertices"].append(product["vertices"])
        arrays["faces"].append(product["faces"] + vertex_offset)
        arrays["materials"].append(product["materials"])
        arrays["parts"].append(product["parts"] + part_offset)
        arrays["slot_indices"].append(
            np.full(len(product["faces"]), index, dtype=np.int32)
        )
        vertex_offset += len(product["vertices"])
        part_offset += int(product["parts"].max()) + 1
    empty = {
        "vertices": np.empty((0, 3)),
        "faces": np.empty((0, 3), dtype=np.int32),
        "materials": np.empty(0, dtype=np.uint16),
        "parts": np.empty(0, dtype=np.int32),
        "slot_indices": np.empty(0, dtype=np.int32),
    }
    return {
        key: np.concatenate(value) if value else empty[key]
        for key, value in arrays.items()
    }
