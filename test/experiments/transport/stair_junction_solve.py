"""Bounded finite-facet LP; polygonal gradient constraints imply exact norm limits."""

import itertools
import time

import numpy as np
import shapely
from scipy import sparse
from scipy.optimize import linprog


def gradient_rows(xy, face):
    points = xy[face]
    inverse = np.linalg.inv(points[1:] - points[0])
    return inverse @ np.array([[-1, 1, 0], [-1, 0, 1]])


class Conditions:
    def __init__(self, variables):
        self.variables = variables
        self.rows, self.columns, self.values, self.bounds = [], [], [], []

    def add(self, columns, values, bound):
        self.rows.extend([len(self.bounds)] * len(columns))
        self.columns.extend(columns)
        self.values.extend(values)
        self.bounds.append(bound)

    def equal(self, columns, values, bound=0):
        self.add(columns, values, bound)
        self.add(columns, -np.array(values), -bound)

    def matrix(self):
        return sparse.csc_matrix(
            (self.values, (self.rows, self.columns)),
            shape=(len(self.bounds), self.variables),
        )


def facets(plan, system, directions, reserve, flat, precision):
    seen = set()
    cast_xy = plan["xy"].astype(np.float32).astype(float)
    epsilon = float(np.spacing(np.float32(plan["rows"][0]["prior"][0] + 10))) / 2
    for owner, (row, faces) in enumerate(zip(plan["rows"], plan["parts"], strict=True)):
        limit = row["gradient"] - (0.002 if row["gradient"] > 0.1 else 0.0002)
        for face in faces:
            key = (tuple(face), limit)
            if key in seen:
                continue
            seen.add(key)
            coefficients = directions @ gradient_rows(plan["xy"], face)
            for values in coefficients:
                system.add(face, values, limit * reserve)
            landing = flat and any(
                owner in owners
                and np.all(
                    shapely.dwithin(mask, shapely.points(plan["xy"][face]), 1e-8)
                )
                for owners, mask in zip(plan["incident"], plan["masks"], strict=True)
            )
            if precision and owner != 0 and not landing:
                quantized = directions @ gradient_rows(cast_xy, face)
                for values in quantized:
                    bound = row["gradient"] * reserve - epsilon * float(
                        abs(values).sum()
                    )
                    if bound < 0:
                        raise ValueError(
                            "cycle face requires a less ill-conditioned precision mesh"
                        )
                    system.add(face, values, bound)


def junctions(plan, system, directions, reserve, flat):
    count = len(plan["xy"])
    for j, (node, owners, mask) in enumerate(
        zip(plan["nodes"], plan["incident"], plan["masks"], strict=True)
    ):
        indices = np.unique(np.concatenate([plan["parts"][i].ravel() for i in owners]))
        indices = indices[
            shapely.dwithin(mask, shapely.points(plan["xy"][indices]), 1e-8)
        ]
        h, east, north = 2 * count + 3 * j + np.arange(3)
        for i in indices:
            xy = plan["xy"][i] - node
            system.equal([i, h, east, north], [1, -1, -xy[0], -xy[1]])
        for direction in directions:
            system.add([east, north], direction, 0 if flat else 0.0598 * reserve)


def overlap_conditions(plan, system, gap):
    mapped = [
        {int(k): f for k, f in zip(keys, faces, strict=True)}
        for keys, faces in zip(plan["part_global_faces"], plan["parts"], strict=True)
    ]
    checks = 0
    for i, j in itertools.combinations(range(4), 2):
        for key in mapped[i].keys() & mapped[j].keys():
            a, b = mapped[i][key], mapped[j][key]
            if np.array_equal(a, b):
                continue
            if (i, j) != (1, 2):
                raise ValueError("unplanned non-contact overlap between source bands")
            for low, high in zip(a, b, strict=True):
                system.add([low, high], [1, -1], -gap)
                checks += 1
    return checks


