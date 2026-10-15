"""Aggregate finite physical windows and plan wood into complete unlit tree views."""

import numpy as np
from foliage_selection import directions
from leaf_sheet import average_blocks, sample_image
from outshine_ref.vegetation.axis_surface import axis_mesh
from plant_atlas import projection
from raster_batch import fragments
from textured_raster import composite, wood_fragments


def plan_wood(plant):
    vertices, faces, normals, offset = [], [], [], 0
    for path in plant.wood.paths():
        axis, level = plant.wood.axis(path), plant.wood.levels[len(path)]
        mesh = axis_mesh(
            axis.length,
            axis.bend,
            level.base_radius,
            level.tip_radius,
            4 if not path else 2,
            8,
        )
        vertices.append(mesh["vertices"] @ axis.frame.T + axis.origin)
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"] @ axis.frame.T)
        offset += len(mesh["vertices"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
    }


def window_fragments(windows, banks, direction, frame, lower, upper, size):
    local = np.einsum("j,njk->nk", direction, windows["frames"])
    choices = np.argmax(local @ directions(len(banks[0])).T, axis=1)
    pieces = []
    uv = np.array(((0, 0), (1, 0), (1, 1), (0, 1)))
    total = 0
    for family in np.unique(windows["families"]):
        for view_index in np.unique(choices[windows["families"] == family]):
            selected = np.flatnonzero(
                (windows["families"] == family) & (choices == view_index)
            )
            image, (basis, lo, hi, _) = banks[int(family)][int(view_index)]
            offset = float(image[..., 8].sum() / max(image[..., 3].sum(), 1e-20))
            quad = (
                np.column_stack(
                    (
                        np.array(
                            (
                                (lo[0], lo[1]),
                                (hi[0], lo[1]),
                                (hi[0], hi[1]),
                                (lo[0], hi[1]),
                            )
                        ),
                        np.full(4, offset),
                    )
                )
                @ basis.T
            )
            world = (
                np.einsum("nij,vj->nvi", windows["frames"][selected], quad)
                + windows["origins"][selected, None]
            )
            vertices = (world @ frame).reshape(-1, 3)
            faces = (
                np.arange(len(selected))[:, None, None] * 4
                + np.array(((0, 1, 2), (0, 2, 3)))
            ).reshape(-1, 3)
            for triangle, weights, pixel, z in fragments(
                vertices, faces, lower, upper, size, max(262144, size * size)
            ):
                owner = selected[triangle // 2]
                coords = np.einsum(
                    "ni,nij->nj",
                    weights,
                    uv[np.array(((0, 1, 2), (0, 2, 3)))[triangle % 2]],
                )
                data = sample_image(image, coords)
                data[:, 4:7] = np.einsum(
                    "nij,nj->ni", windows["frames"][owner], data[:, 4:7]
                )
                data[:, :8] *= windows["weights"][owner, None]
                data[:, 8] = z * data[:, 3]
                keep = data[:, 3] > 0
                total += int(keep.sum())
                if total > 16 * size * size:
                    raise ValueError("complete window view exceeds fragment budget")
                pieces.append((pixel[keep], owner[keep], z[keep], data[keep]))
    if not pieces:
        return np.empty(0, int), np.empty(0), np.empty((0, 9))
    pixel, owner, depth, samples = (
        np.concatenate([p[i] for p in pieces]) for i in range(4)
    )
    if not len(pixel):
        return pixel, depth, samples
    order = np.lexsort((np.arange(len(pixel)), owner, pixel))
    keys = pixel[order] * len(windows["origins"]) + owner[order]
    order = order[np.r_[keys[1:] != keys[:-1], True]]
    return pixel[order], depth[order], samples[order]


def tree_bounds(windows, banks, wood):
    extent = max(
        float(np.linalg.norm(view[1][2] - view[1][1]))
        for bank in banks
        for view in bank
    )
    lower3 = np.minimum(
        wood["vertices"].min(axis=0), windows["origins"].min(axis=0) - extent
    )
    upper3 = np.maximum(
        wood["vertices"].max(axis=0), windows["origins"].max(axis=0) + extent
    )
    return np.array(np.meshgrid(*zip(lower3, upper3))).reshape(3, -1).T


def tree_image(windows, banks, wood, direction, size=128, samples=2):
    corners = tree_bounds(windows, banks, wood)
    frame, lower, upper, offset = projection(corners, direction)
    parts = [
        window_fragments(
            windows, banks, direction, frame, lower, upper, size * samples
        ),
        wood_fragments(wood, frame, lower, upper, size * samples),
    ]
    pixel, depth, data = (np.concatenate([p[i] for p in parts]) for i in range(3))
    return average_blocks(composite(pixel, depth, data, size * samples), samples), (
        frame,
        lower,
        upper,
        offset,
    )
