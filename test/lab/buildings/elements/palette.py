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

FIELDS = {
    "gruenderzeit": ((0.86, 0.83, 0.74), (0.82, 0.74, 0.57), (0.76, 0.78, 0.71),
                     (0.85, 0.78, 0.75), (0.80, 0.79, 0.77)),
    "jugendstil":   ((0.87, 0.84, 0.76), (0.78, 0.80, 0.72), (0.83, 0.81, 0.79)),
    "baroque":      ((0.88, 0.84, 0.72), (0.84, 0.76, 0.55), (0.86, 0.82, 0.78)),
    "gothic":       ((0.72, 0.69, 0.61), (0.68, 0.65, 0.58)),
    "sacral":       ((0.78, 0.75, 0.68), (0.74, 0.71, 0.63)),
    "interwar":     ((0.47, 0.28, 0.22), (0.53, 0.33, 0.25), (0.41, 0.26, 0.22)),
    "postwar":      ((0.81, 0.80, 0.76), (0.83, 0.81, 0.70), (0.76, 0.76, 0.74)),
    "late20":       ((0.84, 0.84, 0.82), (0.80, 0.80, 0.78), (0.86, 0.85, 0.81)),
    "contemporary": ((0.87, 0.87, 0.86), (0.30, 0.31, 0.32), (0.78, 0.78, 0.77)),
    "industrial":   ((0.45, 0.27, 0.22), (0.52, 0.51, 0.49), (0.40, 0.24, 0.20)),
    "hall":         ((0.60, 0.62, 0.63), (0.72, 0.73, 0.72)),
    "commercial":   ((0.74, 0.73, 0.70), (0.62, 0.63, 0.64)),
    "tower":        ((0.34, 0.37, 0.40), (0.46, 0.49, 0.52)),
    "farm":         ((0.88, 0.86, 0.80), (0.85, 0.83, 0.75)),
    "siedlungshaus": ((0.87, 0.85, 0.79), (0.84, 0.80, 0.70), (0.82, 0.83, 0.79)),
    "bungalow":     ((0.86, 0.85, 0.82), (0.80, 0.79, 0.75)),
    "einfamilienhaus": ((0.88, 0.87, 0.83), (0.84, 0.82, 0.76), (0.79, 0.79, 0.77)),
}

# THE REGION'S OWN WALL, where the epoch has nothing to say -- a wall is made of what is to hand.
BY_WALL = {
    "stone":  ((0.75, 0.72, 0.64), (0.70, 0.68, 0.62), (0.79, 0.76, 0.68)),
    "brick":  ((0.55, 0.33, 0.26), (0.48, 0.29, 0.24), (0.61, 0.40, 0.31)),
    "timber": ((0.62, 0.42, 0.29), (0.78, 0.72, 0.62), (0.55, 0.35, 0.26)),
    "frame":  ((0.85, 0.85, 0.83), (0.79, 0.79, 0.78)),
}

JOINERY = ((0.90, 0.89, 0.86), (0.90, 0.89, 0.86), (0.22, 0.26, 0.22), (0.34, 0.26, 0.19))


def _pick(rows, seed):
    return rows[seed % len(rows)]


def _mix(a, b, u):
    return tuple(a[k] * (1.0 - u) + b[k] * u for k in range(3))


def of(epoch, wall_material, seed=0):
    """The body's colours: {role: rgb}. `seed` is the body's own, so a street is deterministic."""
    field = _pick(FIELDS.get(epoch, FIELDS["late20"]), seed)
    if epoch not in FIELDS:
        field = _pick(BY_WALL.get(wall_material, BY_WALL["brick"]), seed)
    # THE TRIM IS ALWAYS LIGHTER THAN THE FIELD, because relief is read by the light on it and a
    # band the colour of its wall is a band nobody sees
    trim = _mix(field, (0.94, 0.93, 0.90), 0.62)
    plinth = _mix(field, (0.42, 0.42, 0.43), 0.55)
    return {
        "wall": field,
        "stone": trim,
        "plinth": plinth,
        "wood": _pick(JOINERY, seed // 3),
        "glass": (0.055, 0.075, 0.095),
        "metal": (0.38, 0.38, 0.40),
    }
