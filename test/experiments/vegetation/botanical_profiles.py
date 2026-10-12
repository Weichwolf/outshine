"""Broadleaf calibration fixtures; these are not the complete plant recipe catalog."""

from dataclasses import replace

from blade_surface import Blade
from plant_experiment import sapling
from plant_plan import PlantPlan

PROFILES = {
    "quercus": {
        "common": "Eiche",
        "genus": "Quercus",
        "species": "Quercus robur",
        "blade": {
            "length": 0.12,
            "width": 0.06,
            "widest": 0.62,
            "lobes": 6,
            "lobe_depth": 0.35,
            "petiole": 0.004,
        },
        "axis": {"length": 0.85, "angle": 1.15, "bend": 0.38, "leaf_internode": None},
        "twig": {"length": 0.42, "angle": 0.95, "bend": 0.42, "leaf_internode": 0.03},
        "description": "Breite gelappte Blätter und ausladende junge Äste.",
    },
    "fagus": {
        "common": "Buche",
        "genus": "Fagus",
        "species": "Fagus sylvatica",
        "blade": {
            "length": 0.085,
            "width": 0.05,
            "widest": 0.47,
            "lobes": 0,
            "lobe_depth": 0,
            "petiole": 0.008,
        },
        "axis": {"length": 0.78, "angle": 0.9, "bend": 0.3, "leaf_internode": None},
        "twig": {"length": 0.38, "angle": 0.85, "bend": 0.32, "leaf_internode": 0.028},
        "description": "Ovale ungeteilte Blätter und steiler gestellte junge Äste.",
    },
    "betula": {
        "common": "Birke",
        "genus": "Betula",
        "species": "Betula pendula",
        "blade": {
            "length": 0.06,
            "width": 0.042,
            "widest": 0.27,
            "lobes": 20,
            "lobe_depth": 0.08,
            "petiole": 0.014,
        },
        "axis": {"length": 0.72, "angle": 0.85, "bend": 0.5, "leaf_internode": None},
        "twig": {"length": 0.46, "angle": 1.0, "bend": 0.8, "leaf_internode": 0.035},
        "description": "Kleinere zugespitzte Blätter mit feinem Randprofil und gebogenen Zweigen.",
    },
}


def plant_recipe(name, seed=17):
    profile = PROFILES[name]
    base = sapling(seed)
    levels = list(base.levels)
    levels[1] = replace(levels[1], arrangement="distichous", **profile["axis"])
    levels[2] = replace(
        levels[2], arrangement="distichous", leaf_sky_alignment=0.3, **profile["twig"]
    )
    blades = tuple(
        replace(
            Blade(**profile["blade"]),
            length=profile["blade"]["length"] * (0.85 + 0.15 * variant / 7),
            width=profile["blade"]["width"] * (0.88 + 0.12 * ((variant * 3) % 8) / 7),
            fold=0.045 + 0.085 * variant / 7,
            bend=0.18 + 0.27 * variant / 7,
            twist=(variant - 3.5) * 0.08,
        )
        for variant in range(8)
    )
    return PlantPlan(seed, tuple(levels), blades)
