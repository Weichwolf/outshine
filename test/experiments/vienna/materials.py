from outshine_ref.buildings.modelling.material_schema import colour, srgb_to_linear

GLASS_IOR = 1.52
WATER_IOR = 1.333
WATER_MATERIALS = ("water", "regional water")

# Linear reflectance, perceptual roughness.
SURFACES = {
    "bridge deck": ((0.23, 0.22, 0.20), 0.82),
    "retaining wall": ((0.285, 0.247, 0.186), 0.87),
    "bridge metal": ((0.085, 0.13, 0.115), 0.46),
    "oak bark": ((0.135, 0.098, 0.070), 0.91),
    "beech bark": ((0.158, 0.150, 0.131), 0.84),
    "foliage": ((0.095, 0.205, 0.044), 0.52),
    "regional soil": ((0.19, 0.16, 0.105), 0.95),
    "grass": ((0.097, 0.145, 0.043), 0.9),
    "grass blades": ((0.083, 0.165, 0.037), 0.86),
    "grass pale blades": ((0.147, 0.201, 0.058), 0.87),
    "meadow": ((0.090, 0.135, 0.034), 0.93),
    "meadow blades": ((0.108, 0.194, 0.050), 0.86),
    "meadow stems": ((0.151, 0.216, 0.065), 0.88),
    "meadow seeds": ((0.337, 0.309, 0.126), 0.87),
    "meadow cream flowers": ((0.68, 0.63, 0.40), 0.70),
    "meadow purple flowers": ((0.34, 0.084, 0.31), 0.69),
    "woodland ferns": ((0.085, 0.195, 0.035), 0.84),
    "woodland grasses": ((0.127, 0.174, 0.043), 0.89),
    "woodland low leaves": ((0.073, 0.146, 0.030), 0.73),
    "bank sedge": ((0.094, 0.195, 0.045), 0.84),
    "reed culms": ((0.164, 0.211, 0.068), 0.85),
    "reed leaves": ((0.097, 0.205, 0.046), 0.78),
    "reed seed heads": ((0.217, 0.125, 0.087), 0.89),
    "shrub wood": ((0.10, 0.073, 0.047), 0.83),
    "shrub leaves": ((0.067, 0.177, 0.037), 0.62),
    "sapling leaves": ((0.102, 0.199, 0.043), 0.58),
    "regional wood": ((0.054, 0.139, 0.041), 0.89),
    "regional water": ((0.028, 0.084, 0.095), 0.09),
    "regional deck": ((0.21, 0.20, 0.18), 0.82),
    "regional roofs": ((0.27, 0.137, 0.09), 0.79),
    "regional walls": ((0.47, 0.39, 0.31), 0.86),
    "water": ((0.028, 0.084, 0.095), 0.09),
    "weir overflow": ((0.18, 0.225, 0.195), 0.37),
    "distant terrain": ((0.11, 0.16, 0.10), 0.95),
    "ground": ((0.105, 0.073, 0.046), 0.92),
    "forest floor": ((0.098, 0.073, 0.047), 0.91),
    "unpaved path": ((0.17, 0.135, 0.095), 0.89),
    "road": ((0.075, 0.08, 0.082), 0.82),
    "sidewalk": ((0.27, 0.25, 0.22), 0.77),
    "paving": ((0.245, 0.237, 0.226), 0.74),
    "stair limestone": ((0.33, 0.30, 0.25), 0.78),
    "painted iron": ((0.055, 0.11, 0.081), 0.45),
    "rail head": ((0.51, 0.49, 0.45), 0.25),
    "roof": ((0.20, 0.10, 0.06), 0.77),
    "roof tile": ((0.235, 0.089, 0.052), 0.82),
    "foundation": ((0.27, 0.26, 0.23), 0.85),
    "portal stone": ((0.40, 0.36, 0.30), 0.76),
    "window sill": ((0.39, 0.35, 0.28), 0.77),
    "wall": ((0.48, 0.40, 0.30), 0.82),
    "street base": ((0.35, 0.30, 0.24), 0.87),
    "street trim": ((0.34, 0.30, 0.25), 0.78),
    "frame": ((0.21, 0.18, 0.14), 0.65),
    "glass": ((0.018, 0.024, 0.028), 0.13),
    "entrance door": ((0.072, 0.048, 0.030), 0.54),
}

METALLIC = {"rail head": 1.0}
DIFFUSE_TRANSMISSION = {
    "foliage": 0.32,
    "grass blades": 0.34,
    "grass pale blades": 0.34,
    "meadow blades": 0.42,
    "woodland ferns": 0.36,
    "woodland grasses": 0.34,
    "woodland low leaves": 0.31,
    "bank sedge": 0.39,
    "reed leaves": 0.42,
    "shrub leaves": 0.29,
    "sapling leaves": 0.35,
}


def surface_properties(name):
    base, separator, encoded = name.partition(" #")
    rgb, roughness = SURFACES[base]
    if separator:
        rgb = tuple(srgb_to_linear(colour(("#" + encoded,))))
        if base == "street base":
            rgb = tuple(0.72 * channel + 0.025 for channel in rgb)
    return base, rgb, roughness
