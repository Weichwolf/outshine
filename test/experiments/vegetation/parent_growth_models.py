"""Controlled source recipes for parent-length and physical-needle comparisons."""

import numpy as np
from generator_specs import Generator, Placement, TubeShape
from outshine_ref.vegetation import DistancePattern, Needle, Profile


def control_nodes():
    fixed = {"scale_variance": 0.0, "pitch": 0.0, "pitch_variance": 0.0}
    return (
        Generator(0, "root", "root", ()),
        Generator(
            1,
            "three parent sizes",
            "axis",
            (0,),
            TubeShape(length=1.0, bend=0.0),
            Placement(
                count=3,
                first=0.0,
                last=1.0,
                scale=Profile(np.linspace(0.5, 1.5, 4)),
                **fixed,
            ),
        ),
        Generator(
            2,
            "relative shoots",
            "axis",
            (1,),
            TubeShape(
                length=0.4, bend=0.0, radius=Profile(np.linspace(0.0002, 0.0001, 4))
            ),
            Placement(
                pattern=DistancePattern(spacing=Profile(np.full(4, 0.2))),
                first=0.1,
                last=0.9,
                **fixed,
            ),
        ),
        Generator(
            3,
            "physical needles",
            "needle",
            (2,),
            Needle(length=0.02),
            Placement(
                pattern=DistancePattern(spacing=Profile(np.full(4, 0.05))),
                first=0.1,
                last=0.9,
                **fixed,
            ),
        ),
    )
