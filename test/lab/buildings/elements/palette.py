"""WHAT A BUILDING IS MADE OF, as colour -- by epoch, by region, and varied per body.

A street where every wall carries one colour reads as a diagram however good its geometry is, and
a street where the colours are random reads as a toy. Between those two is the truth: a period and
a place have a PALETTE, three or four values wide, and a street is a draw from it. Gruenderzeit
stucco is cream, ochre, pale grey-green or pale rose; interwar is Klinker brick; a Bavarian farm
is white render over a stone base whatever the century.

    WALL      the field, from the epoch first and the region's own wall material second
    TRIM      the bands -- sockel, gurt, kranz, sohlbank, sturz. Stone or a lighter stucco, and
              ALWAYS lighter than the field, because that is what makes relief read
    PLINTH    darker and greyer than either: it is the part that gets splashed
    JOINERY   the window frame and the door, which in every period before the 1960s is white or
              a dark green-brown, never the wall's colour

The seed is the body's own, so the same footprint always draws the same colour and a street built
twice is the same street -- determinism is compulsory here as everywhere.
"""

# EVERY VALUE HERE IS AN ALBEDO -- the fraction of light a surface actually returns -- and not a
# colour picked off a screen. That distinction is the whole reason a render looks like a render:
# a wall at 0.86 does not exist, and a street of them comes back paper-white under any exposure
# (rendered a street at eye level and looked at, 2026-09-06). The measured numbers:
#
#     fresh white render      0.70..0.80      aged render          0.50..0.60
#     cream / ochre stucco    0.40..0.55      red brick            0.15..0.25
#     limestone, sandstone    0.35..0.50      concrete             0.25..0.35
#     clay roof tile          0.15..0.25      slate                0.08..0.12
#     asphalt                 0.07..0.12      concrete paving      0.25..0.35
#     weathered copper        0.18..0.22      window glass, diffuse ~0.05
#
# A palette that ignores them is a palette that cannot be lit, however carefully the exposure is
# set -- the exposure can only move the whole picture, and the RATIOS between materials are what
# a viewer reads a material from.
FIELDS = {
    "gruenderzeit": ((0.533, 0.515, 0.459), (0.508, 0.459, 0.353), (0.471, 0.484, 0.440),
                     (0.527, 0.484, 0.465), (0.496, 0.490, 0.477)),
    "jugendstil":   ((0.539, 0.521, 0.471), (0.484, 0.496, 0.446), (0.515, 0.502, 0.490)),
    "baroque":      ((0.546, 0.521, 0.446), (0.521, 0.471, 0.341), (0.533, 0.508, 0.484)),
    "gothic":       ((0.446, 0.428, 0.378), (0.422, 0.403, 0.360)),
    "sacral":       ((0.484, 0.465, 0.422), (0.459, 0.440, 0.391)),
    "interwar":     ((0.291, 0.174, 0.136), (0.329, 0.205, 0.155), (0.254, 0.161, 0.136)),
    "postwar":      ((0.502, 0.496, 0.471), (0.515, 0.502, 0.434), (0.471, 0.471, 0.459)),
    "late20":       ((0.521, 0.521, 0.508), (0.496, 0.496, 0.484), (0.533, 0.527, 0.502)),
    "contemporary": ((0.539, 0.539, 0.533), (0.186, 0.192, 0.198), (0.484, 0.484, 0.477)),
    "industrial":   ((0.279, 0.167, 0.136), (0.322, 0.316, 0.304), (0.248, 0.149, 0.124)),
    "hall":         ((0.372, 0.384, 0.391), (0.446, 0.453, 0.446)),
    "commercial":   ((0.459, 0.453, 0.434), (0.384, 0.391, 0.397)),
    "tower":        ((0.211, 0.229, 0.248), (0.285, 0.304, 0.322)),
    "farm":         ((0.546, 0.533, 0.496), (0.527, 0.515, 0.465)),
    "siedlungshaus": ((0.539, 0.527, 0.490), (0.521, 0.496, 0.434), (0.508, 0.515, 0.490)),
    "bungalow":     ((0.533, 0.527, 0.508), (0.496, 0.490, 0.465)),
    "einfamilienhaus": ((0.546, 0.539, 0.515), (0.521, 0.508, 0.471), (0.490, 0.490, 0.477)),
}

