"""Bounded normal/depth partitions of canonical foliage groups into spatial charts."""

from dataclasses import dataclass
from functools import lru_cache
from types import MappingProxyType

import numpy as np
from leaf_sheet import average_blocks, organ_colour
from raster_batch import rasterize
from shoot_plan import normal_frame


@lru_cache(maxsize=64)
def coarse_leaf(blade):
    mesh = blade.mesh(16, across=1)
    for value in mesh.values():
        value.setflags(write=False)
    return MappingProxyType(mesh)


def group_geometry(plant, path, max_leaves=128):
    if type(max_leaves) is not int or not 0 <= max_leaves <= 256:
        raise ValueError("integer group budget up to 256 leaves required")
    summary = plant.summary(path)
    if summary["leaves"] > max_leaves:
        return None
    slot = path[-1] if path else 0
    if plant.slot_leaf_counts[len(path)][slot]:
        raise ValueError("group root must have no own leaves")
    anchor = plant.axis(path)
    poses = [plant.leaves(child, summary["leaves"]) for child in plant.children(path)]
    poses = [pose for pose in poses if pose is not None]
    if not poses or any(plant.children(child) for child in plant.children(path)):
        raise ValueError("one order of leaf-bearing children required")
    placement = {key: np.concatenate([pose[key] for pose in poses]) for key in poses[0]}
    vertices, faces, normals, parameters, variants, organ_faces = [], [], [], [], [], []
    offset = 0
    for index, (origin, frame, variant) in enumerate(
        zip(placement["origins"], placement["frames"], placement["variants"])
    ):
        mesh = coarse_leaf(plant.blades[int(variant)])
        world = mesh["vertices"] @ frame.T + origin
        vertices.append((world - anchor.origin) @ anchor.frame)
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"] @ frame.T @ anchor.frame)
        parameters.append(mesh["parameters"])
        variants.extend([variant] * len(mesh["vertices"]))
        organ_faces.extend([index] * len(mesh["faces"]))
        offset += len(mesh["vertices"])
    assert len(placement["ids"]) == summary["leaves"]
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "parameters": np.concatenate(parameters),
        "variants": np.asarray(variants),
        "organ_faces": np.asarray(organ_faces),
        "ids": placement["ids"],
        "anchor_origin": anchor.origin,
        "anchor_frame": anchor.frame,
        "plan_identity": plant.identity,
    }


@dataclass(frozen=True)
class Chart:
    mesh: dict
    colour: np.ndarray
    normal: np.ndarray
    members: np.ndarray
    frame: np.ndarray
    offset: float
    displacement_m: float


def organ_statistics(source):
    triangles = source["vertices"][source["faces"]]
    face_area = (
        np.linalg.norm(
            np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            ),
            axis=1,
        )
        / 2
    )
    count = len(source["ids"])
    weights = np.bincount(source["organ_faces"], weights=face_area, minlength=count)
    centres = np.column_stack(
        [
            np.bincount(
                source["organ_faces"],
                weights=face_area * triangles.mean(axis=1)[:, component],
                minlength=count,
            )
            / weights
            for component in range(3)
        ]
    )
    normals = source["normals"].reshape(-1, 3, 3).mean(axis=1)
    orientation = np.column_stack(
        [
            np.bincount(
                source["organ_faces"],
                weights=face_area * normals[:, component],
                minlength=count,
            )
            for component in range(3)
        ]
    )
    orientation /= np.linalg.norm(orientation, axis=1, keepdims=True)
    return centres, orientation


