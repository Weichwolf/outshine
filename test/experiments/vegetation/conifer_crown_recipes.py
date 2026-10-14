"""Third-order leafy shoots complete the young conifer crown interpretation."""

from dataclasses import replace

from europe_conifer_recipes import conifers
from outshine_ref.vegetation import AxisLevel


def crown_conifers():
    specifications = (
        (12, 0.32, 6, 0.12, 0.55, 1.20),
        (10, 0.28, 4, 0.14, 0.60, 0.75),
        (12, 0.30, 6, 0.13, 0.60, 0.80),
        (12, 0.28, 5, 0.13, 0.55, 1.25),
    )
    result = []
    for species, (twigs, length, tips, tip_length, taper, angle) in zip(
        conifers(), specifications, strict=True
    ):
        second = replace(species.levels[2], branches=twigs, length=length)
        terminal = AxisLevel(
            tips,
            tip_length,
            0.00024,
            0.00015,
            tip_scale=taper,
            first=0.16,
            last=0.95,
            angle=angle,
            angle_variation=0.28,
            attachment_jitter=0.27,
            azimuth_variation=0.55,
            upward_bias=0.06,
            bend=0.28,
            bend_variation=0.18,
        )
        pattern = species.patterns[-1]
        if species.key == "pinus-sylvestris":
            pattern = replace(pattern, spacing=0.004)
        elif species.key == "pinus-cembra":
            pattern = replace(pattern, spacing=0.006)
        result.append(
            replace(
                species,
                levels=(*species.levels[:2], second, terminal),
                patterns=(*species.patterns, pattern),
            )
        )
    return tuple(result)