# THE REGION'S OWN WALL, where the epoch has nothing to say -- a wall is made of what is to hand.
BY_WALL = {
    "stone":  ((0.465, 0.446, 0.397), (0.434, 0.422, 0.384), (0.490, 0.471, 0.422)),
    "brick":  ((0.341, 0.205, 0.161), (0.298, 0.180, 0.149), (0.378, 0.248, 0.192)),
    "timber": ((0.384, 0.260, 0.180), (0.484, 0.446, 0.384), (0.341, 0.217, 0.161)),
    "frame":  ((0.527, 0.527, 0.515), (0.490, 0.490, 0.484)),
}

JOINERY = ((0.62, 0.61, 0.58), (0.62, 0.61, 0.58), (0.09, 0.12, 0.09), (0.14, 0.10, 0.07))


def materials_of(epoch, wall_material, seed=0):
    """THE BODY'S MATERIALS, in glTF's own terms: {role: Material}.

    The COLOUR is the epoch's field and the MATERIAL is what the wall is made of -- a Klinker
    facade and a rendered one differ in roughness and in how the light leaves them, not only in
    hue, and the metallic-roughness model is where that difference lives."""
    import sys, pathlib as _pl
    sys.path.insert(0, str(_pl.Path(__file__).resolve().parents[3] / "lab"))
    import materials
    rgb = of(epoch, wall_material, seed)
    wall_stock = STOCK_OF_EPOCH.get(epoch) or STOCK_OF_WALL.get(wall_material, "render")
    out = {
        "wall": materials.STOCK[wall_stock].tinted(rgb["wall"]),
        "stone": materials.STOCK["limestone"].tinted(rgb["stone"]),
        "plinth": materials.STOCK["limestone"].tinted(rgb["plinth"]),
        "wood": materials.STOCK["joinery"].tinted(rgb["wood"]),
        "glass": materials.STOCK["glass"],
        "metal": materials.STOCK["zinc"],
    }
    for role, mat in out.items():
        mat.name = f"{epoch}.{role}"
    return out


def _pick(rows, seed):
    return rows[seed % len(rows)]


def _mix(a, b, u):
    return tuple(a[k] * (1.0 - u) + b[k] * u for k in range(3))


STOCK_OF_WALL = {"stone": "limestone", "brick": "brick", "timber": "timber",
                 "frame": "render"}
STOCK_OF_EPOCH = {"interwar": "brick", "industrial": "brick", "gothic": "limestone",
                  "sacral": "limestone", "baroque": "render", "gruenderzeit": "stucco",
                  "jugendstil": "stucco", "hall": "concrete", "tower": "concrete",
                  "commercial": "concrete", "contemporary": "render", "farm": "render",
                  "siedlungshaus": "render", "bungalow": "render", "einfamilienhaus": "render",
                  "postwar": "render", "late20": "render"}


def of(epoch, wall_material, seed=0):
    """The body's colours: {role: rgb}. `seed` is the body's own, so a street is deterministic."""
    field = _pick(FIELDS.get(epoch, FIELDS["late20"]), seed)
    if epoch not in FIELDS:
        field = _pick(BY_WALL.get(wall_material, BY_WALL["brick"]), seed)
    # THE TRIM IS ALWAYS LIGHTER THAN THE FIELD, because relief is read by the light on it and a
    # band the colour of its wall is a band nobody sees
    trim = _mix(field, (0.62, 0.61, 0.58), 0.62)
    plinth = _mix(field, (0.26, 0.26, 0.27), 0.55)
    return {
        "wall": field,
        "stone": trim,
        "plinth": plinth,
        "wood": _pick(JOINERY, seed // 3),
        "glass": (0.040, 0.052, 0.062),
        "metal": (0.21, 0.21, 0.22),
    }
