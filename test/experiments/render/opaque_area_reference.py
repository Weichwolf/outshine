"""Visible-polygon area and GGX quadrature; no point-sampled coverage oracle."""

import numpy as np
from opaque_area_grid import AreaGrid, integration_points
from visible_normal_material import shade


def pixel_grid(source, view, box):
    eye, basis, focal, width, height = view
    start = (box[0] - [width / 2, height / 2]) * [1, -1] / focal
    step = np.array([1, -1]) / focal
    return AreaGrid(source, eye, basis, start, step, box[1] - box[0])


def quadrature(fragments, view, box, *, refined=False):
    """Keep visibility fixed while changing only within-fragment quadrature."""
    _, basis, focal, width, height = view
    rows, faces, coordinates, masses = [], [], [], []
    for row, pieces in fragments:
        for face, polygon in pieces:
            xy, weight = integration_points(polygon, refined=refined)
            rows.append(np.full(len(weight), row, np.int32))
            faces.append(np.full(len(weight), face, np.int32))
            coordinates.append(xy)
            masses.append(weight)
    if not rows:
        xy, weights = np.empty((0, 2)), np.empty(0)
        rows, faces = np.empty(0, np.int32), np.empty(0, np.int32)
    else:
        rows, faces = np.concatenate(rows), np.concatenate(faces)
        xy, weights = np.concatenate(coordinates), np.concatenate(masses)
    xy += box[0]
    directions = (
        np.column_stack(
            (
                (xy[:, 0] - width / 2) / focal,
                -(xy[:, 1] - height / 2) / focal,
                np.ones(len(xy)),
            )
        )
        @ basis
    )
    return {"rows": rows, "faces": faces, "weights": weights, "directions": directions}


def evaluate(program, source, shape, light, roughness, energy):
    count = int(np.prod(shape))
    values = shade(
        source["vertex_normal"][source["indices"][program["faces"], 0]],
        -program["directions"],
        light,
        roughness,
        energy,
    )
    image = np.zeros((count, 3))
    np.add.at(image, program["rows"], program["weights"][:, None] * values)
    coverage = np.bincount(program["rows"], weights=program["weights"], minlength=count)
    width, height = map(int, shape)
    return image.reshape(height, width, 3), coverage.reshape(height, width, 1)
