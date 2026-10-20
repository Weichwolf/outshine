"""Analytically filtered projected quadtree cells with current homogeneous GGX."""

import numpy as np
import shapely
from adaptive_area_measure import leaf_bounds
from opaque_area_grid import integration_points
from packed_normals import unpack
from visible_normal_material import shade


def program(arrays, geometry, view, box, *, refined=False):
    rectangles = leaf_bounds(arrays)
    eye, basis, focal, width, height = view
    rows, words, weights, coordinates = [], [], [], []
    work = {
        "leaves_tested": len(rectangles),
        "leaf_pixel_clips": 0,
        "quadrature_records": 0,
    }
    for leaf, bounds in enumerate(rectangles):
        begin, end = arrays["offsets"][leaf : leaf + 2]
        if begin == end:
            continue
        xy = np.array(
            [[bounds[x, 0], bounds[y, 1]] for x, y in ((0, 0), (1, 0), (1, 1), (0, 1))]
        )
        q = geometry["start"] + xy * geometry["step"]
        world = (
            geometry["eye"]
            + np.column_stack((q, np.ones(4))) * geometry["plane"] @ geometry["basis"]
        )
        local = (world - eye) @ basis.T
        if local[:, 2].min() <= 0.2:
            raise ValueError("projected measure crosses current near plane")
        screen = [width / 2, height / 2] + focal * local[:, :2] / local[:, 2, None] * [
            1,
            -1,
        ]
        low = np.maximum(np.floor(screen.min(0)), box[0]).astype(int)
        high = np.minimum(np.ceil(screen.max(0)), box[1]).astype(int)
        polygon = shapely.Polygon(screen)
        for y in range(low[1], high[1]):
            for x in range(low[0], high[0]):
                work["leaf_pixel_clips"] += 1
                part = shapely.intersection(polygon, shapely.box(x, y, x + 1, y + 1))
                if part.area <= 0:
                    continue
                points, mass = integration_points(part, refined=refined)
                count = len(points)
                selected = arrays["normal_words"][begin:end]
                rows.append(
                    np.full(
                        count * len(selected),
                        (y - int(box[0, 1])) * int(box[1, 0] - box[0, 0])
                        + x
                        - int(box[0, 0]),
                        np.int32,
                    )
                )
                words.append(np.tile(selected, count))
                coordinates.append(np.repeat(points, len(selected), axis=0))
                weights.append(
                    (mass[:, None] * arrays["weights"][None, begin:end]).ravel()
                )
    if not rows:
        return {
            "rows": np.empty(0, np.int32),
            "normal": np.empty((0, 3)),
            "weights": np.empty(0),
            "directions": np.empty((0, 3)),
            "work": work,
        }
    xy = np.concatenate(coordinates)
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
    work["quadrature_records"] = len(xy)
    return {
        "rows": np.concatenate(rows),
        "normal": unpack(np.concatenate(words)),
        "weights": np.concatenate(weights),
        "directions": directions,
        "work": work,
    }


def evaluate(program, shape, light, roughness, energy):
    count = int(np.prod(shape))
    values = shade(program["normal"], -program["directions"], light, roughness, energy)
    rgb = np.zeros((count, 3))
    np.add.at(rgb, program["rows"], program["weights"][:, None] * values)
    coverage = np.bincount(program["rows"], weights=program["weights"], minlength=count)
    width, height = map(int, shape)
    return rgb.reshape(height, width, 3), coverage.reshape(height, width, 1)
