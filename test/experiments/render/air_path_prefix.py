"""Monotone finite-air prefix with shared local coefficients, for #206/#193.

The Python proof stores cells explicitly. A native implementation can stream the same
cells and retain only the current endpoints; this file makes no GPU-memory claim.
"""

from dataclasses import dataclass

import numpy as np
from air_path_depth_table import depth_nodes
from air_path_integral import quadrature, source
from air_path_segments import view_edges


@dataclass(frozen=True)
class AirCells:
    edges: np.ndarray
    extinction: np.ndarray
    emission: np.ndarray
    source_evaluations: int


def build_cells(medium, arguments, distance_km, count, interpolation="linear"):
    physical = view_edges(medium, *arguments, distance_km)
    edges = np.unique(
        np.concatenate([physical, depth_nodes(count + 1, distance_km, "quadratic")])
    )
    if interpolation == "constant":
        positions = (edges[:-1] + edges[1:]) * 0.5
        extinction, emission = source(medium, *arguments, positions)
        return AirCells(
            edges,
            np.repeat(extinction[:, None], 2, axis=1),
            np.repeat(emission[:, None], 2, axis=1),
            len(positions),
        )
    positions = (
        np.stack([edges[:-1], (edges[:-1] + edges[1:]) * 0.5, edges[1:]], axis=1)
        if interpolation == "quadratic"
        else np.stack([edges[:-1], edges[1:]], axis=1)
    )
    offset = np.minimum(1e-6, np.diff(edges) * 1e-6)
    positions[:, 0] += np.isin(positions[:, 0], physical) * offset
    positions[:, -1] -= np.isin(positions[:, -1], physical) * offset
    unique, indices = np.unique(positions, return_inverse=True)
    extinction, emission = source(medium, *arguments, unique)
    shape = (len(edges) - 1, positions.shape[1], 3)
    extinction, emission = (
        extinction[indices].reshape(shape),
        emission[indices].reshape(shape),
    )
    if interpolation == "quadratic":
        for coefficients in (extinction, emission):
            coefficients[:, 1] = np.maximum(
                0.0,
                2.0 * coefficients[:, 1]
                - 0.5 * (coefficients[:, 0] + coefficients[:, 2]),
            )
    return AirCells(
        edges,
        extinction,
        emission,
        len(unique),
    )


def coefficient_value(controls, fraction):
    unit = np.asarray(fraction)[..., None]
    if len(controls) == 2:
        return controls[0] * (1.0 - unit) + controls[1] * unit
    return (
        controls[0] * (1.0 - unit) ** 2
        + 2.0 * controls[1] * unit * (1.0 - unit)
        + controls[2] * unit**2
    )


def optical_depth(controls, fraction, width):
    unit = np.asarray(fraction)[..., None]
    if len(controls) == 2:
        return width * (
            controls[0] * (unit - 0.5 * unit**2) + controls[1] * 0.5 * unit**2
        )
    return width * (
        controls[0] * (unit - unit**2 + unit**3 / 3.0)
        + controls[1] * (unit**2 - 2.0 * unit**3 / 3.0)
        + controls[2] * unit**3 / 3.0
    )


def cell_integral(cells, index, distance_km):
    width = cells.edges[index + 1] - cells.edges[index]
    nodes, weights = quadrature(4)
    positions = nodes * distance_km
    extinction = cells.extinction[index]
    emission = cells.emission[index]
    depth = optical_depth(extinction, positions / width, width)
    light = coefficient_value(emission, positions / width)
    weighted = weights[:, None] * np.exp(-(depth - depth[0]))
    numerator = np.sum(light * weighted, axis=0)
    denominator = np.sum(
        coefficient_value(extinction, positions / width) * weighted, axis=0
    )
    total_depth = optical_depth(extinction, distance_km / width, width)
    opacity = -np.expm1(-total_depth)
    radiance = np.divide(
        opacity * numerator,
        denominator,
        out=distance_km * numerator,
        where=denominator > 0.0,
    )
    through = np.exp(-total_depth)
    return radiance, through


class AirPrefix:
    def __init__(self, cells):
        self.cells = cells
        self.distance_km = 0.0
        self.cell = 0
        self.radiance_at_start = np.zeros(3)
        self.through_at_start = np.ones(3)
        self.partial_integrals = 0

    def advance(self, distance_km):
        if not self.distance_km <= distance_km <= self.cells.edges[-1]:
            raise ValueError(
                "prefix distances must be finite, monotone and inside the declared ray"
            )
        self.distance_km = distance_km
        while (
            self.cell + 1 < len(self.cells.edges) - 1
            and distance_km >= self.cells.edges[self.cell + 1]
        ):
            width = self.cells.edges[self.cell + 1] - self.cells.edges[self.cell]
            radiance, through = cell_integral(self.cells, self.cell, width)
            self.radiance_at_start += self.through_at_start * radiance
            self.through_at_start *= through
            self.cell += 1
            self.partial_integrals += 1
        distance = distance_km - self.cells.edges[self.cell]
        radiance, through = cell_integral(self.cells, self.cell, distance)
        self.partial_integrals += distance > 0.0
        return (
            self.radiance_at_start + self.through_at_start * radiance,
            self.through_at_start * through,
        )
