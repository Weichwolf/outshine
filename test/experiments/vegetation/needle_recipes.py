"""Three botanical morphology probes; incomplete plant recipes remain explicit."""

import numpy as np
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from needle_surface import Needle
from profile_curve import Profile

LABELS = (
    "Pinus spp. · Zweierbündel",
    "Pinus spp. · Fünferbündel",
    "Picea spp. · Einzelorgane",
)


def needle_graph(index, seed=418):
    if type(index) is not int or index not in (0, 1, 2):
        raise ValueError("declared needle morphology probe required")
    count = (2, 5, 1)[index]
    groups = 14 if index < 2 else 40
    needle = (
        Needle(),
        Needle(length=0.08, radius=0.00055, aspect=0.7, bend=0.4, twist=0.65),
        Needle(
            length=0.018,
            radius=0.0005,
            aspect=0.85,
            bend=0.18,
            twist=0.1,
            section="rhombus",
        ),
    )[index]
    twig = TubeShape(0.13, 0.1, Profile(np.array([0.0018, 0.0014, 0.001, 0.0006])))
    short = TubeShape(
        0.005 if index < 2 else 0.001,
        0,
        Profile(np.full(4, 0.0006 if index < 2 else 0.0003)),
    )
    nodes = (
        Generator(0, "root", "root", ()),
        Generator(1, "twig", "axis", (0,), twig, Placement(pitch=0, pitch_variance=0)),
        Generator(
            2,
            "short shoots",
            "axis",
            (1,),
            short,
            Placement(
                count=groups,
                first=0.1,
                last=0.87,
                pitch=1.1,
                pitch_variance=0.08,
                azimuth_variance=0.15,
                scale_variance=0.05,
            ),
        ),
        Generator(
            3,
            "needles",
            "needle",
            (2,),
            needle,
            Placement(
                count=count,
                first=1,
                last=1,
                pitch=0.16 if count > 1 else 0.03,
                pitch_variance=0.025,
                azimuth_step=2 * np.pi / count,
                azimuth_variance=0.04,
                scale_variance=0.08,
            ),
        ),
    )
    return GeneratorGraph(seed, nodes)
