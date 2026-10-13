"""Mixed-organ function graph; botanical species recipes remain a later stage."""

import numpy as np
from blade_surface import Blade
from frond_pattern import PinnatePattern
from frond_surface import FrondGuide
from generator_graph import GeneratorGraph
from generator_specs import FrondShape, Generator, Placement, TubeShape
from profile_curve import Profile


def mixed_graph(seed=17):
    t = np.linspace(0, 1, 1000)
    align = Placement(pitch=0, pitch_variance=0, scale_variance=0)
    stem = TubeShape(1.25, 0.08, Profile(np.linspace(0.023, 0.008, 4)))
    twigs = TubeShape(0.55, 0.35, Profile(np.linspace(0.0055, 0.0018, 4)))
    blade = Blade(length=0.12, width=0.055, lobes=5, bend=0.25, fold=0.14)
    frond = FrondShape(
        PinnatePattern(length=0.50, pairs=10, leaflet_length=0.12),
        FrondGuide(length=0.50, bend=0.65, fold_per_m=1.0, roll=Profile(0.2 * t)),
    )
    return GeneratorGraph(
        seed,
        (
            Generator(0, "Wurzel", "root", ()),
            Generator(1, "Stamm", "axis", (0,), stem, align),
            Generator(
                2,
                "Äste",
                "axis",
                (1,),
                twigs,
                Placement(
                    count=6,
                    first=0.28,
                    last=0.95,
                    pitch=1.1,
                    scale=Profile(np.array((0.8, 1.05, 0.9, 0.55))),
                ),
            ),
            Generator(
                3,
                "Blätter",
                "blade",
                (2,),
                blade,
                Placement(
                    count=8,
                    first=0.12,
                    last=0.95,
                    pitch=1.15,
                    pitch_variance=0.18,
                    scale=Profile(np.array((1.0, 1.0, 0.95, 0.7))),
                    scale_variance=0.08,
                ),
            ),
            Generator(
                4,
                "Fronds",
                "frond",
                (1, 2),
                frond,
                Placement(
                    first=0.25,
                    last=0.45,
                    pitch=1.35,
                    pitch_variance=0.07,
                    scale=Profile(np.full(4, 0.7)),
                ),
            ),
        ),
    )
