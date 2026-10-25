"""One finite-quadrature GGX comparison on unchanged cameras and frozen geometry."""

import numpy as np
from building_mass_lighting import shade
from current_visibility import project, raster
from triangle_image_reference import average_samples, image_rays


def hit_materials(source, fields, points, faces):
    """Evaluate the two analytic material states at original-face hit locations."""
    corner = source["indices"][faces, 0]
    uv_y = source["uv"][corner, 1] + np.einsum(
        "ij,ij->i", fields["uv_gradient"][faces], points - source["positions"][corner]
    )
    state = (uv_y >= 1).astype(int)
    if not np.all(fields["supported"][faces]):
        raise ValueError("visible original material states are unqualified")
    return fields["albedo"][faces, state], fields["weights"][faces, state]


def samples(source, view, box, supersample):
    width, height = map(int, box[1] - box[0])
    projected, forward = project(source["positions"], view)
    if np.any(forward <= 0.1):
        raise ValueError("diagnostic requires geometry wholly beyond the near plane")
    projected[:, :2] = (projected[:, :2] - box[0]) * supersample
    depth, face = raster(
        projected,
        forward,
        source["indices"],
        width * supersample,
        height * supersample,
        with_faces=True,
    )
    _, rays = image_rays(view, box, width, height, supersample)
    hit = face.ravel() >= 0
    t = 0.1 / depth.ravel()[hit]
    return {
        "hit": hit,
        "faces": face.ravel()[hit],
        "points": view[0] + t[:, None] * rays[hit],
        "outgoing": -rays[hit] / np.linalg.norm(rays[hit], axis=1)[:, None],
    }


def evaluate(
    source,
    original,
    fields,
    appearance,
    product,
    view,
    box,
    tables,
    *,
    parent=False,
    neutral=False,
    diagnostic=None,
    supersample=16,
):
    supersample = supersample if diagnostic is None else 1
    data = samples(source, view, box, supersample) if diagnostic is None else diagnostic
    faces = data["faces"]
    count = len(product["indices"]) if parent else 0
    grouped = faces < count
    colour = np.zeros((len(faces), 3))
    weights = np.zeros((len(faces), len(appearance["roughness_palette"])))
    triangle = source["positions"].astype(float)[source["indices"]]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    normal /= np.where(
        np.linalg.norm(normal, axis=1) > 0, np.linalg.norm(normal, axis=1), 1
    )[:, None]
    if parent:
        original_faces = np.flatnonzero(
            np.isin(original["binding"][:, 1], product["needs_sources"])
        )
        local = original_faces[faces[~grouped] - count]
    else:
        local = faces
    colour[~grouped], weights[~grouped] = hit_materials(
        original, fields, data["points"][~grouped], local
    )
    if grouped.any():
        groups = product["triangle_groups"][faces[grouped]]
        role = (normal[faces[grouped], 2] > 0.5).astype(int)
        if not np.all(appearance["supported"][groups]):
            raise ValueError("visible parent groups have unsupported material measures")
        colour[grouped] = appearance["albedo"][groups, role]
        weights[grouped] = appearance["roughness_weights"][groups, role]
    palette = appearance["roughness_palette"]
    if neutral and grouped.any():
        colour[grouped] = 0.6
        weights = np.pad(weights, ((0, 0), (0, 1)))
        weights[grouped] = 0
        weights[grouped, -1] = 1
        palette = np.r_[palette, 0.7]
    value, work = shade(
        colour,
        weights,
        palette,
        normal[faces],
        data["outgoing"],
        [0.45, -0.5, 0.7],
        tables,
    )
    width, height = map(int, box[1] - box[0])
    c = np.zeros((len(data["hit"]), 3))
    c[data["hit"]] = value
    return {
        "C": average_samples(c, width, height, supersample),
        "T": average_samples((~data["hit"]).astype(float), width, height, supersample),
    }, dict(
        work,
        sample_grid_per_pixel=[supersample, supersample],
        hit_samples=int(data["hit"].sum()),
        all_samples=len(data["hit"]),
    )
