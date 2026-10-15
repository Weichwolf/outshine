"""Procedural reusable furniture templates; logical footprint/height equals their actual mesh bounds."""

import numpy as np
import shapely
from building_mesh import solid

WOOD = 40
METAL = 41
FABRIC = 42
SCREEN = 43
CERAMIC = 44


def box(storage, bounds, low, high, material):
    solid(storage, shapely.box(*bounds), low, high, material)


def table(storage, x, y, width, depth, height):
    box(storage, (x, y, x + width, y + depth), height - 0.055, height, WOOD)
    for a in (x + 0.06, x + width - 0.10):
        for b in (y + 0.06, y + depth - 0.10):
            box(storage, (a, b, a + 0.04, b + 0.04), 0.02, height - 0.055, METAL)


def chair(storage, x, y, width=0.48, depth=0.48, height=1.05, heading_deg=0.0):
    first = len(storage[0])
    box(storage, (x, y, x + width, y + depth), 0.43, 0.49, FABRIC)
    box(storage, (x, y + depth - 0.055, x + width, y + depth), 0.49, height, FABRIC)
    for a in (x + 0.045, x + width - 0.075):
        for b in (y + 0.045, y + depth - 0.075):
            box(storage, (a, b, a + 0.03, b + 0.03), 0.0, 0.43, METAL)
    if heading_deg:
        angle = np.deg2rad(heading_deg)
        c, s = np.cos(angle), np.sin(angle)
        matrix = np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])
        centre = np.array([x + width / 2, y + depth / 2, 0.0])
        storage[0][first:] = (
            (np.asarray(storage[0][first:]) - centre) @ matrix.T + centre
        ).tolist()


def template(spec):
    width, depth, height = spec.width_m, spec.depth_m, spec.height_m
    storage = ([], [], [], [])
    if spec.kind == "workstation":
        table(storage, -width / 2, 0.0, width, 0.65, 0.75)
        chair(storage, -0.24, depth - 0.48, height=1.05)
        box(storage, (-0.28, 0.15, 0.28, 0.20), 0.78, height, SCREEN)
        box(storage, (-0.18, 0.10, 0.18, 0.32), 0.75, 0.78, METAL)
    elif spec.kind == "storage":
        box(storage, (-width / 2, 0.0, -width / 2 + 0.04, depth), 0.0, height, WOOD)
        box(storage, (width / 2 - 0.04, 0.0, width / 2, depth), 0.0, height, WOOD)
        box(
            storage,
            (-width / 2 + 0.04, 0.0, width / 2 - 0.04, 0.025),
            0.0,
            height,
            WOOD,
        )
        for z in np.linspace(0.04, height - 0.04, 5):
            box(
                storage,
                (-width / 2 + 0.04, 0.025, width / 2 - 0.04, depth),
                z - 0.02,
                z + 0.02,
                WOOD,
            )
    elif spec.kind == "meeting-group":
        table(storage, -width / 2 + 0.52, 0.50, width - 1.04, depth - 1.0, 0.76)
        chair(storage, -width / 2, depth / 2 - 0.24, height=height, heading_deg=90.0)
        chair(
            storage,
            width / 2 - 0.48,
            depth / 2 - 0.24,
            height=height,
            heading_deg=-90.0,
        )
        chair(storage, -0.24, 0.0, height=height, heading_deg=180.0)
        chair(storage, -0.24, depth - 0.48, height=height)
    elif spec.kind == "tea-counter":
        box(storage, (-width / 2, 0.0, width / 2, depth), 0.0, 0.90, WOOD)
        box(storage, (-width / 2, 0.0, width / 2, depth), 0.90, 0.94, CERAMIC)
        box(storage, (-width / 2, 0.0, -width / 2 + 0.50, depth), 0.94, height, METAL)
        box(storage, (0.1, 0.15, 0.42, 0.45), 0.94, 1.27, METAL)
    elif spec.kind == "wc":
        box(storage, (-width / 2, 0.0, width / 2, 0.18), 0.0, height, CERAMIC)
        box(
            storage,
            (-width * 0.38, 0.16, width * 0.38, depth - 0.04),
            0.0,
            0.40,
            CERAMIC,
        )
        box(storage, (-width / 2, 0.18, width / 2, depth), 0.40, 0.46, CERAMIC)
    elif spec.kind == "washbasin":
        box(storage, (-width / 2, 0.0, width / 2, depth), 0.72, 0.82, CERAMIC)
        box(storage, (-0.10, 0.04, 0.10, 0.12), 0.0, 0.72, METAL)
        box(storage, (-0.06, 0.02, 0.06, 0.08), 0.82, height, METAL)
    else:
        raise ValueError("No procedural furnishing template for " + spec.kind)
    result = {
        "vertices": np.asarray(storage[0], dtype=float),
        "faces": np.asarray(storage[1], dtype=np.uint32),
        "materials": np.asarray(storage[2], dtype=np.uint16),
        "parts": np.asarray(storage[3], dtype=np.int32),
    }
    points = result["vertices"]
    bounds = np.r_[points.min(axis=0), points.max(axis=0)]
    expected = [-width / 2, 0.0, 0.0, width / 2, depth, height]
    np.testing.assert_allclose(bounds, expected, atol=1e-9, rtol=0.0)
    return result


def build(programme):
    arrays = {"vertices": [], "faces": [], "materials": [], "parts": []}
    offset = part_offset = 0
    instances = []
    cache = {}
    for room in programme["rooms"]:
        for item in room["result"]["items"]:
            spec = item["spec"]
            if spec not in cache:
                cache[spec] = template(spec)
            source = cache[spec]
            t = item["tangent"]
            front = np.array([-t[1], t[0]])
            rotation = np.array(
                [[t[0], front[0], 0.0], [t[1], front[1], 0.0], [0.0, 0.0, 1.0]]
            )
            points = source["vertices"] @ rotation.T + [
                *item["centre"],
                room["floor_z_m"],
            ]
            arrays["vertices"].append(points)
            arrays["faces"].append(source["faces"] + offset)
            arrays["materials"].append(source["materials"])
            arrays["parts"].append(source["parts"] + part_offset)
            instances.append(
                {
                    "id": item["id"],
                    "room_id": room["room_id"],
                    "storey": room["storey"],
                    "kind": item["kind"],
                    "first_triangle": sum(len(x) for x in arrays["faces"][:-1]),
                    "triangles": len(source["faces"]),
                    "template": spec.kind,
                }
            )
            offset += len(points)
            part_offset += int(source["parts"].max()) + 1
    if not instances:
        raise ValueError("Programme has no furnishing geometry to expand")
    return {k: np.concatenate(v) for k, v in arrays.items()}, {
        "instances": instances,
        "templates": len(cache),
        "scope": "Own explicit detailed control meshes from shared immutable templates; render-LOD/instanced native export remains separate",
    }
