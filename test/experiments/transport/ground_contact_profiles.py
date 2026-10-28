"""Bounded exact negative witnesses between whole fixed affine contact edges.

On a convex patch, a continuous piecewise affine field whose triangle gradients
all have norm <=G is G-Lipschitz. A fixed road/outer-edge point pair with
dz² > G²(dx²+dy²) contradicts such a field only when the road has that bound too.
Floating quadratic extrema propose witnesses; exact dyadic/rational arithmetic
alone proves the strict inequality and road condition. A missed witness, an
unproven premise or a budget end yields no conclusion. Construction and final
conforming coverage checks remain mandatory. Callers supply the convex outer
patch and an immutable qualified road; this is not a general feasibility oracle.
"""

from fractions import Fraction

import numpy as np
from outshine_ref.transport.surface import _sorted_boundary


def proposed_witnesses(first, second, grade):
    delta = first[:, 0] - second[:, 0]
    s, t = first[:, 1] - first[:, 0], second[:, 0] - second[:, 1]

    def product(a, b):
        return a[:, 2] * b[:, 2] - grade**2 * np.einsum("ij,ij->i", a[:, :2], b[:, :2])

    a, b, c = product(s, s), 2 * product(s, t), product(t, t)
    d, e, f = 2 * product(delta, s), 2 * product(delta, t), product(delta, delta)
    count = len(a)
    pairs = [
        (np.full(count, x), np.full(count, y))
        for x, y in ((0.0, 0.0), (0.0, 1.0), (1.0, 0.0), (1.0, 1.0))
    ]
    with np.errstate(divide="ignore", invalid="ignore"):
        pairs.extend(
            (
                (np.zeros(count), np.clip(-e / (2 * c), 0, 1)),
                (np.ones(count), np.clip(-(b + e) / (2 * c), 0, 1)),
                (np.clip(-d / (2 * a), 0, 1), np.zeros(count)),
                (np.clip(-(b + d) / (2 * a), 0, 1), np.ones(count)),
            )
        )
        determinant = 4 * a * c - b * b
        x, y = (b * e - 2 * c * d) / determinant, (b * d - 2 * a * e) / determinant
        inside = (x >= 0) & (x <= 1) & (y >= 0) & (y <= 1)
        pairs.append((np.where(inside, x, 0), np.where(inside, y, 0)))
        values = np.array(
            [a * x * x + b * x * y + c * y * y + d * x + e * y + f for x, y in pairs]
        )
    values = np.nan_to_num(values, nan=-np.inf, posinf=-np.inf, neginf=-np.inf)
    best = values.argmax(axis=0)
    maximum = values[best, np.arange(count)]
    scale = 1 + abs(a) + abs(b) + abs(c) + abs(d) + abs(e) + abs(f)
    # This only selects possible witnesses for exact checking. It grants no
    # membership/feasibility tolerance or negative proof by itself.
    selected = np.flatnonzero(maximum >= -128 * np.finfo(float).eps * scale)
    selected = selected[np.argsort(-maximum[selected], kind="stable")]
    return [
        (int(i), float(pairs[best[i]][0][i]), float(pairs[best[i]][1][i]))
        for i in selected
    ]


def exact_squared_excess(first, second, s, t, grade):
    def point(edge, at):
        ratio = Fraction(at)
        return [
            Fraction(float(a)) + ratio * (Fraction(float(b)) - Fraction(float(a)))
            for a, b in zip(edge[0], edge[1], strict=True)
        ]

    left, right = point(first, s), point(second, t)
    delta = [a - b for a, b in zip(left, right, strict=True)]
    return delta[2] ** 2 - Fraction(grade) ** 2 * (delta[0] ** 2 + delta[1] ** 2)