def stair_conditions(plan, system):
    count = len(plan["xy"])
    face = plan["parts"][0]
    low, high = 2 * count, 2 * count + 3
    for i in np.unique(face):
        system.add([low, i], [1, -1], 0)
        system.add([i, high], [1, -1], 0)
    row = plan["rows"][0]
    shapes = shapely.polygons(plan["xy"][face])
    for a, b in itertools.pairwise(row["xy"]):
        tangent = (b - a) / np.linalg.norm(b - a)
        influence = shapely.LineString([a, b]).buffer(row["width_m"])
        for index in np.flatnonzero(
            shapely.area(shapely.intersection(shapes, influence)) > 1e-10
        ):
            f = face[index]
            system.add(f, -tangent @ gradient_rows(plan["xy"], f), 0)


def solve(
    plan,
    *,
    flat=True,
    directions=16,
    seconds=5,
    gap_m=2.82,
    fixed_port_heights=False,
    precision=True,
):
    began = time.perf_counter()
    xy, count = plan["xy"], len(plan["xy"])
    if count > 4096 or sum(map(len, plan["parts"])) > 8192:
        raise ValueError("four-way finite surface budget")
    angles = np.arange(directions) * 2 * np.pi / directions
    normals = np.c_[np.cos(angles), np.sin(angles)]
    reserve = float(np.cos(np.pi / directions))
    system = Conditions(2 * count + 6)
    origin_z = plan["rows"][0]["prior"][0]
    priors = []
    for point, owner in zip(xy, plan["primary_source"], strict=True):
        row = plan["rows"][owner]
        line = shapely.LineString(row["xy"])
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(row["xy"], axis=0), axis=1))]
        priors.append(
            float(
                np.interp(line.project(shapely.Point(point)), arc, row["prior"])
                - origin_z
            )
        )
    for i, prior in enumerate(priors):
        system.add([i, count + i], [1, -1], prior)
        system.add([i, count + i], [-1, -1], -prior)
    facets(plan, system, normals, reserve, flat, precision)
    junctions(plan, system, normals, reserve, flat)
    stair_conditions(plan, system)
    overlaps = overlap_conditions(plan, system, gap_m)
    anchors = ((0, 0), (1, -1), (2, 0)) if fixed_port_heights else ((0, 0),)
    for owner, end in anchors:
        row = plan["rows"][owner]
        distance = np.linalg.norm(xy - row["xy"][end], axis=1)
        i = int(np.argmin(distance))
        if distance[i] > 1e-7:
            raise ValueError("actual outer/node height anchor is absent")
        system.equal([i], [1], float(row["prior"][end] - origin_z))
    matrix = system.matrix()
    answer = linprog(
        np.r_[np.zeros(count), np.ones(count), np.zeros(6)],
        A_ub=matrix,
        b_ub=system.bounds,
        bounds=[(-2, 10)] * count + [(0, None)] * count + [(None, None)] * 6,
        method="highs",
        options={"time_limit": seconds, "primal_feasibility_tolerance": 1e-9},
    )
    note = {
        "valid": bool(answer.success),
        "status": answer.message,
        "variables": matrix.shape[1],
        "constraints": matrix.shape[0],
        "nonzeros": matrix.nnz,
        "overlap_vertex_constraints": overlaps,
        "seconds": time.perf_counter() - began,
        "flat_landings": flat,
        "fixed_outer_prior_heights": fixed_port_heights,
        "precision_bounds": precision,
    }
    if not answer.success:
        return None, note
    z = answer.x[:count] + origin_z
    norms = [
        float(
            np.linalg.norm(
                np.array([gradient_rows(xy, f) @ z[f] for f in faces]), axis=1
            ).max()
        )
        for faces in plan["parts"]
    ]
    note.update(
        maximum_gradients=norms,
        junction_planes=answer.x[-6:].reshape(2, 3).tolist(),
        objective=float(answer.fun),
        maximum_residual=float(np.max(matrix @ answer.x - system.bounds)),
    )
    return z, note
