"""Source-only cell areas with direct and recursive rectangle clipping."""

from itertools import pairwise, product

import numpy as np
import shapely


def direct_areas(geometry, x, y):
    lo_x, lo_y = np.meshgrid(x[:-1], y[:-1])
    hi_x, hi_y = np.meshgrid(x[1:], y[1:])
    rectangles = shapely.box(lo_x, lo_y, hi_x, hi_y)
    count = rectangles.size
    return shapely.area(shapely.intersection(rectangles, geometry)), {
        "intersection_tests": count,
        "intersection_input_coordinates": count
        * int(shapely.get_num_coordinates(geometry)),
        "rectangles": count,
        "hierarchy_nodes": 0,
        "covers_tests": 0,
        "area_queries": count,
        "covered_cells": 0,
        "empty_cells": 0,
        "peak_stack_entries": 0,
    }


def hierarchical_areas(geometry, x, y):
    """Intersect each child with its already clipped parent; keep holes intact.

    Input-coordinate counts describe geometry supplied to GEOS, not its internal
    operations, copies or allocator work. Outputs are source statistics only,
    never World empty/ready certificates.
    """
    result = np.zeros((len(y) - 1, len(x) - 1))
    root = shapely.box(x[0], y[0], x[-1], y[-1])
    stack = [(0, 0, len(x) - 1, len(y) - 1, shapely.intersection(geometry, root))]
    work = {
        "intersection_tests": 1,
        "intersection_input_coordinates": int(shapely.get_num_coordinates(geometry)),
        "rectangles": 1,
        "hierarchy_nodes": 0,
        "covers_tests": 0,
        "area_queries": 0,
        "covered_cells": 0,
        "empty_cells": 0,
        "peak_stack_entries": 1,
    }
    while stack:
        a, b, c, d, geometry = stack.pop()
        cells = (c - a) * (d - b)
        work["hierarchy_nodes"] += 1
        if shapely.is_empty(geometry):
            work["empty_cells"] += cells
            continue
        if cells == 1:
            result[b, a] = shapely.area(geometry)
            work["area_queries"] += 1
            continue
        rectangle = shapely.box(x[a], y[b], x[c], y[d])
        work["rectangles"] += 1
        work["covers_tests"] += 1
        if shapely.covers(geometry, rectangle):
            result[b:d, a:c] = np.outer(np.diff(y[b : d + 1]), np.diff(x[a : c + 1]))
            work["covered_cells"] += cells
            continue
        xs = [a, (a + c) // 2, c] if c - a > 1 else [a, c]
        ys = [b, (b + d) // 2, d] if d - b > 1 else [b, d]
        children = np.array(
            [(i, k, j, l) for (i, j), (k, l) in product(pairwise(xs), pairwise(ys))]
        )
        boxes = shapely.box(
            x[children[:, 0]], y[children[:, 1]], x[children[:, 2]], y[children[:, 3]]
        )
        pieces = shapely.intersection(geometry, boxes)
        work["intersection_tests"] += len(children)
        work["intersection_input_coordinates"] += len(children) * int(
            shapely.get_num_coordinates(geometry)
        )
        work["rectangles"] += len(children)
        stack.extend(
            (*map(int, bounds), piece)
            for bounds, piece in zip(children, pieces, strict=True)
        )
        work["peak_stack_entries"] = max(work["peak_stack_entries"], len(stack))
    return result, work


def region_areas(geometry, x, y, method):
    if method == "direct":
        return direct_areas(geometry, x, y)
    if method == "hierarchical":
        return hierarchical_areas(geometry, x, y)
    raise ValueError("direct or hierarchical source-area method required")
