"""Authored mature oak growth space and cubic main limbs; no species admission."""

from dataclasses import dataclass

import numpy as np
from colonization import pipe_radii
from crown_attractors import attractors
from outshine_ref.vegetation.spine_curve import CubicSpine


@dataclass(frozen=True)
class OakRecipe:
    seed: int = 682
    height_m: float = 18.0
    spread_m: float = 14.0
    root_radius_m: float = 0.325
    terminal_radius_m: float = 0.0015
    step_m: float = 0.28
    attractor_count: int = 4096
    leaves: int = 90000

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or type(self.attractor_count) is not int
            or not 32 <= self.attractor_count <= 8192
            or type(self.leaves) is not int
            or not 1 <= self.leaves <= 100000
            or not np.isfinite(
                (
                    self.height_m,
                    self.spread_m,
                    self.root_radius_m,
                    self.terminal_radius_m,
                    self.step_m,
                )
            ).all()
            or not 5 <= self.height_m <= 35
            or not 4 <= self.spread_m <= 25
            or not 0.1 <= self.root_radius_m <= 0.75
            or not 0.0005 <= self.terminal_radius_m <= 0.01
            or self.terminal_radius_m >= self.root_radius_m
            or not 0.1 <= self.step_m <= 0.5
        ):
            raise ValueError("bounded explicit authored oak dimensions required")

    def growth_space(self):
        return attractors(self.attractor_count, self.seed) * [
            self.spread_m / 5,
            self.spread_m / 5,
            self.height_m / 8,
        ]

    def skeleton(self, limbs=True):
        if type(limbs) is not bool:
            raise ValueError("explicit scaffold or trunk control required")
        h, w, step = self.height_m, self.spread_m, self.step_m
        trunk = CubicSpine(
            (
                (
                    (0, 0, 0),
                    (0, 0, h * 0.2),
                    (0.2, -0.12, h * 0.4),
                    (0.3, 0.05, h * 0.55),
                ),
            )
        )
        count = (
            int(
                np.ceil(
                    3
                    * np.linalg.norm(np.diff(trunk.controls[0], axis=0), axis=1).max()
                    / step
                )
            )
            + 1
        )
        positions = list(trunk.evaluate(np.linspace(0, 1, count)))
        parents = list(range(-1, count - 1))
        if limbs:
            rng = np.random.Generator(np.random.PCG64(self.seed))
            phase = rng.uniform(0, 2 * np.pi)
            for i, fraction in enumerate((0.36, 0.45, 0.53, 0.63, 0.76, 0.91)):
                host = int(np.rint(fraction * (count - 1)))
                azimuth = (
                    phase + i * np.pi * (3 - np.sqrt(5)) + rng.uniform(-0.22, 0.22)
                )
                radial = np.array([np.cos(azimuth), np.sin(azimuth), 0])
                side = np.cross([0, 0, 1], radial)
                reach = w * rng.uniform(0.24, 0.34) * (1 - 0.035 * i)
                rise = h * rng.uniform(0.12, 0.20)
                origin = positions[host]
                controls = [
                    origin,
                    origin + radial * reach * 0.3 + [0, 0, rise * 0.08],
                    origin
                    + radial * reach * 0.9
                    + side * reach * 0.12
                    + [0, 0, rise * 0.55],
                    origin + radial * reach + [0, 0, rise],
                ]
                spine = CubicSpine((controls,))
                # The derivative control hull bounds every parameter interval length.
                segments = int(
                    np.ceil(
                        3
                        * np.linalg.norm(np.diff(controls, axis=0), axis=1).max()
                        / step
                    )
                )
                points = spine.evaluate(np.linspace(0, 1, segments + 1))[1:]
                first = len(positions)
                parents.extend([host, *range(first, first + len(points) - 1)])
                positions.extend(points)
        return np.array(positions, float), np.array(parents, np.int32)


def tapered_radii(parents, root_radius, terminal_radius):
    """Authored support-power taper with fixed endpoint radii, not pipe conservation."""
    if (
        not np.isfinite([root_radius, terminal_radius]).all()
        or not 0 < terminal_radius < root_radius <= 1
    ):
        raise ValueError("positive decreasing physical radius endpoints required")
    pipes = pipe_radii(parents, 1.0)
    minimum = float(pipes.min())
    if minimum >= 1:
        raise ValueError("branching support required for two distinct radius endpoints")
    exponent = np.log(terminal_radius / root_radius) / np.log(minimum)
    return root_radius * pipes**exponent
