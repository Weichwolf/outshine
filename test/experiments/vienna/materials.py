GLASS_IOR = 1.52

# Linear reflectance, perceptual roughness.
SURFACES = {
    "bridge deck": ((0.23, 0.22, 0.20), 0.82),
    "bridge metal": ((0.085, 0.13, 0.115), 0.46),
    "oak bark": ((0.135, 0.098, 0.070), 0.91),
    "beech bark": ((0.158, 0.150, 0.131), 0.84),
    "foliage": ((0.095, 0.205, 0.044), 0.52),
    "regional soil": ((0.19, 0.16, 0.105), 0.95),
    "grass": ((0.097, 0.145, 0.043), 0.9),
    "grass blades": ((0.083, 0.165, 0.037), 0.86),
    "grass pale blades": ((0.147, 0.201, 0.058), 0.87),
    "regional wood": ((0.054, 0.139, 0.041), 0.89),
    "regional water": ((0.028, 0.084, 0.095), 0.09),
    "regional deck": ((0.21, 0.20, 0.18), 0.82),
    "regional roofs": ((0.27, 0.137, 0.09), 0.79),
    "regional walls": ((0.47, 0.39, 0.31), 0.86),
    "water": ((0.028, 0.084, 0.095), 0.09),
    "distant terrain": ((0.11, 0.16, 0.10), 0.95),
    "ground": ((0.105, 0.073, 0.046), 0.92),
    "unpaved path": ((0.17, 0.135, 0.095), 0.89),
    "road": ((0.075, 0.08, 0.082), 0.82),
    "sidewalk": ((0.27, 0.25, 0.22), 0.77),
    "paving": ((0.245, 0.237, 0.226), 0.74),
    "stair limestone": ((0.33, 0.30, 0.25), 0.78),
    "painted iron": ((0.055, 0.11, 0.081), 0.45),
    "rail head": ((0.51, 0.49, 0.45), 0.25),
    "roof": ((0.20, 0.10, 0.06), 0.77),
    "foundation": ((0.27, 0.26, 0.23), 0.85),
    "wall": ((0.48, 0.40, 0.30), 0.82),
    "frame": ((0.21, 0.18, 0.14), 0.65),
    "glass": ((0.018, 0.024, 0.028), 0.13),
}

METALLIC = {"rail head": 1.0}
DIFFUSE_TRANSMISSION = {"grass blades": 0.34, "grass pale blades": 0.34}