def make_chart(source, members, normal, size):
    face_mask = np.isin(source["organ_faces"], members)
    faces = source["faces"][face_mask]
    first = int(np.argmin(np.abs(normal)))
    forward = np.eye(3)[first] - normal[first] * normal
    frame = normal_frame(forward, normal)
    projected = source["vertices"] @ frame
    selected = projected[np.unique(faces)]
    offset = (selected[:, 2].min() + selected[:, 2].max()) / 2
    displacement = np.max(np.abs(selected[:, 2] - offset)) + 1e-12
    lower, upper = selected[:, :2].min(axis=0), selected[:, :2].max(axis=0)
    border = 1 if size == 8 else 2
    padding = (upper - lower) * border / (size - 2 * border)
    lower, upper = lower - padding, upper + padding
    attributes = np.concatenate(
        (
            source["normals"].reshape(-1, 3, 3)[face_mask] @ frame,
            source["parameters"][faces],
            source["variants"][faces, None],
        ),
        axis=-1,
    )
    covered, sampled = rasterize(projected, faces, attributes, lower, upper, size * 2)
    mapped = sampled[..., :3]
    mapped /= np.maximum(np.linalg.norm(mapped, axis=-1, keepdims=True), 1e-12)
    alpha = covered.astype(float)[..., None]
    rgba = average_blocks(
        np.concatenate(
            (organ_colour(sampled[..., 5], sampled[..., 4]) * alpha, alpha), axis=-1
        )
    )
    normal_map = average_blocks(
        np.concatenate(((mapped + 1) / 2 * alpha, alpha), axis=-1)
    )
    xy = np.asarray(
        (
            (lower[0], lower[1]),
            (upper[0], lower[1]),
            (upper[0], upper[1]),
            (lower[0], upper[1]),
        )
    )
    positions = np.column_stack((xy, np.full(4, offset))) @ frame.T
    mesh = {
        "vertices": positions,
        "faces": np.asarray(((0, 1, 2), (0, 2, 3)), dtype=np.int32),
        "normals": np.broadcast_to(normal, (6, 3)).copy(),
        "uv": np.asarray(((0, 0), (1, 0), (1, 1), (0, 1)), dtype=float),
    }
    return Chart(
        mesh, rgba, normal_map, members, frame, float(offset), float(displacement)
    )


def make_cloud(source, layers=2, size=32, fit=True):
    if (
        type(layers) is not int
        or not 1 <= layers <= 8
        or type(size) is not int
        or size not in (8, 16, 32, 64, 128)
    ):
        raise ValueError("bounded integer layer count and chart size required")
    centres, orientation = organ_statistics(source)
    axes = np.argmax(np.abs(orientation), axis=1)
    charts = []
    for axis in range(3):
        selected = np.flatnonzero(axes == axis)
        normal = np.eye(3)[axis]
        if fit and len(selected):
            signs = np.where(orientation[selected, axis] < 0, -1, 1)
            normal = np.sum(orientation[selected] * signs[:, None], axis=0)
            normal /= np.linalg.norm(normal)
        selected = selected[np.argsort(centres[selected] @ normal, kind="stable")]
        for members in (
            np.array_split(selected, min(layers, len(selected)))
            if len(selected)
            else ()
        ):
            fitted = normal
            if fit:
                signs = np.where(orientation[members] @ normal < 0, -1, 1)
                fitted = np.sum(orientation[members] * signs[:, None], axis=0)
                fitted /= np.linalg.norm(fitted)
            charts.append(make_chart(source, members, fitted, size))
    return tuple(charts)


def greedy_cloud(source, tolerance=0.015, size=32, angle_limit=0.26):
    if not np.isfinite(tolerance) or tolerance <= 0 or not 0 < angle_limit < np.pi / 2:
        raise ValueError("positive distance and acute orientation limits required")
    if type(size) is not int or size not in (8, 16, 32, 64, 128):
        raise ValueError("bounded chart resolution required")
    centres, orientation = organ_statistics(source)
    first_vertices = source["faces"][
        np.unique(source["organ_faces"], return_index=True)[1], 0
    ]
    normals = []
    for organ in range(len(source["ids"])):
        vertices = source["vertices"][
            np.unique(source["faces"][source["organ_faces"] == organ])
        ]
        centred = vertices - vertices.mean(axis=0)
        normal = np.linalg.eigh(centred.T @ centred)[1][:, 0]
        normal *= 1 if normal @ orientation[organ] >= 0 else -1
        normals.append(normal)
    normals = np.asarray(normals)
    projected = source["vertices"] @ normals.T
    minima = np.minimum.reduceat(projected, first_vertices, axis=0).T
    maxima = np.maximum.reduceat(projected, first_vertices, axis=0).T
    offsets = (np.diag(minima) + np.diag(maxima)) / 2
    valid = (
        np.maximum(np.abs(minima - offsets[:, None]), np.abs(maxima - offsets[:, None]))
        <= tolerance
    )
    valid &= np.abs(normals @ orientation.T) >= np.cos(angle_limit)
    if not np.all(np.diag(valid)):
        raise ValueError("tolerance requires spatial leaf fallback")
    remaining = np.ones(len(centres), dtype=bool)
    charts = []
    while np.any(remaining):
        covered = valid & remaining[None]
        chosen = int(np.argmax(covered.sum(axis=1)))
        members = np.flatnonzero(covered[chosen])
        charts.append(make_chart(source, members, normals[chosen], size))
        remaining[members] = False
    return tuple(charts)
