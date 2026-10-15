"""Authored woody size recipes for flat vegetation studies, not species allometry."""

from dataclasses import replace

from botanical_profiles import plant_recipe
from outshine_ref.vegetation import Blade, PlantPlan


def woody_plan(key, seed):
    plant = plant_recipe("fagus" if key == "beech" else "quercus", seed)
    a, b, c = plant.levels
    if key == "heather":
        levels = (
            replace(a, length=0.48, base_radius=0.009, tip_radius=0.003),
            replace(
                b,
                branches=7,
                length=0.26,
                base_radius=0.002,
                tip_radius=0.0008,
                first=0.06,
                leaf_internode=0.015,
                angle=0.9,
            ),
            replace(
                c,
                branches=4,
                length=0.16,
                base_radius=0.0005,
                tip_radius=0.00015,
                leaf_internode=0.009,
                arrangement="opposite",
            ),
        )
        blades = (
            Blade(length=0.012, width=0.003, lobes=0, lobe_depth=0, petiole=0.001),
        )
    else:
        levels = (
            replace(a, length=6.5, base_radius=0.14, tip_radius=0.045),
            replace(
                b,
                branches=12,
                length=2.5,
                base_radius=0.035,
                tip_radius=0.01,
                first=0.3,
                leaf_internode=0.025,
                length_shape="rounded",
            ),
            replace(
                c,
                branches=7,
                length=1.15,
                base_radius=0.007,
                tip_radius=0.0008,
                leaf_internode=0.012,
                length_shape="rounded",
            ),
        )
        blades = plant.blades
    return PlantPlan(seed, levels, blades)
