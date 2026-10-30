"""Independent numerical screen-pixel areas of one constant-cell RGBA field.

Existing GEOS rectangle intersections handle boundary cells; contained cells use
quadrilateral area. This is a diagnostic of the same field/plane, not original
scene shading, native execution, visibility/depth authority or an exact bound.
"""

import numpy as np
import shapely
from field_scale_projection import project
from urban_parent_areas import direct_areas


def validate(field, view):
    if (
        not isinstance(field, np.ndarray)
        or field.ndim != 3
        or field.shape[2] != 4
        or field.dtype.kind != "f"
        or min(field.shape[:2]) < 1
        or max(field.shape[:2]) > 512
        or field.shape[0] != field.shape[1]
        or view.width * view.height > 16384
        or not np.isfinite(field).all()
        or np.any(field < 0)
        or np.any(field[:, :, 3] > 1)
        or np.any(field[:, :, :3][field[:, :, 3] == 0] != 0)
    ):
        raise ValueError("Bounded square premultiplied constant RGBA cells required")


def project_coordinates(coordinates, size, grid, view, plane):
    captured = coordinates / size * 2 - 1
    captured[:, 1] *= -1
    world = plane.points(grid, captured)
    camera = (
        np.asarray(view.eye),
        np.asarray(view.basis),
        view.focal_px,
        view.width,
        view.height,
    )
    return project(world, camera)


def pixel_pairs(low, high, capacity):
    spans = np.maximum(high - low + 1, 0)
    counts = np.prod(spans, axis=1, dtype=np.int64)
    total = int(counts.sum())
    if type(capacity) is not int or capacity < 1 or total > capacity:
        raise ValueError("Cell/pixel clipping capacity exceeded before expansion")
    owners = np.repeat(np.arange(len(counts)), counts)
    offset = np.arange(total) - np.repeat(np.cumsum(counts) - counts, counts)
    pixel = low[owners] + np.column_stack(
        (offset % spans[owners, 0], offset // spans[owners, 0])
    )
    return owners, pixel


def cell_integrals(field, grid, view, plane, *, pairs_most=1048576):
    validate(field, view)
    y, x = np.nonzero(field[:, :, 3] > 0)
    image = np.zeros((view.height, view.width, 4), float)
    if not len(x):
        return image, {
            "source_cells": 0,
            "inside_pixel_cells": 0,
            "clipped_cell_pixel_pairs": 0,
        }
    corners = np.column_stack((x, y))[:, None] + np.array(
        [[0, 0], [1, 0], [1, 1], [0, 1]]
    )
    xy = project_coordinates(
        corners.reshape(-1, 2), len(field), grid, view, plane
    ).reshape(-1, 4, 2)
    low_raw, high_raw = (
        np.floor(xy.min(1)).astype(np.int64),
        np.floor(xy.max(1)).astype(np.int64),
    )
    low = np.maximum(low_raw, 0)
    high = np.minimum(high_raw, [view.width - 1, view.height - 1])
    whole = (
        (low_raw == high_raw).all(1)
        & (low_raw >= 0).all(1)
        & (high_raw < [view.width, view.height]).all(1)
    )
    colour = field[y, x].astype(float)
    area = (
        abs(
            np.sum(
                xy[:, :, 0] * np.roll(xy[:, :, 1], -1, axis=1)
                - xy[:, :, 1] * np.roll(xy[:, :, 0], -1, axis=1),
                axis=1,
            )
        )
        / 2
    )
    np.add.at(image, (low[whole, 1], low[whole, 0]), colour[whole] * area[whole, None])
    boundary = np.flatnonzero(~whole)
    owners, pixel = pixel_pairs(low[boundary], high[boundary], pairs_most)
    owners = boundary[owners]
    overlap = shapely.area(
        shapely.intersection(
            shapely.polygons(xy[owners]),
            shapely.box(pixel[:, 0], pixel[:, 1], pixel[:, 0] + 1, pixel[:, 1] + 1),
        )
    )
    np.add.at(image, (pixel[:, 1], pixel[:, 0]), colour[owners] * overlap[:, None])
    return image, {
        "source_cells": len(x),
        "projected_vertices": 4 * len(x),
        "inside_pixel_cells": int(whole.sum()),
        "clipped_cell_pixel_pairs": len(owners),
        "projected_cell_coordinate_bytes": xy.nbytes,
        "gathered_premultiplied_cell_bytes": colour.nbytes,
        "pair_index_bytes": owners.nbytes + pixel.nbytes,
        "output_bytes": image.nbytes,
    }


def mask_integrals(field, grid, view, plane):
    validate(field, view)
    alpha = field[:, :, 3]
    if np.any((alpha != 0) & (alpha != 1)):
        raise ValueError("Binary point coverage required for opaque polygon union")
    if not np.any(alpha):
        return np.zeros((view.height, view.width)), {
            "source_cells": 0,
            "source_row_runs": 0,
        }
    changes = np.diff(np.pad(alpha.astype(np.int8), ((0, 0), (1, 1))), axis=1)
    y, x0 = np.nonzero(changes == 1)
    y1, x1 = np.nonzero(changes == -1)
    if not np.array_equal(y, y1):
        raise ValueError("Complete source mask row runs required")
    source = shapely.union_all(shapely.box(x0, y, x1, y + 1))
    if source.area != float(alpha.sum()):
        raise ValueError("Source row runs changed occupied-cell area")
    screen = shapely.transform(
        source, lambda xy: project_coordinates(xy, len(field), grid, view, plane)
    )
    area, work = direct_areas(
        screen, np.arange(view.width + 1), np.arange(view.height + 1)
    )
    work.update(
        source_row_runs=len(y),
        source_cells=int(alpha.sum()),
        source_polygon_coordinates=int(shapely.get_num_coordinates(source)),
        area_sum_residual=float(area.sum() - screen.area),
    )
    return area, work
