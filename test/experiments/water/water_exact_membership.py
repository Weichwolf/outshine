"""Bounded rational vertical slabs for triangle/source overlap and union coverage.

No vertex moves: arithmetic refers to the supplied finite binary64 coordinates.
Polygon rings must describe the caller's valid simple polygon/multipolygon input.
"""

from dataclasses import dataclass, field
from fractions import Fraction
from itertools import chain, pairwise


class ExactBudgetExceeded(ValueError):
    pass


@dataclass
class ExactBudget:
    limit: int = 100000
    max_bits: int = 4096
    max_edges: int = 512
    operations: int = field(default=0, init=False)
    peak_bits: int = field(default=0, init=False)
    counts: dict = field(default_factory=dict, init=False)

    def __post_init__(self):
        if any(
            type(v) is not int or not low <= v <= high
            for v, low, high in (
                (self.limit, 0, 10000000),
                (self.max_bits, 64, 16384),
                (self.max_edges, 3, 4096),
            )
        ):
            raise ValueError(
                "bounded exact operation, rational bit and active-edge limits required"
            )

    def tick(self, name):
        if self.operations >= self.limit:
            raise ExactBudgetExceeded("exact operation budget")
        self.operations += 1
        self.counts[name] = self.counts.get(name, 0) + 1

    def check(self, value):
        bits = max(value.numerator.bit_length(), value.denominator.bit_length())
        self.peak_bits = max(self.peak_bits, bits)
        if bits > self.max_bits:
            raise ExactBudgetExceeded("exact rational bit budget")
        return value


def _point(point, origin, budget):
    budget.tick("input_points")
    return tuple(
        budget.check(Fraction(float(v)) - o)
        for v, o in zip(point[:2], origin, strict=True)
    )


def _edges(rings, owner, origin, low, high, budget, capacity):
    result = []
    for ring in rings:
        points = [_point(p, origin, budget) for p in ring]
        for a, b in pairwise(points):
            if a[0] == b[0]:
                continue
            first, last = sorted((a[0], b[0]))
            first, last = max(first, low), min(last, high)
            if first >= last:
                continue
            if len(result) == capacity:
                raise ExactBudgetExceeded("exact active-edge budget")
            slope = budget.check((b[1] - a[1]) / (b[0] - a[0]))
            intercept = budget.check(a[1] - slope * a[0])
            budget.tick("active_edges")
            result.append((first, last, slope, intercept, owner))
    return result


def _value(edge, x, budget):
    budget.tick("line_evaluations")
    return budget.check(edge[2] * x + edge[3])


def _events(edges, low, high, budget):
    events = {low, high}
    for edge in edges:
        events.update(edge[:2])
    for i, a in enumerate(edges):
        for b in edges[i + 1 :]:
            budget.tick("edge_pairs")
            first, last = max(a[0], b[0]), min(a[1], b[1])
            if first >= last or a[2] == b[2]:
                continue
            x = budget.check((b[3] - a[3]) / (a[2] - b[2]))
            if first < x < last:
                events.add(x)
    return sorted(events)


def _crossings(edges, x, owners, budget):
    rows = [[] for _ in range(owners + 1)]
    for edge in edges:
        budget.tick("edge_slab_tests")
        if edge[0] < x < edge[1]:
            rows[edge[4] + 1].append((_value(edge, x, budget), edge))
    for row in rows:
        row.sort(key=lambda p: p[0])
        if len(row) % 2:
            raise ValueError("exact slice has unmatched ring crossings")
    return rows


def _clipped_intervals(crossings, query):
    low, high = query
    result = []
    for a, b in zip(crossings[::2], crossings[1::2], strict=True):
        a = max(a, low, key=lambda p: p[0])
        b = min(b, high, key=lambda p: p[0])
        if a[0] < b[0]:
            result.append((a, b))
    return result


def _integral(interval, left, right, budget):
    budget.tick("interval_integrals")
    a, b = interval
    heights = sum(
        (_value(b[1], x, budget) - _value(a[1], x, budget) for x in (left, right)),
        Fraction(0),
    )
    return budget.check((right - left) * heights / 2)


def _union(intervals, budget):
    intervals.sort(key=lambda p: p[0][0])
    result = []
    for low, high in intervals:
        budget.tick("union_intervals")
        if not result or low[0] > result[-1][1][0]:
            result.append((low, high))
        elif high[0] > result[-1][1][0]:
            result[-1] = (result[-1][0], high)
    return result


def exact_overlap(triangle, sources, budget):
    """Return exact per-source areas and uncovered triangle area, or refuse.

    All segment endpoints and pair crossings partition x into open slabs.
    Edge order is constant in each slab. Even/odd ring slices preserve holes;
    clipped/union interval lengths are affine there, so trapezoids integrate
    exactly. Source overlap is counted per source; union coverage only once.
    Operation and bit caps apply to this finite fallback, not GEOS/STRtree work.
    """
    if len(triangle) != 3 or len(sources) > 4096:
        raise ValueError("one triangle and bounded original sources required")
    origin = tuple(budget.check(Fraction(float(v))) for v in triangle[0][:2])
    tri = [_point(p, origin, budget) for p in triangle]
    area = (
        abs(
            sum(
                (a[0] * b[1] - b[0] * a[1] for a, b in pairwise([*tri, tri[0]])),
                Fraction(0),
            )
        )
        / 2
    )
    area = budget.check(area)
    if not area:
        raise ValueError("positive exact triangle required")
    low, high = min(p[0] for p in tri), max(p[0] for p in tri)
    edges = _edges(
        [[*triangle, triangle[0]]], -1, origin, low, high, budget, budget.max_edges
    )
    for owner, source in enumerate(sources):
        parts = (source,) if source.geom_type == "Polygon" else source.geoms
        for part in parts:
            if part.geom_type != "Polygon":
                raise ValueError("original polygons required")
            edges.extend(
                _edges(
                    chain((part.exterior.coords,), (r.coords for r in part.interiors)),
                    owner,
                    origin,
                    low,
                    high,
                    budget,
                    budget.max_edges - len(edges),
                )
            )
    events = _events(edges, low, high, budget)
    areas, covered = [Fraction(0) for _ in sources], Fraction(0)
    for left, right in pairwise(events):
        budget.tick("slabs")
        crossings = _crossings(
            edges, budget.check((left + right) / 2), len(sources), budget
        )
        if len(crossings[0]) != 2:
            raise ValueError("positive triangle slice required")
        intervals = []
        for owner, row in enumerate(crossings[1:]):
            budget.tick("source_slabs")
            clipped = _clipped_intervals(row, crossings[0])
            for interval in clipped:
                areas[owner] = budget.check(
                    areas[owner] + _integral(interval, left, right, budget)
                )
            intervals.extend(clipped)
        for interval in _union(intervals, budget):
            covered = budget.check(covered + _integral(interval, left, right, budget))
    if not 0 <= covered <= area or any(not 0 <= a <= area for a in areas):
        raise ValueError("inconsistent exact slice areas")
    return tuple(areas), budget.check(area - covered)
