"""Bounded adaptive positive residual ranges, not a rigorous Float64 integrator."""

import heapq
import math
from collections import Counter
from dataclasses import dataclass

import numpy as np
from relief_finite_kernel import kernel_moments, point_response, transform
from relief_lit_kernel import lit_polygon
from shapely.geometry import Polygon, box
from shapely.geometry.polygon import orient


def square_range(low, high):
    return (
        0.0 if low <= 0 <= high else min(low * low, high * high),
        max(low * low, high * high),
    )


def cosine_range(vertices, direction, norm):
    """Linear numerator extrema are at corners; the positive norm is bounded."""
    values = direction[2] - vertices @ direction[:2]
    low, high = float(values.min()), float(values.max())
    endpoints = [low / norm[0], low / norm[1], high / norm[0], high / norm[1]]
    return min(endpoints), max(endpoints)


def visibility_times_light(nl, nv, a2):
    """Increasing in nl>=0, decreasing in nv>0, including the nl=0 limit."""
    return (
        0.5
        * nl
        / (
            nl * np.sqrt(nv * nv * (1 - a2) + a2)
            + nv * np.sqrt(nl * nl * (1 - a2) + a2)
        )
    )


def residual_range(vertices, roughness, light, view, half):
    """Real-arithmetic enclosure of response/W, preserving all native guards.

    S=1+q.q, C=Hz-Hxy.q, B=S+(a2-1)*C². For C>=0, D/W=S²;
    for the clamped C<0 half-normal, D/W=B². Nothing discards n.v<0.
    Interval dependency deliberately overestimates variation. Arithmetic/kernel
    rounding is not certified; independent controls are an additional requirement.
    """
    a2 = roughness**4
    low, high = vertices.min(0), vertices.max(0)
    xx, yy = square_range(low[0], high[0]), square_range(low[1], high[1])
    s = (1 + xx[0] + yy[0], 1 + xx[1] + yy[1])
    norm = np.sqrt(s)
    nl = tuple(max(x, 0.0) for x in cosine_range(vertices, light, norm))
    nv = tuple(max(x, 1e-6) for x in cosine_range(vertices, view, norm))
    half_dot = half[2] - vertices @ half[:2]
    if half_dot.min() >= 0:
        factor = s[0] ** 2, s[1] ** 2
    else:
        squared = square_range(float(half_dot.min()), float(half_dot.max()))
        b = (max(0.0, s[0] + (a2 - 1) * squared[1]), s[1] + (a2 - 1) * squared[0])
        factor = b[0] ** 2, b[1] ** 2
        if half_dot.max() > 0:
            factor = min(factor[0], s[0] ** 2), max(factor[1], s[1] ** 2)
    fresnel = 0.04 + 0.96 * (1 - max(float(view @ half), 0.0)) ** 5
    return np.array(
        [
            fresnel * visibility_times_light(nl[0], nv[1], a2) * factor[0],
            fresnel * visibility_times_light(nl[1], nv[0], a2) * factor[1],
        ]
    )


@dataclass(frozen=True)
class Cell:
    vertices: np.ndarray
    density: float
    interval: np.ndarray


def split(vertices):
    low, high = vertices.min(0), vertices.max(0)
    axis = int(np.argmax(high - low))
    middle = low[axis] + (high[axis] - low[axis]) / 2
    if not low[axis] < middle < high[axis]:
        raise ValueError("unrepresentable_local_split")
    shape, children = Polygon(vertices), []
    for side in range(2):
        lower, upper = low.copy(), high.copy()
        if side:
            lower[axis] = middle
        else:
            upper[axis] = middle
        child = shape.intersection(box(*lower, *upper))
        if child.area == 0:
            continue
        if child.geom_type != "Polygon" or not child.is_valid:
            raise ValueError("unsupported_local_split")
        children.append(np.asarray(orient(child, sign=1).exterior.coords[:-1]))
    if len(children) != 2:
        raise ValueError("nonconservative_local_split")
    return children


def response(
    model, roughness, light, view, *, relative=0.004, leaf_limit=64, history=None
):
    """Midpoint of summed positive model ranges; unproved quality at budget end.

    The criterion uses the current lower integral, never the reference peak.
    It bounds truncation in real arithmetic, not analytic-mass/Float64 error.
    """
    if (
        not np.isfinite(relative)
        or not 0 < relative < 1
        or type(leaf_limit) is not int
        or not 1 <= leaf_limit <= 1024
        or not np.isfinite(roughness)
        or not 0 < roughness <= 1
    ):
        raise ValueError(
            "positive finite tolerance/roughness and bounded leaves required"
        )
    gradient, coverage, corners, _ = model
    if len(corners) > leaf_limit:
        raise ValueError("initial_domain_budget")
    light, view = np.asarray(light, float), np.asarray(view, float)
    if any(x.shape != (3,) or not np.isfinite(x).all() for x in (light, view)):
        raise ValueError("finite three-dimensional directions required")
    if (
        any(abs(np.linalg.norm(x) - 1) > 1e-12 for x in (light, view))
        or np.linalg.norm(light + view) == 0
    ):
        raise ValueError("unit directions and a defined half vector required")
    constant = float(
        coverage @ point_response(gradient, roughness, light[None], view)[:, 0]
    )
    transformed = transform(roughness, light[None], view)
    work = Counter(constant_brdf_evaluations=len(coverage), transform_calls=1)
    heap, next_id = [], 0

    def cell(vertices, density):
        mass, _centroid = kernel_moments(vertices, transformed)
        interval = (
            density
            * mass[0]
            * residual_range(vertices, roughness, light, view, transformed[-1][0])
        )
        if not np.isfinite(interval).all() or not 0 <= interval[0] <= interval[1]:
            raise ValueError("invalid_local_range")
        work.update(
            range_cells=1,
            kernel_moment_queries=1,
            kernel_edge_visits=len(vertices),
            smith_square_roots=4,
            cell_vertex_bytes_visited=vertices.nbytes,
            weighted_interval_arrays_created=1,
        )
        return Cell(vertices, density, interval)

    def push(node):
        nonlocal next_id
        heapq.heappush(heap, (-float(np.diff(node.interval)[0]), next_id, node))
        next_id += 1

    for vertices, density in corners:
        clipped, state = lit_polygon(vertices, light)
        work.update(support_polygons=1, support_vertex_tests=len(vertices))
        work[state] += 1
        if len(clipped):
            push(cell(clipped, density))
    while True:
        interval = np.array(
            [
                constant + math.fsum(item[2].interval[k] for item in heap)
                for k in range(2)
            ]
        )
        work["summed_active_terms"] += 2 * len(heap)
        value = float((interval[0] + interval[1]) / 2)
        radius = float((interval[1] - interval[0]) / 2)
        met = radius <= relative * interval[0]
        if history is not None:
            history.append((len(heap), value, float(interval[0]), float(interval[1])))
        if met or len(heap) == leaf_limit or not heap:
            return (
                value,
                interval,
                dict(
                    work,
                    leaves=len(heap),
                    model_tolerance_met=bool(met),
                    active_numeric_array_bytes=sum(
                        n.vertices.nbytes + n.interval.nbytes for _, _, n in heap
                    ),
                ),
            )
        _width, _id, selected = heapq.heappop(heap)
        children = split(selected.vertices)
        work.update(splits=1, polygon_intersections=2)
        for child in children:
            work["split_coordinate_payload_bytes"] += child.nbytes
            push(cell(child, selected.density))
