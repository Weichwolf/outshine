"""Serializable physical material parameters for the bark-function product."""


def bark_recipe():
    return {
        "version": "solid-bark-preview-1",
        "noise_scale_per_m": [850, 850, 32],
        "noise_detail": 3,
        "noise_roughness": 0.65,
        "colour_positions": [0.27, 0.73],
        "colour_low": [0.065, 0.042, 0.019, 1],
        "colour_high": [0.20, 0.14, 0.061, 1],
        "young_colour": [0.13, 0.12, 0.035, 1],
        "radius_range_m": [0.0015, 0.007],
        "roughness": 0.8,
        "bump_strength": 0.30,
        "bump_distance_m": 0.00035,
    }
