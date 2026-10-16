"""Reuse the closed roof envelope and subtract one room and owned openings once."""

import numpy as np
import shapely
from outshine_ref.buildings.interiors.geometry import rectangle
from outshine_ref.buildings.modelling import PrismVolume, Shape, subtract_prisms
from outshine_ref.buildings.modelling.faces import Face


def cuts(plan, material):
    entry, gate = plan["entry"], plan["gate"]
    room = shapely.from_wkb(plan["interior_wkb"])
    result = [
        PrismVolume(plan["room_id"], room, plan["floor_m"], plan["ceiling_m"], material)
    ]
    for opening in [
        dict(entry["door"], id=entry["id"], width_m=1.1, bottom_m=0.16),
        gate,
        *plan["windows"],
    ]:
        region = rectangle(
            opening["centre"], opening["tangent"], opening["width_m"], 0.10, 0.34
        )
        result.append(
            PrismVolume(
                opening["id"],
                region,
                opening["bottom_m"],
                opening["bottom_m"] + opening["height_m"],
                material,
            )
        )
    result.append(
        PrismVolume(
            gate["id"] + ":ramp-pocket",
            shapely.from_wkb(gate["pocket_wkb"]),
            0,
            0.16,
            material,
        )
    )
    return result


def structure(plan, envelope, material, *, extra_cuts=()):
    shell = subtract_prisms(
        plan["id"] + ":roof-envelope", envelope, [*cuts(plan, material), *extra_cuts]
    )
    raw = dict(shell.mesh)
    points = raw["vertices"][raw["faces"]]
    normals = raw["normals"][raw["faces"][:, 0]]
    mats = raw["materials"].copy()
    room = shell.source_indices == 1
    mats[room & (normals[:, 2] > 0.5)] = 0
    mats[room & (normals[:, 2] < -0.5)] = 2
    uv = raw["uv"].copy().reshape(-1, 3, 2)
    for axis in range(3):
        selected = (shell.source_indices != 0) & (abs(normals).argmax(axis=1) == axis)
        uv[selected] = points[selected][:, :, [i for i in range(3) if i != axis]]
    return dict(
        raw,
        uv=uv.reshape(-1, 2),
        materials=mats,
        facade_extent=np.zeros_like(raw["vertices"]),
    ), shell


def metric(raw, material):
    points = raw["vertices"][raw["faces"]]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    along = points[:, 1] - points[:, 0]
    along /= np.linalg.norm(along, axis=1, keepdims=True)
    across = np.cross(normals, along)
    uv = np.stack(
        (
            np.einsum("tij,tj->ti", points, along),
            np.einsum("tij,tj->ti", points, across),
        ),
        axis=2,
    )
    return {
        "vertices": points.reshape(-1, 3),
        "faces": np.arange(points.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normals, 3, axis=0),
        "uv": uv.reshape(-1, 2),
        "materials": np.full(len(points), material, dtype=np.uint16),
        "facade_extent": np.zeros((points.size // 3, 3)),
    }


def ramp(plan):
    gate = plan["gate"]
    tangent = np.asarray(gate["tangent"])
    inward = np.array([-tangent[1], tangent[0]])
    raw = (
        Shape.from_faces(
            np.array([[0.0, 0, 0], [0, 3, 0], [0, 3, 0.16]]), [Face((0, 1, 2))]
        )
        .extrude(gate["width_m"])
        .emit()
    )
    basis = np.array([[*tangent, 0], [*inward, 0], [0, 0, 1]])
    origin = np.r_[np.asarray(gate["centre"]) - tangent * gate["width_m"] / 2, 0]
    result = metric(raw, 0)
    result["vertices"] = result["vertices"] @ basis + origin
    result["normals"] = result["normals"] @ basis
    return result