class WholeProfileGuard:
    def __init__(
        self,
        road,
        grade,
        *,
        maximum_pairs=2048,
        maximum_exact_checks=4,
        maximum_road_faces=256,
    ):
        if (
            not np.isfinite(grade)
            or grade <= 0
            or grade > 1
            or any(
                type(v) is not int or not 0 <= v <= 65536
                for v in (maximum_pairs, maximum_exact_checks, maximum_road_faces)
            )
        ):
            raise ValueError(
                "Finite grade and bounded nonnegative profile proof capacities required"
            )
        self.road, self.grade = road, float(grade)
        self.edges = _sorted_boundary(road.triangles)
        self.maximum_pairs, self.maximum_exact_checks = (
            maximum_pairs,
            maximum_exact_checks,
        )
        self.maximum_road_faces = maximum_road_faces
        self.road_grade_proven = None
        self.work = {
            "calls": 0,
            "edge_pairs": 0,
            "pair_buffer_bytes_written": 0,
            "exact_witness_checks": 0,
            "exact_road_faces": 0,
        }

    def road_condition(self):
        if self.road_grade_proven is not None:
            return self.road_grade_proven
        if len(self.road.triangles) > self.maximum_road_faces:
            return False
        numerator, denominator = self.grade.as_integer_ratio()
        ratios = {
            int(i): [float(v).as_integer_ratio() for v in self.road.vertices[i]]
            for i in np.unique(self.road.triangles)
        }
        if not ratios:
            return False
        scale = max(d for p in ratios.values() for _, d in p)
        # Float denominators are powers of two. One shared integer scale preserves
        # every coordinate exactly and cancels out of the gradient inequality.
        points = {i: [n * (scale // d) for n, d in p] for i, p in ratios.items()}
        for face in self.road.triangles:
            a, b, c = [points[i] for i in face]
            u = [q - p for p, q in zip(a, b, strict=True)]
            v = [q - p for p, q in zip(a, c, strict=True)]
            determinant = u[0] * v[1] - u[1] * v[0]
            x, y = v[1] * u[2] - u[1] * v[2], u[0] * v[2] - v[0] * u[2]
            self.work["exact_road_faces"] += 1
            if (
                determinant == 0
                or (x * x + y * y) * denominator**2 > numerator**2 * determinant**2
            ):
                self.road_grade_proven = False
                return False
        self.road_grade_proven = True
        return True

    def __call__(self, corners, heights):
        self.work["calls"] += 1
        outer = np.c_[corners, heights]
        pairs = len(self.edges) * len(outer)
        empty = {"state": "no_conclusion"}
        if (
            not pairs
            or pairs > self.maximum_pairs
            or not self.maximum_exact_checks
            or not len(outer)
        ):
            return empty | {"reason": "profile_work_budget"}
        if (
            np.max(abs(self.road.vertices)) > 1e9
            or np.max(abs(outer)) > 1e9
            or not np.isfinite(outer).all()
        ):
            return empty | {"reason": "profile_numeric_domain"}
        self.work["edge_pairs"] += pairs
        first = np.repeat(self.road.vertices[self.edges], len(outer), axis=0)
        second = np.tile(
            np.stack((outer, np.roll(outer, -1, axis=0)), axis=1),
            (len(self.edges), 1, 1),
        )
        self.work["pair_buffer_bytes_written"] += first.nbytes + second.nbytes
        for index, s, t in proposed_witnesses(first, second, self.grade)[
            : self.maximum_exact_checks
        ]:
            self.work["exact_witness_checks"] += 1
            excess = exact_squared_excess(first[index], second[index], s, t, self.grade)
            if excess <= 0:
                continue
            if not self.road_condition():
                return empty | {"reason": "uniform_road_gradient_not_proven"}
            return {
                "state": "contradiction",
                "road_edge": self.edges[index // len(outer)].tolist(),
                "outer_edge": int(index % len(outer)),
                "parameters": (s, t),
                "exact_squared_excess_m2": str(excess),
                "squared_excess_m2": float(excess),
                "effective_grade": self.grade,
                "proof": "Exact rational fixed affine profile-point contradiction, with exact physical grade bound on every supplied road triangle; convex whole patch required. Not a complete feasibility/minimum-domain certificate.",
            }
        return empty
