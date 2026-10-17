"""Finite unlit needle-form catalog, expanded independently of whole plants."""

import hashlib
import json
from dataclasses import replace

import numpy as np
from conifer_windows import pattern_key
from foliage_selection import directions
from leaf_sheet import average_blocks
from needle_group_source import material_fields
from outshine_ref.vegetation import NeedledPlant
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.plant_plan import AxisState
from plant_atlas import projection
from raster_batch import rasterize


def prototypes(directory, scene):
    paths, result = [], []
    for product in scene["products"]:
        if product["surface"] != "leaf":
            continue
        path = directory / product["mesh"]
        paths.append(path)
        with np.load(path, allow_pickle=False) as saved:
            mesh = {key: saved[key] for key in saved.files}
        colour, _ = material_fields(directory, product, mesh, paths)
        result.append((mesh, colour))
    return result, paths


def family_geometry(plant, pattern, meshes, groups):
    return family_geometry_for_seed(plant.wood.seed, pattern, meshes, groups)


def family_geometry_for_seed(seed, pattern, meshes, groups):
    canonical = replace(pattern, first=0, last=1 - 0.5 / groups)
    length = groups * pattern.spacing
    axis = AxisState((), 0, length, 0, np.zeros(3), np.eye(3), 0, 0)
    count = groups * pattern.group_size
    poses = canonical.detail(axis, (0.0002, 0.00014), seed, count)
    assert len(poses["ids"]) == count
    vertices, faces, normals, colours, kinds, offset = [], [], [], [], [], 0
    for index in range(count):
        mesh, colour = meshes[(index + seed) % len(meshes)]
        frame, origin = poses["frames"][index], poses["origins"][index]
        vertices.append(mesh["vertices"] * poses["scales"][index] @ frame.T + origin)
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"].reshape(-1, 3, 3) @ frame.T)
        colours.append(colour)
        kinds.append(np.ones(len(mesh["faces"]), bool))
        offset += len(mesh["vertices"])
    if pattern.short_length:
        mesh = axis_mesh(pattern.short_length, 0, 0.0002, 0.00014, 2, 6)
        for frame, origin in zip(
            poses["short_frames"], poses["short_origins"], strict=True
        ):
            vertices.append(mesh["vertices"] @ frame.T + origin)
            faces.append(mesh["faces"] + offset)
            normals.append(mesh["normals"].reshape(-1, 3, 3) @ frame.T)
            colours.append(
                np.broadcast_to((0.14, 0.105, 0.037), (len(mesh["faces"]), 3, 3))
            )
            kinds.append(np.zeros(len(mesh["faces"]), bool))
            offset += len(mesh["vertices"])
    centre = np.array((0, (groups - 1) * pattern.spacing / 2, 0))
    source = {
        "vertices": np.concatenate(vertices) - centre,
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "colour": np.concatenate(colours),
        "leaf": np.concatenate(kinds),
    }
    metadata = {
        "needles": count,
        "pattern": pattern.recipe(),
        "phase": float(
            np.random.default_rng(np.random.SeedSequence((seed, 0))).uniform(
                -np.pi, np.pi
            )
        ),
        "source_extent_m": np.ptp(source["vertices"], axis=0).tolist(),
        "window_span_m": (groups - 1) * pattern.spacing,
    }
    return source, metadata


def catalog_sources(directory, *, nominal_span_m=0.08, max_groups=64):
    scene_path = directory / "shoot.json"
    scene = json.loads(scene_path.read_text())
    recipe_path = directory / scene["plant_recipe"]
    plant = NeedledPlant.from_recipe(json.loads(recipe_path.read_text()))
    meshes, paths = prototypes(directory, scene)
    paths += [scene_path, recipe_path]
    families = {}
    group_capacities = plant.window_summary(
        nominal_span_m=nominal_span_m, max_groups=max_groups
    )["groups_per_window"]
    for order, pattern in enumerate(plant.patterns):
        if pattern is None or pattern_key(pattern) in families:
            continue
        source, metadata = family_geometry(
            plant, pattern, meshes, group_capacities[order]
        )
        metadata["family"] = len(families)
        families[pattern_key(pattern)] = (source, metadata)
    return (
        plant,
        families,
        {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
    )


def template_image(source, direction, size=32, samples=4, *, projection_data=None):
    frame, lower, upper, offset = (
        projection(source["vertices"], direction)
        if projection_data is None
        else projection_data
    )
    points = source["vertices"] @ frame
    attributes = np.concatenate(
        (
            source["colour"],
            source["normals"],
            np.broadcast_to(
                source["leaf"][:, None, None], (*source["normals"].shape[:2], 1)
            ),
            points[source["faces"], 2, None],
        ),
        axis=-1,
    )
    covered, data = rasterize(
        points,
        source["faces"],
        attributes,
        lower,
        upper,
        size * samples,
        max(262144, (size * samples) ** 2),
    )
    normal = data[..., 3:6]
    normal /= np.maximum(np.linalg.norm(normal, axis=-1, keepdims=True), 1e-20)
    alpha = covered[..., None].astype(float)
    image = np.concatenate(
        (data[..., :3] * alpha, alpha, normal * alpha, data[..., 6:8] * alpha), axis=-1
    )
    return average_blocks(image, samples).astype(np.float32), (
        frame,
        lower,
        upper,
        offset,
    )


def bake_template(source, count=64, size=32, samples=4):
    return [
        template_image(source, direction, size, samples)
        for direction in directions(count)
    ]
