"""Authored spatial function curves, not botanical species recipes."""

import numpy as np
from bend_pattern import BendPattern
from blade_surface import Blade
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from placement_patterns import DistancePattern
from profile_curve import Profile
from spatial_axis import SpatialAxis
from spine_curve import CubicSpine


def spatial_axis(kind="space", intervals=16):
    t = np.linspace(0, 1, 9)
    if kind == "straight":
        x = z = dx = dz = np.zeros_like(t)
    elif kind == "inflection":
        x = 0.25 * (t**3 - t**2)
        dx = 0.25 * (3 * t**2 - 2 * t)
        z = 0.1 * np.sin(np.pi * t) ** 2
        dz = 0.1 * np.pi * np.sin(2 * np.pi * t)
    else:
        x = 0.18 * np.sin(2 * np.pi * t) ** 2
        dx = 0.18 * 4 * np.pi * np.sin(2 * np.pi * t) * np.cos(2 * np.pi * t)
        z = 0.12 * (1 - np.cos(2 * np.pi * t))
        dz = 0.24 * np.pi * np.sin(2 * np.pi * t)
    p = np.column_stack((x, 1.5 * t, z))
    d = np.column_stack((dx, np.full_like(t, 1.5), dz))
    return SpatialAxis(
        CubicSpine.from_hermite(p, d),
        Profile(np.linspace(0.015, 0.003, 4)),
        Profile(np.linspace(0, 0.3 if kind == "roll" else 0, 4)),
        (1.1, 0.9),
        intervals,
    )


def spatial_graph(seed=17):
    parent = spatial_axis()
    return GeneratorGraph(
        seed,
        (
            Generator(0, "Wurzel", "root", ()),
            Generator(
                1,
                "Räumlicher Stamm",
                "axis",
                (0,),
                parent,
                Placement(
                    pitch=0, pitch_variance=0, scale_variance=0, azimuth_variance=0
                ),
            ),
            Generator(
                2,
                "Biegeverzweigungen",
                "axis",
                (1,),
                TubeShape(0.43, 0.25, Profile(np.linspace(0.005, 0.0015, 4))),
                Placement(
                    first=0.12,
                    last=0.91,
                    pitch=1.0,
                    pattern=BendPattern(
                        minimum_angle=0.13,
                        window_m=0.20,
                        minimum_spacing_m=0.19,
                        limit=12,
                    ),
                ),
            ),
            Generator(
                3,
                "Blätter",
                "blade",
                (1, 2),
                Blade(length=0.13, width=0.045, bend=0.45, fold=0.12, lobes=0),
                Placement(
                    first=0.10,
                    last=0.92,
                    pitch=1.15,
                    pattern=DistancePattern(
                        spacing=Profile(np.full(4, 0.065)), divergence=np.pi
                    ),
                ),
            ),
        ),
    )
