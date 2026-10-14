"""Young Swiss/European conifers: measured organ ranges and plausible crowns."""

from dataclasses import dataclass, replace

from outshine_ref.vegetation import (
    AxisLevel,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
)


@dataclass(frozen=True)
class Conifer:
    key: str
    latin: str
    common: str
    sisf: int
    colour: tuple[float, float, float]
    needles: tuple[Needle, ...]
    patterns: tuple[NeedlePattern | None, ...]
    levels: tuple[AxisLevel, ...]

    def plant(self, seed=718):
        return NeedledPlant(
            PlantPlan(seed, self.levels, ()), self.patterns, self.needles
        )

    def reference(self):
        name = self.key.replace("-", "_")
        return {
            "latin": self.latin,
            "common": self.common,
            "sisf": self.sisf,
            "infoflora": f"https://www.infoflora.ch/de/flora/{self.key}.html",
            "atlas": f"https://forest.jrc.ec.europa.eu/media/atlas/{name.capitalize()}.pdf",
            "atlas_pdf": f"doc/references/vegetation/botany/2016-european-atlas-{self.key}.pdf",
            "checked": "2026-10-14",
            "scope": "young summer plant and attached needle shoots; no cones or mature/seasonal recipe",
            "crown_parameters": "own plausible interpretation, not measured allometry",
            "needle_material": "procedural appearance approximation; not calibrated needle-volume optics",
        }


def needle_variants(lengths, **shape):
    return tuple(Needle(length=length, **shape) for length in lengths)


def levels(
    height, branches, whorl, length, top, angle, angle_tip, twigs, twig_length, bend
):
    return (
        AxisLevel(1, height, 0.038, 0.002, bend=0.035, bend_variation=0.025),
        AxisLevel(
            branches,
            length,
            0.004,
            0.001,
            whorl_size=whorl,
            tip_scale=top,
            first=0.18,
            last=0.91,
            angle=angle,
            angle_tip=angle_tip,
            angle_variation=0.19,
            attachment_jitter=0.2,
            azimuth_variation=0.34,
            bend=bend,
            bend_variation=0.14,
        ),
        AxisLevel(
            twigs,
            twig_length,
            0.0007,
            0.0003,
            tip_scale=0.55,
            first=0.16,
            last=0.95,
            angle=1.1,
            angle_variation=0.26,
            attachment_jitter=0.23,
            azimuth_variation=0.48,
            upward_bias=0.04,
            bend=0.25,
            bend_variation=0.18,
        ),
    )


def needle_patterns():
    single = NeedlePattern(
        spacing=0.0012,
        group_size=1,
        pitch=1.3,
        angle_variation=0.44,
        short_length=0.0006,
        scale_variation=0.08,
    )
    pair = NeedlePattern(
        spacing=0.013,
        group_size=2,
        pitch=0.65,
        spread=0.13,
        angle_variation=0.22,
        short_length=0.0025,
        scale_variation=0.07,
    )
    five = NeedlePattern(
        spacing=0.011,
        group_size=5,
        pitch=0.57,
        spread=0.13,
        angle_variation=0.2,
        short_length=0.002,
        scale_variation=0.08,
    )
    cluster = NeedlePattern(
        spacing=0.034,
        group_size=30,
        pitch=1.08,
        spread=0.78,
        angle_variation=0.22,
        short_length=0.003,
        scale_variation=0.1,
    )
    return single, pair, five, cluster


def conifers():
    single, pair, five, cluster = needle_patterns()
    return (
        Conifer(
            "picea-abies",
            "Picea abies",
            "Fichte",
            302800,
            (0.035, 0.105, 0.042),
            needle_variants(
                (0.015, 0.018, 0.022),
                radius=0.00045,
                aspect=1,
                bend=0.13,
                twist=0.05,
                section="rhombus",
            ),
            (replace(single, first=0.94), replace(single, first=0.78), single),
            levels(3.4, 35, 5, 1.18, 0.17, 1.56, 0.7, 10, 0.25, 0.25),
        ),
        Conifer(
            "pinus-sylvestris",
            "Pinus sylvestris",
            "Wald-Föhre",
            306100,
            (0.025, 0.071, 0.055),
            needle_variants(
                (0.052, 0.060, 0.068),
                radius=0.00065,
                aspect=1,
                bend=0.34,
                twist=0.9,
                section="sector",
                sector_count=2,
            ),
            (replace(pair, first=0.95), replace(pair, first=0.84), pair),
            levels(3.2, 24, 4, 1.1, 0.24, 1.23, 0.69, 7, 0.25, 0.12),
        ),
        Conifer(
            "pinus-cembra",
            "Pinus cembra",
            "Arve",
            304900,
            (0.022, 0.074, 0.035),
            needle_variants(
                (0.055, 0.065, 0.077),
                radius=0.00065,
                aspect=1,
                bend=0.21,
                twist=0.15,
                section="sector",
                sector_count=5,
            ),
            (replace(five, first=0.95), replace(five, first=0.79), five),
            levels(3.05, 28, 4, 0.99, 0.22, 1.18, 0.65, 9, 0.26, 0.1),
        ),
        Conifer(
            "larix-decidua",
            "Larix decidua",
            "Europäische Lärche",
            227200,
            (0.07, 0.145, 0.025),
            needle_variants(
                (0.021, 0.027, 0.033),
                radius=0.00048,
                aspect=0.22,
                bend=0.22,
                twist=0.07,
                section="ellipse",
            ),
            (
                replace(
                    cluster,
                    group_size=1,
                    spacing=0.0035,
                    spread=0,
                    short_length=0,
                    first=0.94,
                ),
                replace(cluster, spacing=0.055, first=0.08),
                cluster,
            ),
            levels(3.45, 30, 5, 1.33, 0.18, 1.52, 0.8, 8, 0.24, 0.32),
        ),
    )
