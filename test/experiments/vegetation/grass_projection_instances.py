"""Conservative cached-form bounds select physical roots before vertex expansion."""

import itertools

import numpy as np
from grass_projection import contacts, query_domain
from grass_projection_stand import expand_selected, require_support


def potential_roots(stand, lower, upper, direction):
    form = stand["form_vertices"]
    bounds = np.array((form.min(1), form.max(1))).transpose(1, 0, 2)
    corners = np.array(list(itertools.product((0, 1), repeat=3)))
    boxes = bounds[:, corners, np.arange(3)]
    boxes = boxes[stand["pose_variants"]]
    world = (
        np.einsum("nij,nkj->nki", stand["pose_frames"], boxes)
        * stand["pose_scales"][:, None, None]
    )
    world += stand["pose_origins"][:, None]
    projection = world[..., :2] - world[..., 2, None] * direction[:2] / direction[2]
    selected = (
        (world[..., 2].max(1) >= 0)
        & np.all(projection.max(1) >= lower, axis=1)
        & np.all(projection.min(1) <= upper, axis=1)
    )
    return np.flatnonzero(selected)


def instanced_contacts(
    recipe, stand, lower, upper, direction, size, pair_budget=262144
):
    lower, upper, direction = query_domain(lower, upper, direction, size)
    require_support(recipe, lower, upper, direction)
    if type(pair_budget) is not int or not size * size <= pair_budget <= 4_194_304:
        raise ValueError("bounded pair batch required before root expansion")
    selected = potential_roots(stand, lower, upper, direction)
    vertices, faces, _, keys = expand_selected(stand, selected)
    product = contacts(vertices, faces, lower, upper, direction, size, pair_budget)
    present = product["face"] >= 0
    product["face"][present] = keys[product["face"][present]]
    product["work"].update(
        root_boxes=len(stand["pose_ids"]),
        box_corners=8 * len(stand["pose_ids"]),
        selected_roots=len(selected),
        expanded_triangles=len(faces),
        expanded_geometry_bytes=vertices.nbytes + faces.nbytes,
        canonical_face_key_bytes=keys.nbytes,
    )
    return product
