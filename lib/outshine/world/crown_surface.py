"""Solve coupled crown heights by world vertex, independently of the view crop."""

from collections import defaultdict, deque
from dataclasses import dataclass, field

import numpy as np
from scipy.optimize import linprog
from scipy.sparse import coo_matrix, vstack

from outshine.transport.crown_constraints import CrownConstraints

HEIGHT_TOLERANCE = 1e-8
GRADE_TOLERANCE = 1e-8


@dataclass(slots=True)
class CrownPatch:
    prior: np.ndarray
    target: np.ndarray
    constraints: CrownConstraints | None
    incidence: dict = field(init=False)

    def __post_init__(self):
        self.incidence = defaultdict(list)
        if self.constraints is not None:
            for row, column in zip(
                *np.nonzero(self.constraints.coefficients), strict=True
            ):
                self.incidence[int(self.constraints.vertices[row, column])].append(row)


class CrownSurface:
    def __init__(self, tiles, cells, patch):
        self.tiles = tiles
        self.cells = cells
        self.stride = cells + 1
        self.period = tiles * cells
        self.patch = patch
        self.heights = {}

    def _vertex(self, tile, index):
        row, column = divmod(index, self.stride)
        return (
            (tile[1] * self.cells + column) % self.period,
            (tile[2] + 1) * self.cells - row,
        )

    def _incident_tiles(self, zoom, vertex):
        east, south = vertex
        x, column = divmod(east, self.cells)
        y, row = divmod(south, self.cells)
        xs = (x,) if column else ((x - 1) % self.tiles, x)
        ys = (y,) if row else (y - 1, y)
        return sorted((zoom, i, j) for i in xs for j in ys if 0 <= j < self.tiles)

    def _local_index(self, tile, vertex):
        x, y = vertex
        row = (tile[2] + 1) * self.cells - y
        column = (x - tile[1] * self.cells) % self.period
        return row * self.stride + column

    def _component(self, seed, tile):
        zoom, x, y = tile
        queue = deque((seed,))
        trials = {seed: None}
        rows = set()
        offsets = {(0, 0)}
        closed = True
        while queue:
            if len(trials) > self.stride**2:
                return trials, rows, False
            vertex = queue.popleft()
            for key in self._incident_tiles(zoom, vertex):
                _, east, south = key
                offset = (east - x + self.tiles // 2) % self.tiles - self.tiles // 2
                position = (offset, south - y)
                if position not in offsets:
                    offsets.add(position)
                    columns, lines = zip(*offsets, strict=True)
                    if max(columns) - min(columns) > 1 or max(lines) - min(lines) > 1:
                        return trials, rows, False
                patch = self.patch(key)
                if patch is None:
                    closed = False
                    continue
                node = self._local_index(key, vertex)
                with_prior = (patch.prior.flat[node], patch.target.flat[node])
                expected = trials[vertex]
                if expected is None:
                    trials[vertex] = with_prior
                elif (
                    max(abs(a - b) for a, b in zip(expected, with_prior, strict=True))
                    > HEIGHT_TOLERANCE
                ):
                    closed = False
                for row in patch.incidence.get(node, ()):
                    if (key, row) in rows:
                        continue
                    rows.add((key, row))
                    if len(rows) > 2 * self.stride**2:
                        return trials, rows, False
                    constraint = patch.constraints
                    for index, value in zip(
                        constraint.vertices[row],
                        constraint.coefficients[row],
                        strict=True,
                    ):
                        if value:
                            other = self._vertex(key, int(index))
                            if other not in trials:
                                trials[other] = None
                                queue.append(other)
        return trials, rows, closed

    def _fit_component(self, trials, rows):
        vertices = sorted(trials)
        prior, target = np.array([trials[key] for key in vertices]).T
        if not rows:
            return zip(vertices, target, strict=True)
        indices = {key: i for i, key in enumerate(vertices)}
        rows = sorted(rows)
        lower, upper = np.empty(len(rows)), np.empty(len(rows))
        row_ids, columns, values = [], [], []
        for number, (tile, row) in enumerate(rows):
            constraint = self.patch(tile).constraints
            lower[number] = constraint.lower[row]
            upper[number] = constraint.upper[row]
            for node, value in zip(
                constraint.vertices[row], constraint.coefficients[row], strict=True
            ):
                if value:
                    row_ids.append(number)
                    columns.append(indices[self._vertex(tile, int(node))])
                    values.append(value)
        matrix = coo_matrix(
            (values, (row_ids, columns)), shape=(len(rows), len(vertices))
        ).tocsr()
        rise = np.asarray(matrix.sum(axis=1)).ravel()
        if np.all(
            (rise >= lower - GRADE_TOLERANCE) & (rise <= upper + GRADE_TOLERANCE)
        ):
            return zip(vertices, target, strict=True)
        delta = target - prior
        fit = linprog(
            -abs(delta),
            A_ub=vstack((matrix, -matrix), format="csc"),
            b_ub=np.r_[upper, -lower],
            bounds=(0, 1),
            options={
                "maxiter": 8 * len(vertices),
                "primal_feasibility_tolerance": 1e-9,
            },
            method="highs",
        )
        if fit.success:
            blend = np.clip(fit.x, 0.0, 1.0)
            rise = matrix @ blend
            if np.all(
                (rise >= lower - GRADE_TOLERANCE) & (rise <= upper + GRADE_TOLERANCE)
            ):
                return zip(vertices, prior + blend * delta, strict=True)
        return dict.fromkeys(vertices).items()

    def fit(self, tile, patch):
        result = patch.target.copy()
        changed = result != patch.prior
        border = np.zeros_like(changed)
        border[[0, -1], :] = changed[[0, -1], :]
        border[:, [0, -1]] = changed[:, [0, -1]]
        nodes = set(np.flatnonzero(border)) | patch.incidence.keys()
        for index in sorted(nodes):
            vertex = self._vertex(tile, int(index))
            if vertex not in self.heights:
                trials, rows, closed = self._component(vertex, tile)
                if closed:
                    self.heights.update(self._fit_component(trials, rows))
                else:
                    self.heights.update(dict.fromkeys(trials))
            height = self.heights[vertex]
            result.flat[index] = patch.prior.flat[index] if height is None else height
        return result
