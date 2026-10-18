"""Seeded periodic longitudinal furrows and finite transverse fractures, in metres."""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class FurrowRecipe:
    seed: int = 765
    width_m: float = 0.6
    height_m: float = 1.2
    furrows: int = 18
    fractures: int = 180

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or type(self.furrows) is not int
            or not 4 <= self.furrows <= 64
            or type(self.fractures) is not int
            or not 0 <= self.fractures <= 1024
            or not np.isfinite((self.width_m, self.height_m)).all()
            or not 0.1 <= self.width_m <= 2
            or not 0.2 <= self.height_m <= 4
            or self.width_m / self.furrows < 0.008
        ):
            raise ValueError("bounded metric furrow recipe and uint64 seed required")

    def plan(self):
        rng = np.random.default_rng(self.seed)
        gap = rng.uniform(0.65, 1.35, self.furrows)
        gap *= self.width_m / gap.sum()
        coefficients = rng.uniform(-1, 1, (self.furrows, 5))
        coefficients /= np.sum(abs(coefficients), axis=1)[:, None]
        width = rng.uniform(0.45, 0.95, self.furrows) * min(0.006, gap.min() * 0.22)
        fracture_coefficients = rng.uniform(-1, 1, (self.fractures, 6))
        fracture_coefficients /= np.sum(abs(fracture_coefficients), axis=1)[:, None]
        plan = {
            "centres": np.r_[0, np.cumsum(gap[:-1])],
            "coefficients": coefficients,
            "phases": rng.uniform(0, 2 * np.pi, (self.furrows, 5)),
            "wander_m": gap.min() * 0.22,
            "widths": width,
            "depths": rng.uniform(0.003, 0.007, self.furrows),
            "fracture_coefficients": fracture_coefficients,
            "fractures": np.column_stack(
                (
                    rng.uniform(0, self.width_m, self.fractures),
                    rng.uniform(0, self.height_m, self.fractures),
                    rng.uniform(0.01, 0.05, self.fractures) * self.width_m,
                    rng.uniform(0.0003, 0.0012, self.fractures),
                    rng.uniform(-0.8, 0.8, self.fractures),
                    rng.uniform(-0.0015, 0.0015, self.fractures),
                    rng.uniform(0.0008, 0.0028, self.fractures),
                )
            ),
        }
        for index, fracture in enumerate(plan["fractures"]):
            owner = index % self.furrows
            z = np.array([fracture[1]])
            left = longitudinal(self, plan, owner, z)[0][0]
            right = longitudinal(self, plan, (owner + 1) % self.furrows, z)[0][0]
            gap = (right - left) % self.width_m
            fracture[0] = (left + gap / 2) % self.width_m
            c, s = abs(np.cos(fracture[4])), abs(np.sin(fracture[4]))
            fracture[2] = min(
                gap / (2 * c),
                0.15 * self.width_m / c,
                0.15 * self.height_m / max(s, 0.01),
            )
        return plan


def longitudinal(recipe, plan, index, z):
    angle = 2 * np.pi * z[:, None] / recipe.height_m
    phase = angle * np.array((1, 2, 4, 7, 13)) + plan["phases"][index]
    wave = np.sin(phase) @ plan["coefficients"][index]
    shared = (
        recipe.width_m
        / recipe.furrows
        * 0.2
        * (np.sin(angle[:, 0]) + 0.5 * np.sin(3 * angle[:, 0]))
    )
    centre = plan["centres"][index] + shared + plan["wander_m"] * wave
    width = plan["widths"][index] * (0.75 + 0.25 * np.sin(phase[:, 1]))
    depth = plan["depths"][index] * (0.6 + 0.4 * np.sin(phase[:, 0]))
    return centre, width, depth


def periodic_delta(value, centre, period):
    return (value - centre + period / 2) % period - period / 2


def compact_cut(offset):
    return np.maximum(0, 1 - offset * offset) ** 3
