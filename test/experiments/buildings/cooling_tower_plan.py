"""Stable procedural cooling intake, support and rim plans on existing source profiles."""

import math
from dataclasses import dataclass

import numpy as np
from outshine_ref.buildings.modelling.radial_profile import RadialProfile


@dataclass(frozen=True, slots=True)
class Support:
    identifier: str
    start: tuple
    end: tuple
    width_m: float


@dataclass(frozen=True, slots=True)
class CoolingTower:
    source: RadialProfile
    shell: RadialProfile
    footing: RadialProfile
    collector: RadialProfile
    rim: RadialProfile
    supports: tuple
    bays: int


def plan(source):
    values = np.asarray(source.knots)
    bottom, top = values[[0, -1], 0]
    height, radius = top - bottom, values[0, 1]
    if not source.hollow or height < 30 or radius < 8:
        raise ValueError("Cooling intake requires a tall, broad, hollow source profile")
    intake = bottom + min(8.0, max(3.0, height * 0.055))
    cut = (intake, *[np.interp(intake, values[:, 0], values[:, j]) for j in (1, 2)])
    shell = RadialProfile(
        source.identifier + ":shell",
        source.centre_xy,
        (cut, *(k for k in source.knots if k[0] > intake)),
        source.materials,
    )
    footing = RadialProfile(
        source.identifier + ":footing",
        source.centre_xy,
        ((bottom, radius, radius - 1.2), (bottom + 0.18, radius, radius - 1.2)),
        source.materials,
    )
    if cut[1] + 0.6 > radius:
        raise ValueError("Cooling collector would escape the original inscribed disk")
    collector = RadialProfile(
        source.identifier + ":collector",
        source.centre_xy,
        (
            (intake - 0.5, cut[1] + 0.6, cut[1] - 1.0),
            (intake + 0.1, cut[1] + 0.6, cut[1] - 1.0),
        ),
        source.materials,
    )
    upper_radius = values[-1, 1]
    if upper_radius + 0.4 > radius:
        raise ValueError("Cooling rim would escape the original inscribed disk")
    rim = RadialProfile(
        source.identifier + ":rim",
        source.centre_xy,
        (
            (top - 0.8, upper_radius + 0.4, upper_radius - 0.8),
            (top, upper_radius + 0.4, upper_radius - 0.8),
        ),
        source.materials,
    )
    bays = min(96, max(16, math.ceil(2 * math.pi * radius / 6)))
    supports = []
    for bay in range(bays):
        centre_angle = 2 * math.pi * bay / bays
        for side in (-1, 1):
            low_angle = centre_angle + side * math.pi / (2 * bays)
            high_angle = centre_angle - side * math.pi / (2 * bays)
            low = np.asarray(source.centre_xy) + (radius - 0.7) * np.array(
                [math.cos(low_angle), math.sin(low_angle)]
            )
            high = np.asarray(source.centre_xy) + (cut[1] + cut[2]) * 0.5 * np.array(
                [math.cos(high_angle), math.sin(high_angle)]
            )
            supports.append(
                Support(
                    f"{source.identifier}:bay-{bay}:support-{side}",
                    (*low, bottom + 0.09),
                    (*high, intake - 0.2),
                    0.65,
                )
            )
    return CoolingTower(source, shell, footing, collector, rim, tuple(supports), bays)
