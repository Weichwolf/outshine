"""One existing cell becomes a shared entrance; actual leaf and clear supporting slabs."""

import numpy as np
from building_mesh import solid
from door_mesh import expand as leaf_mesh
from door_mesh import template as leaf_template
from opening_modules import place
from opening_modules import template as frame_template
from outshine_ref.buildings.interiors.doors import DoorLeaf
from outshine_ref.buildings.opening_panels import OpeningPanel, panel_surface
from tile_room_details import corner_buffers


def expand(raw, material):
    output = corner_buffers(raw, material)
    output["facade_extent"] = np.zeros((len(output["vertices"]), 3))
    return output


def local_quads(rectangles, basis, origin, scale, extent, material):
    vertices, uv = [], []
    for x0, y0, x1, y1, w in rectangles:
        quad = np.array([[x0, y0, w], [x1, y0, w], [x1, y1, w], [x0, y1, w]])
        vertices.extend(quad)
        uv.extend(quad[:, :2] / scale)
    points = np.asarray(vertices).reshape(-1, 3)
    faces = (
        np.arange(len(points), dtype=np.uint32)
        .reshape(-1, 4)[:, [[0, 1, 2], [0, 2, 3]]]
        .reshape(-1, 3)
    )
    return {
        "vertices": points @ basis + origin,
        "faces": faces,
        "normals": np.tile(basis[2], (len(points), 1)),
        "uv": np.asarray(uv).reshape(-1, 2),
        "facade_extent": np.tile(extent, (len(points), 1)),
        "materials": np.full(len(faces), material, dtype=np.uint16),
    }


def basis_for(frame):
    a = np.asarray(frame["a"])
    tangent = (np.asarray(frame["b"]) - a) / frame["width_m"]
    normal = np.array([tangent[1], -tangent[0], 0.0])
    return np.array([np.r_[tangent, 0.0], [0.0, 0.0, 1.0], normal])


def flat_remainder(entry, material):
    frame = entry["frame"]
    width, height = entry["cell_width_m"], entry["floor_m"]
    low, high = entry["column"] * width, (entry["column"] + 1) * width
    rectangles = [
        (0.0, 0.0, low, frame["height_m"], 0.0),
        (high, 0.0, frame["width_m"], frame["height_m"], 0.0),
        (low, height, high, frame["height_m"], 0.0),
    ]
    rectangles = [r for r in rectangles if r[2] > r[0] and r[3] > r[1]]
    return local_quads(
        rectangles,
        basis_for(frame),
        np.r_[frame["a"], 0.0],
        np.array([width, height]),
        [frame["columns"], frame["rows"], 0.0],
        material,
    )


def doorway(entry, material, mode, *, base_m=0.0):
    frame = entry["frame"]
    basis = basis_for(frame)
    origin = (
        np.r_[frame["a"], base_m] + basis[0] * entry["column"] * entry["cell_width_m"]
    )
    width, height = entry["cell_width_m"], entry["floor_m"]
    opening = (width / 2 - 0.55, 0.16, width / 2 + 0.55, 0.16 + 2.15)
    if mode == "compact":
        return [
            local_quads(
                [(0.0, 0.0, width, height, 0.0)],
                basis,
                origin,
                [1.0, 1.0],
                [0.0, 0.0, 0.0],
                material,
            ),
            local_quads(
                [(*opening, 0.001)], basis, origin, [1.0, 1.0], [0.0, 0.0, 0.0], 10
            ),
        ]
    if mode != "through":
        raise ValueError("Unknown entrance representation")
    panel = panel_surface(OpeningPanel(width, height, opening))
    raw = {
        "vertices": panel["vertices"] @ basis + origin,
        "faces": panel["faces"],
        "materials": np.ones(len(panel["faces"]), dtype=np.uint8),
    }
    output = [expand(raw, material)]
    door = entry["door"]
    leaf = DoorLeaf(
        entry["id"],
        tuple(door["centre"]),
        tuple(door["tangent"]),
        tuple(-basis[2, :2]),
        base_m + 0.16,
        wall_face_m=0.24,
    )
    raw = leaf_mesh(leaf, leaf_template(leaf), entry["angle_deg"])
    raw["materials"][:] = 10
    output.append(expand(raw, material))
    framing = place(
        frame_template("entrance", 1.10, 2.15),
        door["centre"],
        door["tangent"],
        base_m + 0.16,
    )
    for part in np.unique(framing["parts"]):
        raw = dict(
            framing,
            faces=framing["faces"][framing["parts"] == part],
            materials=framing["materials"][framing["parts"] == part],
        )
        output.append(expand(raw, material))
    return output


def products(entry, material, mode):
    output = doorway(entry, material, mode)
    if mode == "compact":
        return output
    door, height = entry["door"], entry["floor_m"]
    for polygon, component in ((entry["interior"], 0), (door["landing"], 4)):
        storage = [], [], [], []
        solid(storage, polygon, 0.0, 0.16, component)
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
    storage = [], [], [], []
    solid(storage, entry["interior"], height - 0.18, height, 2)
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
